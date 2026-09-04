#include "app/AgvCanFdTest.h"

#include "app/AppContext.h"
#include "core/DebugCore.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QSet>
#include <QTextStream>
#include <QTimer>

#include <limits>

namespace {

constexpr quint32 IdEmergencyStop = 0x080;
constexpr quint32 IdControl = 0x091;
constexpr quint32 IdHeartbeat = 0x101;
constexpr quint32 IdGotoLocal = 0x121;
constexpr quint32 IdCommandAck = 0x181;
constexpr quint32 IdDriveStatus = 0x201;
constexpr quint32 IdOdomStatus = 0x211;
constexpr quint32 IdMotorStatus = 0x221;
constexpr quint32 IdSafetyStatus = 0x231;

constexpr quint8 AckReceived = 0;
constexpr quint8 AckAccepted = 1;
constexpr quint8 AckCompleted = 2;
constexpr quint8 AckCanceled = 3;
constexpr quint8 AckRejected = 5;
constexpr quint8 AckFault = 6;

constexpr quint16 InitialStopSequence = 90;
constexpr quint16 ClearFaultSequence = 100;
constexpr quint16 GotoSequence = 101;
constexpr quint16 FinalStopSequence = 102;
constexpr quint16 EmergencyStopSequence = 103;

quint8 byteAt(const QByteArray& data, int offset)
{
    return static_cast<quint8>(data.at(offset));
}

quint16 readLe16(const QByteArray& data, int offset)
{
    return static_cast<quint16>(byteAt(data, offset))
        | (static_cast<quint16>(byteAt(data, offset + 1)) << 8);
}

quint32 readLe32(const QByteArray& data, int offset)
{
    return static_cast<quint32>(byteAt(data, offset))
        | (static_cast<quint32>(byteAt(data, offset + 1)) << 8)
        | (static_cast<quint32>(byteAt(data, offset + 2)) << 16)
        | (static_cast<quint32>(byteAt(data, offset + 3)) << 24);
}

void putLe16(QByteArray* data, int offset, quint16 value)
{
    (*data)[offset] = static_cast<char>(value & 0xff);
    (*data)[offset + 1] = static_cast<char>((value >> 8) & 0xff);
}

void putLe32(QByteArray* data, int offset, quint32 value)
{
    (*data)[offset] = static_cast<char>(value & 0xff);
    (*data)[offset + 1] = static_cast<char>((value >> 8) & 0xff);
    (*data)[offset + 2] = static_cast<char>((value >> 16) & 0xff);
    (*data)[offset + 3] = static_cast<char>((value >> 24) & 0xff);
}

quint16 crc16Ccitt(const QByteArray& data, int length)
{
    quint16 crc = 0xffff;
    for (int index = 0; index < length; ++index) {
        crc ^= static_cast<quint16>(byteAt(data, index)) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) != 0
                ? static_cast<quint16>((crc << 1) ^ 0x1021)
                : static_cast<quint16>(crc << 1);
        }
    }
    return crc;
}

QByteArray withCrc(QByteArray payload)
{
    const quint16 crc = crc16Ccitt(payload, payload.size() - 2);
    putLe16(&payload, payload.size() - 2, crc);
    return payload;
}

bool hasValidCrc(const QByteArray& payload)
{
    return payload.size() >= 2
        && crc16Ccitt(payload, payload.size() - 2)
            == readLe16(payload, payload.size() - 2);
}

QByteArray makeProjectFrame(quint32 canId, const QByteArray& payload)
{
    QByteArray frame;
    frame.reserve(7 + payload.size());
    frame.append(static_cast<char>(0xca));
    frame.append(static_cast<char>(0xfd));
    frame.append(static_cast<char>((canId >> 24) & 0xff));
    frame.append(static_cast<char>((canId >> 16) & 0xff));
    frame.append(static_cast<char>((canId >> 8) & 0xff));
    frame.append(static_cast<char>(canId & 0xff));
    frame.append(static_cast<char>(payload.size()));
    frame.append(payload);
    return frame;
}

QByteArray makeHeartbeat(quint16 sequence, quint32 session, quint32 uptimeMs)
{
    QByteArray payload(16, '\0');
    payload[0] = 1;
    putLe16(&payload, 2, sequence);
    putLe32(&payload, 4, session);
    putLe32(&payload, 8, uptimeMs);
    return makeProjectFrame(IdHeartbeat, withCrc(payload));
}

