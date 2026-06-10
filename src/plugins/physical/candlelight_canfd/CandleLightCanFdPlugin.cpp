#include "CandleLightCanFdPlugin.h"

#include <QRegularExpression>

#include <algorithm>
#include <cstring>
#include <limits>

#if defined(Q_OS_WIN)
#include <objbase.h>
#include <setupapi.h>

#ifndef RAW_IO
#define RAW_IO 0x07
#endif

namespace {
constexpr quint8 DirOut = 0x00;
constexpr quint8 DirIn = 0x80;
constexpr quint8 TypeVendor = 0x40;
constexpr quint8 RecipInterface = 0x01;

constexpr quint8 ReqSetHostFormat = 0;
constexpr quint8 ReqSetBitTiming = 1;
constexpr quint8 ReqSetDeviceMode = 2;
constexpr quint8 ReqGetCapabilities = 4;
constexpr quint8 ReqGetDeviceConfig = 5;
constexpr quint8 ReqSetBitTimingFd = 10;
constexpr quint8 ReqGetCapabilitiesFd = 11;
constexpr quint8 ReqSetTermination = 12;

constexpr quint32 ModeReset = 0;
constexpr quint32 ModeStart = 1;
constexpr quint32 FlagListenOnly = 0x00001;
constexpr quint32 FlagLoopback = 0x00002;
constexpr quint32 FlagOneShot = 0x00008;
constexpr quint32 FlagFd = 0x00100;
constexpr quint32 FeatureFd = 0x00100;
constexpr quint32 FeatureBitTimingFd = 0x00400;
constexpr quint32 FeatureTermination = 0x00800;

constexpr quint32 CanIdError = 0x20000000;
constexpr quint32 CanIdRtr = 0x40000000;
constexpr quint32 CanIdExtended = 0x80000000;
constexpr quint32 CanMask11 = 0x000007ff;
constexpr quint32 CanMask29 = 0x1fffffff;
constexpr quint32 EchoRx = 0xffffffff;
constexpr quint8 FrameFd = 0x02;
constexpr quint8 FrameBrs = 0x04;

constexpr quint8 CandleLightGuidBytes[16] = {
    0x08, 0x43, 0x5b, 0xc1, 0xd3, 0x04, 0xe6, 0x11,
    0xb3, 0xea, 0x60, 0x57, 0x18, 0x9e, 0x64, 0x43
};

#pragma pack(push, 1)
struct GsDeviceConfig {
    quint8 reserved1;
    quint8 reserved2;
    quint8 reserved3;
    quint8 icount;
    quint32 swVersion;
    quint32 hwVersion;
};

struct GsDeviceMode {
    quint32 mode;
    quint32 flags;
};

struct GsBitTiming {
    quint32 prop;
    quint32 seg1;
    quint32 seg2;
    quint32 sjw;
    quint32 brp;
};

struct GsTimeLimits {
    quint32 seg1Min;
    quint32 seg1Max;
    quint32 seg2Min;
    quint32 seg2Max;
    quint32 sjwMax;
    quint32 brpMin;
    quint32 brpMax;
    quint32 brpInc;
};

struct GsCapabilities {
    quint32 feature;
    quint32 fclkCan;
    GsTimeLimits time;
};

struct GsCapabilitiesFd {
    quint32 feature;
    quint32 fclkCan;
    GsTimeLimits nominal;
    GsTimeLimits data;
};

struct GsHostFrameHeader {
    quint32 echoId;
    quint32 canId;
    quint8 canDlc;
    quint8 channel;
    quint8 flags;
    quint8 reserved;
};
#pragma pack(pop)

quint32 readLe32(const char* data)
{
    const auto* p = reinterpret_cast<const unsigned char*>(data);
    return static_cast<quint32>(p[0])
        | (static_cast<quint32>(p[1]) << 8)
        | (static_cast<quint32>(p[2]) << 16)
        | (static_cast<quint32>(p[3]) << 24);
}

void appendLe32(QByteArray* out, quint32 value)
{
    out->append(static_cast<char>(value & 0xff));
    out->append(static_cast<char>((value >> 8) & 0xff));
    out->append(static_cast<char>((value >> 16) & 0xff));
    out->append(static_cast<char>((value >> 24) & 0xff));
}

GUID defaultCandleLightGuid()
{
    GUID guid = {};
    std::memcpy(&guid, CandleLightGuidBytes, sizeof(guid));
    return guid;
}

bool parseGuid(const QString& text, GUID* guid)
{
    if (!guid) {
        return false;
    }
    const QString normalized = text.trimmed();
    if (normalized.isEmpty()) {
        *guid = defaultCandleLightGuid();
        return true;
    }
    return CLSIDFromString(reinterpret_cast<LPCOLESTR>(normalized.utf16()), guid) == S_OK;
}

QStringList findDevicePaths(const GUID& guid, int vid, int pid)
{
    QStringList paths;
    HDEVINFO devInfo = SetupDiGetClassDevsW(&guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfo == INVALID_HANDLE_VALUE) {
        return paths;
    }

    for (DWORD index = 0;; ++index) {
        SP_DEVICE_INTERFACE_DATA interfaceData = {};
        interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(devInfo, nullptr, &guid, index, &interfaceData)) {
            break;
        }

        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);
        if (requiredSize == 0) {
            continue;
        }

