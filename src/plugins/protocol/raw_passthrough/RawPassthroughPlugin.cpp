#include "RawPassthroughPlugin.h"

#include <QRegularExpression>

RawPassthroughPlugin::RawPassthroughPlugin(QObject* parent)
    : IProtocolPlugin(parent)
{
}

void RawPassthroughPlugin::feedBytes(const QByteArray& raw)
{
    if (raw.isEmpty()) {
        return;
    }

    DataFrame frame;
    frame.timestamp_us = currentTimestampMicros();
    frame.rawPayload = raw;
    frame.direction = FrameDirection::Receive;
    frame.attributes.insert(QStringLiteral("sample_format"), QStringLiteral("raw_bytes"));

    // Raw 协议没有帧结构，只把每个字节映射成一个数值通道，便于图表临时观察字节变化。
    // rawPayload 仍保留完整原始包，Raw Viewer 不依赖这些派生通道。
    frame.channels.reserve(raw.size());
    for (int i = 0; i < raw.size(); ++i) {
        ChannelSample sample;
        sample.index = static_cast<quint16>(i);
        sample.value = static_cast<quint8>(raw.at(i));
        sample.name = QStringLiteral("RX Byte %1").arg(i);
        sample.unit = QStringLiteral("byte");
        frame.channels.push_back(sample);
    }

    emit frameParsed(frame);
}

QByteArray RawPassthroughPlugin::encodeCommand(const QVariantMap& command)
{
    // Raw Control 已经会给 bytes；保留 hex 入口是为了脚本/会话文件可以直接描述文本命令。
    if (command.contains(QStringLiteral("bytes"))) {
        return command.value(QStringLiteral("bytes")).toByteArray();
    }
    return parseHexString(command.value(QStringLiteral("hex")).toString());
}

QString RawPassthroughPlugin::name() const
{
    return QStringLiteral("Raw Protocol");
}

QString RawPassthroughPlugin::version() const
{
    return QStringLiteral("1.0.0");
}

QByteArray RawPassthroughPlugin::parseHexString(const QString& text)
{
    QString compact = text;
    // 支持常见分隔符，便于从抓包工具或手册复制 "A5 01" / "A5-01" / "A5,01"。
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