QByteArray makeControl(quint8 opcode, quint16 sequence, quint32 session)
{
    QByteArray payload(16, '\0');
    payload[0] = 1;
    payload[1] = static_cast<char>(opcode);
    putLe16(&payload, 2, sequence);
    putLe32(&payload, 4, session);
    return makeProjectFrame(IdControl, withCrc(payload));
}

QByteArray makeEmergencyStop(quint16 sequence, quint32 session, quint32 uptimeMs)
{
    QByteArray payload(16, '\0');
    payload[0] = 1;
    payload[1] = 1;
    putLe16(&payload, 2, sequence);
    putLe32(&payload, 4, session);
    putLe32(&payload, 8, uptimeMs);
    return makeProjectFrame(IdEmergencyStop, withCrc(payload));
}

QByteArray makeGoto300(quint16 sequence, quint32 session)
{
    QByteArray payload(32, '\0');
    payload[0] = 1;
    putLe16(&payload, 2, sequence);
    putLe32(&payload, 4, session);
    putLe32(&payload, 8, 300);
    putLe16(&payload, 20, 50);
    return makeProjectFrame(IdGotoLocal, withCrc(payload));
}

class AgvCanFdTestRunner {
public:
    AgvCanFdTestRunner(QApplication& app, AppContext& context, bool move300mm)
        : m_app(app)
        , m_move300mm(move300mm)
        , m_context(context)
        , m_core(context.debugCore())
        , m_out(stdout)
        , m_err(stderr)
    {
    }

    int run()
    {
        m_context.scanPlugins();

        QObject::connect(m_core, &DebugCore::errorOccurred, &m_app,
                         [this](const QString& message) {
                             m_err << "ERROR,PLUGIN," << message << '\n';
                             m_err.flush();
                         });
        QObject::connect(m_core, &DebugCore::framePublished, &m_app,
                         [this](const DataFrame& frame) { handleFrame(frame); });

        if (!m_core->pluginManager()->activateProtocol(QStringLiteral("CAN Frame"))) {
            m_err << "FAIL,CAN_FRAME_PROTOCOL_OPEN\n";
            return 2;
        }

        IPhysicalPlugin* physical = nullptr;
        for (IPhysicalPlugin* candidate : m_core->pluginManager()->physicalPlugins()) {
            if (candidate->name() == QStringLiteral("CandleLight CANFD")) {
                physical = candidate;
                break;
            }
        }
        if (!physical
            || !m_core->pluginManager()->activatePhysical(
                physical->name(), physical->defaultConfig())) {
            m_err << "FAIL,CANDLELIGHT_OPEN\n";
            m_core->pluginManager()->deactivateAll();
            return 2;
        }

        m_clock.start();
        m_session = QRandomGenerator::global()->generate();
        if (m_session == 0) {
            m_session = 1;
        }

        m_heartbeat.setInterval(100);
        QObject::connect(&m_heartbeat, &QTimer::timeout, &m_app,
                         [this]() { sendHeartbeat(); });
        m_watchdog.setInterval(50);
        QObject::connect(&m_watchdog, &QTimer::timeout, &m_app,
                         [this]() { checkDeadline(); });

        m_stage = Stage::WaitingSafety;
        setDeadline(3000);
        sendHeartbeat();
        m_heartbeat.start();
        m_watchdog.start();
        m_out << "TEST,START,mode=" << (m_move300mm ? "MOVE_300_MM" : "LINK_ONLY")
              << ",session=0x"
              << QString::number(m_session, 16).rightJustified(8, QLatin1Char('0')).toUpper()
              << ",nominal=1000000,data=4000000,brs=1\n";
        m_out.flush();
        return m_app.exec();
    }

private:
    enum class Stage {
        WaitingSafety,
        WaitingInitialStopAck,
        WaitingClearAck,
        WaitingFaultClear,
        WaitingGotoAck,
        WaitingCompletion,
        WaitingFinalOdom,
        WaitingFinalStopAck,
        WaitingTelemetry,
        Finished
    };