        QByteArray storage(static_cast<int>(requiredSize), 0);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, detail, requiredSize, nullptr, nullptr)) {
            continue;
        }

        const QString path = QString::fromWCharArray(detail->DevicePath);
        const QString lower = path.toLower();
        const QString vidNeedle = QStringLiteral("vid_%1").arg(vid, 4, 16, QLatin1Char('0')).toLower();
        const QString pidNeedle = QStringLiteral("pid_%1").arg(pid, 4, 16, QLatin1Char('0')).toLower();
        if ((vid <= 0 || lower.contains(vidNeedle)) && (pid <= 0 || lower.contains(pidNeedle))) {
            paths.push_back(path);
        }
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    paths.sort(Qt::CaseInsensitive);
    return paths;
}

CandleLightCanFdPlugin::TimeLimits toLimits(const GsTimeLimits& wire)
{
    CandleLightCanFdPlugin::TimeLimits limits;
    limits.seg1Min = wire.seg1Min;
    limits.seg1Max = wire.seg1Max;
    limits.seg2Min = wire.seg2Min;
    limits.seg2Max = wire.seg2Max;
    limits.sjwMax = wire.sjwMax;
    limits.brpMin = wire.brpMin;
    limits.brpMax = wire.brpMax;
    limits.brpInc = wire.brpInc == 0 ? 1 : wire.brpInc;
    return limits;
}
}
#endif

CandleLightCanFdPlugin::CandleLightCanFdPlugin(QObject* parent)
    : IPhysicalPlugin(parent)
{
}

CandleLightCanFdPlugin::~CandleLightCanFdPlugin()
{
    close();
}

bool CandleLightCanFdPlugin::open(const QVariantMap& config)
{
#if defined(Q_OS_WIN)
    close();
    m_timeoutMs = static_cast<quint32>(parseInt(config.value(QStringLiteral("timeout_ms")), 500));
    m_forceFd = parseBool(config.value(QStringLiteral("force_fd")), true);
    m_brs = parseBool(config.value(QStringLiteral("brs")), true);

    if (!openWinUsb(config)) {
        close();
        emit statusChanged(false);
        return false;
    }
    if (!initializeDevice(config)) {
        close();
        emit statusChanged(false);
        return false;
    }

    m_running = true;
    m_open = true;
    m_worker = QThread::create([this]() { readLoop(); });
    m_worker->setObjectName(QStringLiteral("candlelight-canfd-reader"));
    m_worker->start();
    emit statusChanged(true);
    return true;
#else
    Q_UNUSED(config)
    emit errorOccurred(tr("CandleLight CANFD is only available on Windows with WinUSB"));
    emit statusChanged(false);
    return false;
#endif
}

void CandleLightCanFdPlugin::close()
{
    m_running = false;

#if defined(Q_OS_WIN)
    if (m_usb) {
        WinUsb_AbortPipe(m_usb, m_epIn);
    }
#endif

    if (m_worker) {
        m_worker->quit();
        m_worker->wait();
        delete m_worker;
        m_worker = nullptr;
    }

#if defined(Q_OS_WIN)
    if (m_usb) {
        setDeviceMode(ModeReset, 0);
        WinUsb_Free(m_usb);
        m_usb = nullptr;
    }
    if (m_device != INVALID_HANDLE_VALUE) {
        CloseHandle(m_device);
        m_device = INVALID_HANDLE_VALUE;
    }
#endif

    if (m_open.exchange(false)) {
        emit statusChanged(false);
    }
}

