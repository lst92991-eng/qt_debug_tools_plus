#include "MagNavCsvProtocolPlugin.h"

#include <QRegularExpression>

#include <array>

namespace {
constexpr int metadataFieldCount = 5;
constexpr int channelCount = 8;
constexpr int expectedFieldCount = metadataFieldCount + channelCount;
}

MagNavCsvProtocolPlugin::MagNavCsvProtocolPlugin(QObject* parent)
    : IProtocolPlugin(parent)
{
}

void MagNavCsvProtocolPlugin::feedBytes(const QByteArray& raw)
{
    if (raw.isEmpty()) {
        return;
    }

    m_buffer.append(raw);
    parseBuffer();
}

QByteArray MagNavCsvProtocolPlugin::encodeCommand(const QVariantMap& command)
{
    if (command.contains(QStringLiteral("bytes"))) {
        return command.value(QStringLiteral("bytes")).toByteArray();
    }
    if (command.contains(QStringLiteral("text"))) {
        return command.value(QStringLiteral("text")).toString().toUtf8();
    }
    return parseHexString(command.value(QStringLiteral("hex")).toString());
}

QString MagNavCsvProtocolPlugin::name() const
{
    return QStringLiteral("MagNav CSV Protocol");
}

QString MagNavCsvProtocolPlugin::version() const
{
    return QStringLiteral("1.0.0");
}

void MagNavCsvProtocolPlugin::parseBuffer()
{
    while (true) {
        int end = m_buffer.indexOf('\n');
        const int cr = m_buffer.indexOf('\r');
        if (cr >= 0 && (end < 0 || cr < end)) {
            end = cr;
        }
        if (end < 0) {
            break;
        }

        const QByteArray lineBytes = m_buffer.left(end).trimmed();
        int removeCount = end + 1;
        while (removeCount < m_buffer.size()
               && (m_buffer.at(removeCount) == '\n' || m_buffer.at(removeCount) == '\r')) {
            ++removeCount;
        }
        m_buffer.remove(0, removeCount);

        DataFrame frame;
        if (parseLine(lineBytes, &frame)) {
            emit frameParsed(frame);
        }
    }

    constexpr int maxBufferedBytes = 8192;
    if (m_buffer.size() > maxBufferedBytes) {
        m_buffer = m_buffer.right(maxBufferedBytes);
    }
}

bool MagNavCsvProtocolPlugin::parseLine(const QByteArray& lineBytes, DataFrame* frame) const
{
    if (!frame || lineBytes.isEmpty()) {
        return false;
    }

    const QList<QByteArray> parts = lineBytes.split(',');
    if (parts.size() != expectedFieldCount) {
        return false;
    }

    std::array<double, expectedFieldCount> fields {};
    for (int index = 0; index < expectedFieldCount; ++index) {
        bool ok = false;
        fields.at(index) = parts.at(index).trimmed().toDouble(&ok);
        if (!ok) {
            return false;
        }
    }

    frame->timestamp_us = currentTimestampMicros();
    frame->rawPayload = lineBytes;
    frame->direction = FrameDirection::Receive;
    frame->attributes.insert(QStringLiteral("sample_format"), QStringLiteral("mag_nav_csv"));
    frame->attributes.insert(QStringLiteral("state"), fields.at(0));
    frame->attributes.insert(QStringLiteral("offset"), fields.at(1));
    frame->attributes.insert(QStringLiteral("strength"), fields.at(2));
    frame->attributes.insert(QStringLiteral("width"), fields.at(3));
    frame->attributes.insert(QStringLiteral("channel_mask"), fields.at(4));
    frame->channels.reserve(channelCount);

    for (int channel = 0; channel < channelCount; ++channel) {
        ChannelSample sample;
        sample.index = static_cast<quint16>(channel);
        sample.value = fields.at(metadataFieldCount + channel);
        sample.name = QStringLiteral("MAG%1").arg(channel);
        sample.unit = QStringLiteral("raw");
        frame->channels.push_back(sample);
    }
    return true;
}

QByteArray MagNavCsvProtocolPlugin::parseHexString(const QString& text)
{
    QString compact = text;
    compact.remove(QRegularExpression(QStringLiteral("[\\s,;:_-]")));
    if (compact.size() % 2 != 0) {
        return {};
    }

    QByteArray bytes;
    bytes.reserve(compact.size() / 2);
    for (int index = 0; index < compact.size(); index += 2) {
        bool ok = false;
        const int value = compact.mid(index, 2).toInt(&ok, 16);
        if (!ok) {
            return {};
        }
        bytes.append(static_cast<char>(value));
    }
    return bytes;
}