    void sendFrame(const QByteArray& bytes)
    {
        QVariantMap command;
        command.insert(QStringLiteral("bytes"), bytes);
        command.insert(QStringLiteral("hex"),
                       QString::fromLatin1(bytes.toHex(' ').toUpper()));
        m_core->sendCommand(command);
    }

    void sendHeartbeat()
    {
        sendFrame(makeHeartbeat(m_heartbeatSequence++, m_session,
                                static_cast<quint32>(m_clock.elapsed())));
        if (m_heartbeatSequence == 0) {
            m_heartbeatSequence = 1;
        }
    }

    void setDeadline(qint64 durationMs)
    {
        m_deadlineMs = m_clock.elapsed() + durationMs;
    }

    void checkDeadline()
    {
        if (m_stage == Stage::Finished || m_clock.elapsed() <= m_deadlineMs) {
            return;
        }
        QString detail = QStringLiteral("stage=%1").arg(stageName());
        if (m_stage == Stage::WaitingFinalOdom) {
            detail += QStringLiteral(",last_x_mm=%1").arg(m_latestOdomX);
        }
        fail(QStringLiteral("TIMEOUT,%1").arg(detail));
    }

    QString stageName() const
    {
        switch (m_stage) {
        case Stage::WaitingSafety: return QStringLiteral("WAIT_SAFETY");
        case Stage::WaitingInitialStopAck: return QStringLiteral("WAIT_STOP_ACK");
        case Stage::WaitingClearAck: return QStringLiteral("WAIT_CLEAR_ACK");
        case Stage::WaitingFaultClear: return QStringLiteral("WAIT_FAULT_CLEAR");
        case Stage::WaitingGotoAck: return QStringLiteral("WAIT_GOTO_ACK");
        case Stage::WaitingCompletion: return QStringLiteral("WAIT_COMPLETION");
        case Stage::WaitingFinalOdom: return QStringLiteral("WAIT_FINAL_ODOM");
        case Stage::WaitingFinalStopAck: return QStringLiteral("WAIT_FINAL_STOP_ACK");
        case Stage::WaitingTelemetry: return QStringLiteral("WAIT_TELEMETRY");
        case Stage::Finished: return QStringLiteral("FINISHED");
        }
        return QStringLiteral("UNKNOWN");
    }

    void handleFrame(const DataFrame& frame)
    {
        if (frame.direction != FrameDirection::Receive) {
            return;
        }
        const quint32 canId = frame.attributes.value(QStringLiteral("can_id")).toUInt();
        const QByteArray& payload = frame.rawPayload;
        ++m_rxCount;
        if (!hasValidCrc(payload)) {
            ++m_badCrcCount;
            return;
        }

        if (!m_seenIds.contains(canId)) {
            m_seenIds.insert(canId);
            m_out << "RX,FIRST,id=0x"
                  << QString::number(canId, 16).rightJustified(3, QLatin1Char('0')).toUpper()
                  << ",len=" << payload.size() << ",crc=OK\n";
            m_out.flush();
        }

        if (canId == IdCommandAck && payload.size() == 16) {
            handleAck(payload);
        } else if (canId == IdSafetyStatus && payload.size() == 24) {
            handleSafety(payload);
        } else if (canId == IdOdomStatus && payload.size() == 32) {
            handleOdom(payload);
        }

        if (m_stage == Stage::WaitingTelemetry && hasAllTelemetry()) {
            pass();
        }
    }