bool CandleLightCanFdPlugin::isOpen() const
{
    return m_open.load();
}

qint64 CandleLightCanFdPlugin::write(const QByteArray& data)
{
#if defined(Q_OS_WIN)
    if (!m_open || !m_usb) {
        emit errorOccurred(tr("CandleLight CANFD is not open"));
        return -1;
    }

    const QByteArray usbFrame = buildUsbFrame(data);
    if (usbFrame.isEmpty()) {
        return -1;
    }

    QMutexLocker locker(&m_ioMutex);
    unsigned long transferred = 0;
    if (!WinUsb_WritePipe(
            m_usb,
            m_epOut,
            reinterpret_cast<PUCHAR>(const_cast<char*>(usbFrame.constData())),
            static_cast<ULONG>(usbFrame.size()),
            &transferred,
            nullptr)) {
        emit errorOccurred(lastWindowsError(tr("CandleLight write failed")));
        return -1;
    }
    if (transferred != static_cast<unsigned long>(usbFrame.size())) {
        emit errorOccurred(tr("CandleLight wrote only %1 of %2 USB bytes").arg(transferred).arg(usbFrame.size()));
        return -1;
    }
    return data.size();
#else
    Q_UNUSED(data)
    emit errorOccurred(tr("CandleLight CANFD is only available on Windows"));
    return -1;
#endif
}

QString CandleLightCanFdPlugin::name() const
{
    return QStringLiteral("CandleLight CANFD");
}

QString CandleLightCanFdPlugin::version() const
{
    return QStringLiteral("1.0.0");
}

QVariantMap CandleLightCanFdPlugin::defaultConfig() const
{
    return {
        {QStringLiteral("device_guid"), QStringLiteral("{c15b4308-04d3-11e6-b3ea-6057189e6443}")},
        {QStringLiteral("device_path"), QStringLiteral("")},
        {QStringLiteral("device_index"), 0},
        {QStringLiteral("vid"), QStringLiteral("")},
        {QStringLiteral("pid"), QStringLiteral("")},
        {QStringLiteral("nominal_bitrate"), 500000},
        {QStringLiteral("data_bitrate"), 2000000},
        {QStringLiteral("nominal_sample_point"), 875},
        {QStringLiteral("data_sample_point"), 800},
        {QStringLiteral("force_fd"), QStringLiteral("true")},
        {QStringLiteral("brs"), QStringLiteral("true")},
        {QStringLiteral("listen_only"), QStringLiteral("false")},
        {QStringLiteral("loopback"), QStringLiteral("false")},
        {QStringLiteral("one_shot"), QStringLiteral("false")},
        {QStringLiteral("termination"), QStringLiteral("false")},
        {QStringLiteral("timeout_ms"), 500}
    };
}

int CandleLightCanFdPlugin::parseInt(const QVariant& value, int fallback)
{
    bool ok = false;
    int direct = value.toInt(&ok);
    if (ok) {
        return direct;
    }

    QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        return fallback;
    }
    direct = text.toInt(&ok, 0);
    if (!ok) {
        text.remove(QRegularExpression(QStringLiteral("[\\s,;:_-]")));
        direct = text.toInt(&ok, 16);
    }
    return ok ? direct : fallback;
}

bool CandleLightCanFdPlugin::parseBool(const QVariant& value, bool fallback)
{
    if (!value.isValid()) {
        return fallback;
    }
    const QString text = value.toString().trimmed().toLower();
    if (text.isEmpty()) {
        return fallback;
    }
    if (text == QStringLiteral("1") || text == QStringLiteral("true") || text == QStringLiteral("yes")
        || text == QStringLiteral("on")) {
        return true;
    }
    if (text == QStringLiteral("0") || text == QStringLiteral("false") || text == QStringLiteral("no")
        || text == QStringLiteral("off")) {
        return false;
    }
    return fallback;
}

int CandleLightCanFdPlugin::canFdLengthToDlc(int length)
{
    if (length <= 8) {
        return std::max(0, length);
    }
    if (length <= 12) {
        return 9;
    }
    if (length <= 16) {
        return 10;
    }
    if (length <= 20) {
        return 11;
    }
    if (length <= 24) {
        return 12;
    }
    if (length <= 32) {
        return 13;
    }
    if (length <= 48) {
        return 14;
    }
    return 15;
}

