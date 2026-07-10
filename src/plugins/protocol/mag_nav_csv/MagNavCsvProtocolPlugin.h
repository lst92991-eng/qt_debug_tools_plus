#pragma once

#include "sdk/IProtocolPlugin.h"

class MagNavCsvProtocolPlugin : public IProtocolPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IProtocolPlugin_iid FILE "mag_nav_csv.json")
    Q_INTERFACES(IProtocolPlugin)

public:
    explicit MagNavCsvProtocolPlugin(QObject* parent = nullptr);

    void feedBytes(const QByteArray& raw) override;
    QByteArray encodeCommand(const QVariantMap& command) override;

    QString name() const override;
    QString version() const override;

private:
    void parseBuffer();
    bool parseLine(const QByteArray& lineBytes, DataFrame* frame) const;
    static QByteArray parseHexString(const QString& text);

    QByteArray m_buffer;
};