    void handleSafety(const QByteArray& payload)
    {
        m_activeBumper = byteAt(payload, 9);
        m_latchedBumper = byteAt(payload, 10);
        m_latestFault = readLe16(payload, 12);
        const quint8 busState = byteAt(payload, 18);

        const quint32 safetyKey = static_cast<quint32>(m_latestFault)
            | (static_cast<quint32>(m_activeBumper) << 16)
            | (static_cast<quint32>(m_latchedBumper) << 24);
        if (!m_haveSafetyKey || safetyKey != m_lastSafetyKey) {
            m_haveSafetyKey = true;
            m_lastSafetyKey = safetyKey;
            m_out << "RX,SAFETY,fault=0x"
                  << QString::number(m_latestFault, 16).rightJustified(4, QLatin1Char('0')).toUpper()
                  << ",active_bumper=0x"
                  << QString::number(m_activeBumper, 16).rightJustified(2, QLatin1Char('0')).toUpper()
                  << ",latched_bumper=0x"
                  << QString::number(m_latchedBumper, 16).rightJustified(2, QLatin1Char('0')).toUpper()
                  << ",bus=0x"
                  << QString::number(busState, 16).rightJustified(2, QLatin1Char('0')).toUpper()
                  << '\n';
            m_out.flush();
        }

        if (m_stage == Stage::WaitingSafety) {
            m_stage = Stage::WaitingInitialStopAck;
            setDeadline(2000);
            sendFrame(makeControl(1, InitialStopSequence, m_session));
            m_out << "TX,SAFETY_STOP,sequence=" << InitialStopSequence << '\n';
            m_out.flush();
            return;
        }

        if (m_stage == Stage::WaitingFaultClear && m_latestFault == 0) {
            sendGoto();
            return;
        }

        if (m_motionAccepted && m_latestFault != 0
            && (m_stage == Stage::WaitingCompletion
                || m_stage == Stage::WaitingFinalOdom)) {
            fail(QStringLiteral("LOWER_FAULT,fault=0x%1")
                     .arg(m_latestFault, 4, 16, QLatin1Char('0')));
        }
    }

    void handleAck(const QByteArray& payload)
    {
        const quint8 status = byteAt(payload, 1);
        const quint16 sequence = readLe16(payload, 2);
        const quint8 command = byteAt(payload, 8);
        const quint8 error = byteAt(payload, 9);
        const quint16 fault = readLe16(payload, 10);
        m_out << "RX,ACK,sequence=" << sequence << ",status=" << status
              << ",command=" << command << ",error=" << error
              << ",fault=0x"
              << QString::number(fault, 16).rightJustified(4, QLatin1Char('0')).toUpper()
              << '\n';
        m_out.flush();

        if (status == AckReceived) {
            return;
        }
        if (status == AckRejected || status == AckFault || status == AckCanceled) {
            fail(QStringLiteral("COMMAND_REJECTED,sequence=%1,status=%2,error=%3,fault=0x%4")
                     .arg(sequence).arg(status).arg(error)
                     .arg(fault, 4, 16, QLatin1Char('0')));
            return;
        }

        if (m_stage == Stage::WaitingInitialStopAck
            && sequence == InitialStopSequence && status == AckAccepted) {
            if (m_activeBumper != 0 || m_latchedBumper != 0) {
                fail(QStringLiteral("BUMPER_NOT_CLEAR,active=0x%1,latched=0x%2")
                         .arg(m_activeBumper, 2, 16, QLatin1Char('0'))
                         .arg(m_latchedBumper, 2, 16, QLatin1Char('0')));
                return;
            }
            if (!m_move300mm) {
                waitForTelemetryOrPass();
            } else if (m_latestFault != 0) {
                m_stage = Stage::WaitingClearAck;
                setDeadline(2000);
                sendFrame(makeControl(2, ClearFaultSequence, m_session));
                m_out << "TX,CLEAR_FAULT,sequence=" << ClearFaultSequence << '\n';
                m_out.flush();
            } else {
                sendGoto();
            }
            return;
        }

        if (m_stage == Stage::WaitingClearAck
            && sequence == ClearFaultSequence && status == AckAccepted) {
            m_stage = Stage::WaitingFaultClear;
            setDeadline(2000);
            if (m_latestFault == 0) {
                sendGoto();
            }
            return;
        }

        if (m_stage == Stage::WaitingGotoAck && sequence == GotoSequence
            && status == AckAccepted) {
            m_motionAccepted = true;
            m_stage = Stage::WaitingCompletion;
            setDeadline(30000);
            m_out << "TEST,MOVE_ACCEPTED,distance_mm=300,speed_mm_s=50\n";
            m_out.flush();
            return;
        }

        if (m_stage == Stage::WaitingCompletion && sequence == GotoSequence
            && status == AckCompleted) {
            m_motionCompleted = true;
            if (odomInRange()) {
                sendFinalStop();
            } else {
                m_stage = Stage::WaitingFinalOdom;
                setDeadline(2000);
            }
            return;
        }

        if (m_stage == Stage::WaitingFinalStopAck
            && sequence == FinalStopSequence && status == AckAccepted) {
            waitForTelemetryOrPass();
        }
    }