int CandleLightCanFdPlugin::canFdDlcToLength(int dlc)
{
    static constexpr int map[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
    if (dlc < 0 || dlc > 15) {
        return 0;
    }
    return map[dlc];
}

QByteArray CandleLightCanFdPlugin::buildProjectCanFrame(quint32 canId, const QByteArray& payload) const
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

QByteArray CandleLightCanFdPlugin::buildUsbFrame(const QByteArray& projectFrame)
{
#if defined(Q_OS_WIN)
    if (projectFrame.size() < 7
        || static_cast<quint8>(projectFrame.at(0)) != 0xca
        || static_cast<quint8>(projectFrame.at(1)) != 0xfd) {
        emit errorOccurred(tr("CandleLight expects CAN Frame bytes: CA FD + can_id + dlc + payload"));
        return {};
    }

    quint32 canId = (static_cast<quint8>(projectFrame.at(2)) << 24)
        | (static_cast<quint8>(projectFrame.at(3)) << 16)
        | (static_cast<quint8>(projectFrame.at(4)) << 8)
        | static_cast<quint8>(projectFrame.at(5));
    int length = static_cast<quint8>(projectFrame.at(6));
    if (length > 64 || projectFrame.size() < 7 + length) {
        emit errorOccurred(tr("Invalid CAN FD payload length"));
        return {};
    }

    QByteArray payload = projectFrame.mid(7, length);
    const bool extended = (canId & CanIdExtended) || ((canId & ~CanMask29) == 0 && canId > CanMask11);
    const bool rtr = (canId & CanIdRtr) != 0;
    canId &= CanMask29;
    if (extended) {
        canId |= CanIdExtended;
    }
    if (rtr) {
        canId |= CanIdRtr;
    }

    const bool fdFrame = m_forceFd || payload.size() > 8;
    if (!fdFrame && payload.size() > 8) {
        emit errorOccurred(tr("Classic CAN payload cannot exceed 8 bytes"));
        return {};
    }

    const int dlc = fdFrame ? canFdLengthToDlc(payload.size()) : payload.size();
    const int wireLength = fdFrame ? canFdDlcToLength(dlc) : payload.size();
    payload.resize(wireLength);

    QByteArray out;
    out.reserve(sizeof(GsHostFrameHeader) + (fdFrame ? 64 : 8));
    appendLe32(&out, ++m_echoId);
    appendLe32(&out, canId);
    out.append(static_cast<char>(dlc));
    out.append(static_cast<char>(m_channel));
    quint8 flags = 0;
    if (fdFrame) {
        flags |= FrameFd;
        if (m_brs) {
            flags |= FrameBrs;
        }
    }
    out.append(static_cast<char>(flags));
    out.append(static_cast<char>(0));
    out.append(payload);
    out.resize(sizeof(GsHostFrameHeader) + (fdFrame ? 64 : 8));
    return out;
#else
    Q_UNUSED(projectFrame)
    return {};
#endif
}

#if defined(Q_OS_WIN)
bool CandleLightCanFdPlugin::openWinUsb(const QVariantMap& config)
{
    const QVariantMap defaults = defaultConfig();
    QString devicePath = config.value(QStringLiteral("device_path"), defaults.value(QStringLiteral("device_path"))).toString().trimmed();
    if (devicePath.isEmpty()) {
        GUID guid = {};
        if (!parseGuid(config.value(QStringLiteral("device_guid"), defaults.value(QStringLiteral("device_guid"))).toString(), &guid)) {
            emit errorOccurred(tr("Invalid CandleLight device GUID"));
            return false;
        }

        const int vid = parseInt(config.value(QStringLiteral("vid")), 0);
        const int pid = parseInt(config.value(QStringLiteral("pid")), 0);
        const int deviceIndex = parseInt(config.value(QStringLiteral("device_index")), 0);
        const QStringList paths = findDevicePaths(guid, vid, pid);
        if (paths.isEmpty()) {
            emit errorOccurred(tr("No CandleLight WinUSB device found. Check driver binding and device_guid."));
            return false;
        }
        if (deviceIndex < 0 || deviceIndex >= paths.size()) {
            emit errorOccurred(tr("device_index %1 is out of range, found %2 device(s)").arg(deviceIndex).arg(paths.size()));
            return false;
        }
        devicePath = paths.at(deviceIndex);
    }

    m_device = CreateFileW(
        reinterpret_cast<LPCWSTR>(devicePath.utf16()),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        nullptr);
    if (m_device == INVALID_HANDLE_VALUE) {
        emit errorOccurred(lastWindowsError(tr("CreateFile failed")));
        return false;
    }

    if (!WinUsb_Initialize(m_device, &m_usb)) {
        emit errorOccurred(lastWindowsError(tr("WinUsb_Initialize failed")));
        return false;
    }

    ULONG timeout = m_timeoutMs;
    WinUsb_SetPipePolicy(m_usb, 0, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);

    USB_INTERFACE_DESCRIPTOR interfaceDescriptor = {};
    if (!WinUsb_QueryInterfaceSettings(m_usb, 0, &interfaceDescriptor)) {
        emit errorOccurred(lastWindowsError(tr("WinUsb_QueryInterfaceSettings failed")));
        return false;
    }
    m_interfaceNumber = interfaceDescriptor.bInterfaceNumber;
    m_channel = m_interfaceNumber > 0 ? static_cast<quint8>(m_interfaceNumber - 1) : 0;

    for (UCHAR pipeIndex = 0; pipeIndex < interfaceDescriptor.bNumEndpoints; ++pipeIndex) {
        WINUSB_PIPE_INFORMATION pipe = {};
        if (!WinUsb_QueryPipe(m_usb, 0, pipeIndex, &pipe)) {
            emit errorOccurred(lastWindowsError(tr("WinUsb_QueryPipe failed")));
            return false;
        }
        if (pipe.PipeType != UsbdPipeTypeBulk) {
            continue;
        }
        if (pipe.PipeId & DirIn) {
            m_epIn = pipe.PipeId;
            UCHAR rawIo = 1;
            WinUsb_SetPipePolicy(m_usb, m_epIn, RAW_IO, sizeof(rawIo), &rawIo);
        } else {
            m_epOut = pipe.PipeId;
            ULONG outTimeout = m_timeoutMs;
            WinUsb_SetPipePolicy(m_usb, m_epOut, PIPE_TRANSFER_TIMEOUT, sizeof(outTimeout), &outTimeout);
        }
    }

    if (m_epIn == 0 || m_epOut == 0) {
        emit errorOccurred(tr("CandleLight bulk endpoints not found"));
        return false;
    }
    return true;
}

bool CandleLightCanFdPlugin::initializeDevice(const QVariantMap& config)
{
    quint32 hostFormat = 0x0000beef;
    if (!controlTransfer(ReqSetHostFormat, DirOut, 1, &hostFormat, sizeof(hostFormat))) {
        return false;
    }

    GsDeviceConfig deviceConfig = {};
    if (!controlTransfer(ReqGetDeviceConfig, DirIn, 1, &deviceConfig, sizeof(deviceConfig))) {
        return false;
    }

    if (!setDeviceMode(ModeReset, 0)) {
        return false;
    }
    if (!readCapabilities()) {
        return false;
    }

    const int nominalBitrate = parseInt(config.value(QStringLiteral("nominal_bitrate")), 500000);
    const int dataBitrate = parseInt(config.value(QStringLiteral("data_bitrate")), 2000000);
    const int nominalSample = parseInt(config.value(QStringLiteral("nominal_sample_point")), 875);
    const int dataSample = parseInt(config.value(QStringLiteral("data_sample_point")), 800);

    BitTiming nominal;
    if (!solveBitTiming(nominalBitrate, nominalSample, m_caps.nominal, &nominal)) {
        emit errorOccurred(tr("Cannot solve nominal CAN timing for %1 bps").arg(nominalBitrate));
        return false;
    }
    if (!setBitTiming(ReqSetBitTiming, nominal)) {
        return false;
    }

    quint32 modeFlags = 0;
    if (parseBool(config.value(QStringLiteral("listen_only")), false)) {
        modeFlags |= FlagListenOnly;
    }
    if (parseBool(config.value(QStringLiteral("loopback")), false)) {
        modeFlags |= FlagLoopback;
    }
    if (parseBool(config.value(QStringLiteral("one_shot")), false)) {
        modeFlags |= FlagOneShot;
    }

    if (m_forceFd) {
        if (!m_caps.supportsFd) {
            emit errorOccurred(tr("Device does not report CAN FD support"));
            return false;
        }
        BitTiming data;
        if (!solveBitTiming(dataBitrate, dataSample, m_caps.data, &data)) {
            emit errorOccurred(tr("Cannot solve data CAN FD timing for %1 bps").arg(dataBitrate));
            return false;
        }
        if (!setBitTiming(ReqSetBitTimingFd, data)) {
            return false;
        }
        modeFlags |= FlagFd;
    }

    if (parseBool(config.value(QStringLiteral("termination")), false)
        && (m_caps.feature & FeatureTermination)) {
        quint32 termination = 1;
        controlTransfer(ReqSetTermination, DirOut, m_channel, &termination, sizeof(termination));
    }

    return setDeviceMode(ModeStart, modeFlags);
}

bool CandleLightCanFdPlugin::setBitTiming(quint8 request, const BitTiming& timing)
{
    GsBitTiming wire = {};
    wire.prop = timing.prop;
    wire.seg1 = timing.seg1;
    wire.seg2 = timing.seg2;
    wire.sjw = timing.sjw;
    wire.brp = timing.brp;
    return controlTransfer(request, DirOut, m_channel, &wire, sizeof(wire));
}

bool CandleLightCanFdPlugin::setDeviceMode(quint32 mode, quint32 flags)
{
    if (!m_usb) {
        return true;
    }
    GsDeviceMode wire = {};
    wire.mode = mode;
    wire.flags = flags;
    return controlTransfer(ReqSetDeviceMode, DirOut, m_channel, &wire, sizeof(wire));
}

bool CandleLightCanFdPlugin::controlTransfer(quint8 request, quint8 direction, quint16 value, void* data, quint32 size, quint32* transferred)
{
    WINUSB_SETUP_PACKET setup = {};
    setup.RequestType = RecipInterface | TypeVendor | direction;
    setup.Request = request;
    setup.Value = value;
    setup.Index = m_interfaceNumber;
    setup.Length = static_cast<USHORT>(size);

    ULONG done = 0;
    if (!WinUsb_ControlTransfer(
            m_usb,
            setup,
            reinterpret_cast<PUCHAR>(data),
            static_cast<ULONG>(size),
            &done,
            nullptr)) {
        emit errorOccurred(lastWindowsError(tr("CandleLight control request %1 failed").arg(request)));
        return false;
    }

    if ((direction & DirIn) && done < size) {
        emit errorOccurred(tr("CandleLight control request %1 returned %2 of %3 bytes").arg(request).arg(done).arg(size));
        return false;
    }
    if (transferred) {
        *transferred = done;
    }
    return true;
}

bool CandleLightCanFdPlugin::readCapabilities()
{
    GsCapabilities classicCaps = {};
    if (!controlTransfer(ReqGetCapabilities, DirIn, m_channel, &classicCaps, sizeof(classicCaps))) {
        return false;
    }

    m_caps.feature = classicCaps.feature;
    m_caps.fclkCan = classicCaps.fclkCan ? classicCaps.fclkCan : 80000000;
    m_caps.nominal = toLimits(classicCaps.time);
    m_caps.data = m_caps.nominal;
    m_caps.supportsFd = (classicCaps.feature & FeatureFd) && (classicCaps.feature & FeatureBitTimingFd);

    if (m_caps.supportsFd) {
        GsCapabilitiesFd fdCaps = {};
        if (controlTransfer(ReqGetCapabilitiesFd, DirIn, m_channel, &fdCaps, sizeof(fdCaps))) {
            m_caps.feature = fdCaps.feature;
            m_caps.fclkCan = fdCaps.fclkCan ? fdCaps.fclkCan : m_caps.fclkCan;
            m_caps.nominal = toLimits(fdCaps.nominal);
            m_caps.data = toLimits(fdCaps.data);
        }
    }
    return true;
}

bool CandleLightCanFdPlugin::solveBitTiming(int bitrate, int samplePointPermille, const TimeLimits& limits, BitTiming* timing) const
{
    if (!timing || bitrate <= 0 || m_caps.fclkCan == 0) {
        return false;
    }

    struct Candidate {
        quint64 score = std::numeric_limits<quint64>::max();
        BitTiming timing;
    } best;

    const quint32 brpInc = std::max<quint32>(1, limits.brpInc);
    for (quint32 brp = std::max<quint32>(1, limits.brpMin); brp <= limits.brpMax; brp += brpInc) {
        const double exactTq = static_cast<double>(m_caps.fclkCan) / (static_cast<double>(brp) * bitrate);
        const int tqCenter = static_cast<int>(exactTq + 0.5);
        for (int totalTq = std::max(3, tqCenter - 1); totalTq <= tqCenter + 1; ++totalTq) {
            if (totalTq <= 2) {
                continue;
            }
            const int produced = static_cast<int>(m_caps.fclkCan / (brp * totalTq));
            const int bitrateError = std::abs(produced - bitrate);
            int seg1 = (samplePointPermille * totalTq + 500) / 1000 - 1;
            seg1 = std::clamp(seg1, static_cast<int>(limits.seg1Min), static_cast<int>(limits.seg1Max));
            const int seg2 = totalTq - 1 - seg1;
            if (seg2 < static_cast<int>(limits.seg2Min) || seg2 > static_cast<int>(limits.seg2Max)) {
                continue;
            }
            const int sample = 1000 * (1 + seg1) / totalTq;
            const int sampleError = std::abs(sample - samplePointPermille);
            const quint64 score = static_cast<quint64>(bitrateError) * 10000ULL + static_cast<quint64>(sampleError);
            if (score < best.score) {
                best.score = score;
                best.timing.prop = 0;
                best.timing.seg1 = static_cast<quint32>(seg1);
                best.timing.seg2 = static_cast<quint32>(seg2);
                best.timing.sjw = std::min<quint32>({static_cast<quint32>(seg1), static_cast<quint32>(seg2), limits.sjwMax});
                best.timing.brp = brp;
            }
        }
    }

    if (best.score == std::numeric_limits<quint64>::max()) {
        return false;
    }
    *timing = best.timing;
    return true;
}

void CandleLightCanFdPlugin::readLoop()
{
    QByteArray buffer(256, '\0');
    while (m_running) {
        ULONG transferred = 0;
        const BOOL ok = WinUsb_ReadPipe(
            m_usb,
            m_epIn,
            reinterpret_cast<PUCHAR>(buffer.data()),
            static_cast<ULONG>(buffer.size()),
            &transferred,
            nullptr);

        if (ok && transferred > 0) {
            parseUsbPacket(buffer.left(static_cast<qsizetype>(transferred)));
            continue;
        }

        const DWORD error = GetLastError();
        if (!m_running || error == ERROR_SEM_TIMEOUT || error == ERROR_OPERATION_ABORTED) {
            continue;
        }
        emit errorOccurred(lastWindowsError(tr("CandleLight read failed")));
        break;
    }

    if (m_open.exchange(false)) {
        emit statusChanged(false);
    }
}

void CandleLightCanFdPlugin::parseUsbPacket(const QByteArray& packet)
{
    if (packet.size() < static_cast<int>(sizeof(GsHostFrameHeader))) {
        return;
    }

    const char* data = packet.constData();
    const quint32 echoId = readLe32(data);
    quint32 canId = readLe32(data + 4);
    const int dlc = static_cast<quint8>(data[8]);
    const quint8 flags = static_cast<quint8>(data[10]);

    if (echoId != EchoRx || (canId & CanIdError) || (canId & CanIdRtr)) {
        return;
    }

    const bool fdFrame = (flags & FrameFd) != 0;
    const int length = fdFrame ? canFdDlcToLength(dlc) : std::min(dlc, 8);
    if (length < 0 || packet.size() < static_cast<int>(sizeof(GsHostFrameHeader)) + length) {
        return;
    }

    const bool extended = (canId & CanIdExtended) != 0;
    canId &= extended ? CanMask29 : CanMask11;
    const QByteArray payload = packet.mid(static_cast<int>(sizeof(GsHostFrameHeader)), length);
    emit dataReceived(buildProjectCanFrame(canId, payload));
}

QString CandleLightCanFdPlugin::lastWindowsError(const QString& prefix) const
{
    const DWORD error = GetLastError();
    LPWSTR buffer = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);
    const QString message = buffer ? QString::fromWCharArray(buffer).trimmed() : QString::number(error);
    if (buffer) {
        LocalFree(buffer);
    }
    return QStringLiteral("%1: %2").arg(prefix, message);
}
#else
void CandleLightCanFdPlugin::readLoop() {}
#endif
