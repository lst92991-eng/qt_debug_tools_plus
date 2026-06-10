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

    QByteArray m_buffer;
    QHash<QString, quint16> m_channels;
    quint16 m_nextChannel = 0;
};