    void handleOdom(const QByteArray& payload)
    {
        m_latestOdomX = static_cast<qint32>(readLe32(payload, 8));
        if (m_stage == Stage::WaitingCompletion
            || m_stage == Stage::WaitingFinalOdom) {
            m_out << "RX,ODOM,x_mm=" << m_latestOdomX << '\n';
            m_out.flush();
        }
        if (m_stage == Stage::WaitingFinalOdom && odomInRange()) {
            sendFinalStop();
        }
    }

    bool odomInRange() const
    {
        return m_latestOdomX >= 250 && m_latestOdomX <= 350;
    }

    void sendGoto()
    {
        m_stage = Stage::WaitingGotoAck;
        setDeadline(2000);
        sendFrame(makeGoto300(GotoSequence, m_session));
        m_out << "TX,GOTO_LOCAL,sequence=" << GotoSequence
              << ",x_mm=300,y_mm=0,yaw_cdeg=0,speed_mm_s=50\n";
        m_out.flush();
    }

    void sendFinalStop()
    {
        m_stage = Stage::WaitingFinalStopAck;
        setDeadline(2000);
        sendFrame(makeControl(1, FinalStopSequence, m_session));
        m_out << "TX,FINAL_STOP,sequence=" << FinalStopSequence << '\n';
        m_out.flush();
    }

    bool hasAllTelemetry() const
    {
        return m_seenIds.contains(IdDriveStatus)
            && m_seenIds.contains(IdOdomStatus)
            && m_seenIds.contains(IdMotorStatus)
            && m_seenIds.contains(IdSafetyStatus);
    }

    void waitForTelemetryOrPass()
    {
        if (hasAllTelemetry()) {
            pass();
            return;
        }
        m_stage = Stage::WaitingTelemetry;
        setDeadline(2000);
    }

    void pass()
    {
        QString detail = QStringLiteral("rx=%1,bad_crc=%2,telemetry=201/211/221/231")
                             .arg(m_rxCount).arg(m_badCrcCount);
        if (m_move300mm) {
            detail += QStringLiteral(",completed=%1,odom_x_mm=%2")
                          .arg(m_motionCompleted ? 1 : 0).arg(m_latestOdomX);
        }
        finish(true, detail);
    }

    void fail(const QString& reason)
    {
        if (m_stage == Stage::Finished) {
            return;
        }
        if (m_motionAccepted && !m_motionCompleted) {
            sendFrame(makeEmergencyStop(EmergencyStopSequence, m_session,
                                        static_cast<quint32>(m_clock.elapsed())));
        } else {
            sendFrame(makeControl(1, FinalStopSequence, m_session));
        }
        finish(false, reason);
    }

    void finish(bool success, const QString& detail)
    {
        if (m_stage == Stage::Finished) {
            return;
        }
        m_stage = Stage::Finished;
        m_heartbeat.stop();
        m_watchdog.stop();
        m_out << (success ? "PASS," : "FAIL,") << detail << '\n';
        m_out.flush();
        QTimer::singleShot(300, &m_app, [this, success]() {
            m_core->pluginManager()->deactivateAll();
            m_app.exit(success ? 0 : 3);
        });
    }

    QApplication& m_app;
    bool m_move300mm = false;
    AppContext& m_context;
    DebugCore* m_core = nullptr;
    QTextStream m_out;
    QTextStream m_err;
    QElapsedTimer m_clock;
    QTimer m_heartbeat;
    QTimer m_watchdog;
    Stage m_stage = Stage::WaitingSafety;
    qint64 m_deadlineMs = 0;
    quint32 m_session = 0;
    quint16 m_heartbeatSequence = 1;
    quint16 m_latestFault = 0;
    quint8 m_activeBumper = 0;
    quint8 m_latchedBumper = 0;
    quint32 m_lastSafetyKey = 0;
    bool m_haveSafetyKey = false;
    bool m_motionAccepted = false;
    bool m_motionCompleted = false;
    qint32 m_latestOdomX = std::numeric_limits<qint32>::min();
    quint64 m_rxCount = 0;
    quint64 m_badCrcCount = 0;
    QSet<quint32> m_seenIds;
};

} // namespace

int runAgvCanFdTest(QApplication& app, AppContext& context, bool move300mm)
{
    AgvCanFdTestRunner runner(app, context, move300mm);
    return runner.run();
}
