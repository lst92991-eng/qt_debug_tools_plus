#pragma once

#include "sdk/IProtocolPlugin.h"

#include <QHash>
#include <QRegularExpression>

class SerialNumericPlugin : public IProtocolPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IProtocolPlugin_iid FILE "serial_numeric.json")
    Q_INTERFACES(IProtocolPlugin)

public:
    explicit SerialNumericPlugin(QObject* parent = nullptr);

    void feedBytes(const QByteArray& raw) override;
    QByteArray encodeCommand(const QVariantMap& command) override;
    QString name() const override;
    QString version() const override;

private:
    struct ParsedValue {
        QString name;
        double value = 0.0;
    };

    void parseBuffer();
    bool parseLine(const QString& line, QVector<ParsedValue>* values) const;
    bool parseJsonLine(const QString& line, QVector<ParsedValue>* values) const;
    bool parseKeyValueLine(const QString& line, QVector<ParsedValue>* values) const;
    bool parseDelimitedNumbers(const QString& line, QVector<ParsedValue>* values) const;
    quint16 channelForName(const QString& name);
    static QString normalizedName(const QString& name, int fallbackIndex);
    static QByteArray parseHexString(const QString& text);

    // 串口可能任意分片到达，m_buffer 只存尚未遇到换行符的尾巴。
    QByteArray m_buffer;
    // 同名通道保持稳定编号，避免曲线在运行中因 JSON 字段顺序变化而跳线。
    QHash<QString, quint16> m_channels;
    quint16 m_nextChannel = 0;
};
