#include "SerialNumericPlugin.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <cmath>
#include <limits>
#include <utility>

SerialNumericPlugin::SerialNumericPlugin(QObject* parent)
    : IProtocolPlugin(parent)
{
}

void SerialNumericPlugin::feedBytes(const QByteArray& raw)
{
    if (raw.isEmpty()) {
        return;
    }

    m_buffer.append(raw);
    parseBuffer();
}

QByteArray SerialNumericPlugin::encodeCommand(const QVariantMap& command)
{
    if (command.contains(QStringLiteral("bytes"))) {
        return command.value(QStringLiteral("bytes")).toByteArray();
    }
    if (command.contains(QStringLiteral("text"))) {
        return command.value(QStringLiteral("text")).toString().toUtf8();
    }
    return parseHexString(command.value(QStringLiteral("hex")).toString());
}

QString SerialNumericPlugin::name() const
{
    return QStringLiteral("Serial Numeric Protocol");
}

QString SerialNumericPlugin::version() const
{
    return QStringLiteral("1.0.0");
}

void SerialNumericPlugin::parseBuffer()
{
    while (true) {
        // 同时兼容 \n、\r、\r\n；嵌入式串口打印经常混用这些换行风格。
        int end = m_buffer.indexOf('\n');
        int cr = m_buffer.indexOf('\r');
        if (cr >= 0 && (end < 0 || cr < end)) {
            end = cr;
        }
        if (end < 0) {
            break;
        }

        const QByteArray lineBytes = m_buffer.left(end);
        int removeCount = end + 1;
        while (removeCount < m_buffer.size()
               && (m_buffer.at(removeCount) == '\n' || m_buffer.at(removeCount) == '\r')) {
            ++removeCount;
        }
        m_buffer.remove(0, removeCount);

        const QString line = QString::fromUtf8(lineBytes).trimmed();
        if (line.isEmpty()) {
            continue;
        }

        QVector<ParsedValue> values;
        if (!parseLine(line, &values) || values.isEmpty()) {
            continue;
        }

        DataFrame frame;
        frame.timestamp_us = currentTimestampMicros();
        frame.rawPayload = lineBytes;
        frame.direction = FrameDirection::Receive;
        frame.attributes.insert(QStringLiteral("sample_format"), QStringLiteral("serial_numeric"));
        frame.attributes.insert(QStringLiteral("line"), line);
        frame.channels.reserve(values.size());

        for (const ParsedValue& value : std::as_const(values)) {
            if (!std::isfinite(value.value)) {
                // NaN/Inf 不进图表，避免坐标轴范围被异常文本样本拖坏。
                continue;
            }
            ChannelSample sample;
            sample.name = value.name;
            sample.index = channelForName(sample.name);
            sample.value = value.value;
            frame.channels.push_back(sample);
        }

        if (!frame.channels.isEmpty()) {
            emit frameParsed(frame);
        }
    }

    constexpr int maxBufferedBytes = 4096;
    if (m_buffer.size() > maxBufferedBytes) {
        // 长时间没有换行说明上游格式异常，保留尾部可恢复数据，防止内存被坏流量撑开。
        m_buffer = m_buffer.right(maxBufferedBytes);
    }
}

bool SerialNumericPlugin::parseLine(const QString& line, QVector<ParsedValue>* values) const
{
    // 解析优先级从结构化到宽松：JSON 最明确，key=value 次之，最后才把整行当纯数字列表。
    return parseJsonLine(line, values)
        || parseKeyValueLine(line, values)
        || parseDelimitedNumbers(line, values);
}

bool SerialNumericPlugin::parseJsonLine(const QString& line, QVector<ParsedValue>* values) const
{
    if (!line.startsWith(QLatin1Char('{')) && !line.startsWith(QLatin1Char('['))) {
        return false;
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        return false;
    }

    QVector<ParsedValue> parsed;
    if (doc.isArray()) {
        const QJsonArray array = doc.array();
        parsed.reserve(array.size());
        for (int i = 0; i < array.size(); ++i) {
            const QJsonValue value = array.at(i);
            if (value.isDouble()) {
                parsed.push_back({QStringLiteral("CH%1").arg(i), value.toDouble()});
            }
        }
    } else if (doc.isObject()) {
        const QJsonObject object = doc.object();
        parsed.reserve(object.size());
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (it.value().isDouble()) {
                parsed.push_back({it.key(), it.value().toDouble()});
            }
        }
    }

    if (parsed.isEmpty()) {
        return false;
    }

    *values = parsed;
    return true;
}

bool SerialNumericPlugin::parseKeyValueLine(const QString& line, QVector<ParsedValue>* values) const
{
    static const QRegularExpression rx(
        QStringLiteral(R"(([A-Za-z_][A-Za-z0-9_.-]*)\s*[:=]\s*([-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?))"));

    QVector<ParsedValue> parsed;
    auto it = rx.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        bool ok = false;
        const double value = match.captured(2).toDouble(&ok);
        if (ok) {
            parsed.push_back({match.captured(1), value});
        }
    }

    if (parsed.isEmpty()) {
        return false;
    }

    *values = parsed;
    return true;
}

bool SerialNumericPlugin::parseDelimitedNumbers(const QString& line, QVector<ParsedValue>* values) const
{
    QString normalized = line;
    normalized.replace(QLatin1Char(','), QLatin1Char(' '));
    normalized.replace(QLatin1Char(';'), QLatin1Char(' '));
    normalized.replace(QLatin1Char('\t'), QLatin1Char(' '));

    const QStringList parts = normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return false;
    }

    QVector<ParsedValue> parsed;
    parsed.reserve(parts.size());
    for (int i = 0; i < parts.size(); ++i) {
        bool ok = false;
        const double value = parts.at(i).toDouble(&ok);
        if (!ok) {
            return false;
        }
        parsed.push_back({QStringLiteral("CH%1").arg(i), value});
    }

    *values = parsed;
    return true;
}

quint16 SerialNumericPlugin::channelForName(const QString& name)
{
    const QString key = normalizedName(name, m_channels.size());
    auto it = m_channels.constFind(key);
    if (it != m_channels.constEnd()) {
        return it.value();
    }

    const quint16 channel = m_nextChannel;
    if (m_nextChannel < std::numeric_limits<quint16>::max()) {
        ++m_nextChannel;
    }
    // 达到 quint16 上限后会复用 65535，这比溢出回 0 更容易暴露通道数量异常。
    m_channels.insert(key, channel);
    return channel;
}

QString SerialNumericPlugin::normalizedName(const QString& name, int fallbackIndex)
{
    const QString trimmed = name.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("CH%1").arg(fallbackIndex) : trimmed;
}

QByteArray SerialNumericPlugin::parseHexString(const QString& text)
{
    QString compact = text;
    compact.remove(QRegularExpression(QStringLiteral("[\\s,;:_-]")));
    if (compact.size() % 2 != 0) {
        return {};
    }

    QByteArray bytes;
    bytes.reserve(compact.size() / 2);
    for (int i = 0; i < compact.size(); i += 2) {
        bool ok = false;
        const int value = compact.mid(i, 2).toInt(&ok, 16);
        if (!ok) {
            return {};
        }
        bytes.append(static_cast<char>(value));
    }
    return bytes;
}
