#pragma once

#include "MagNavPanel.h"
#include "sdk/IVisualPlugin.h"

#include <QVBoxLayout>

class MagNavViewerPlugin : public IVisualPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IVisualPlugin_iid FILE "mag_nav_viewer.json")
    Q_INTERFACES(IVisualPlugin)

public:
    explicit MagNavViewerPlugin(QWidget* parent = nullptr);

    void onChannelData(const DataFrame& frame) override;
    QList<quint16> subscribedChannels() override;
    qint64 historyFrom() override;
    QString name() const override;

private:
    MagNavPanel* m_panel = nullptr;
};
