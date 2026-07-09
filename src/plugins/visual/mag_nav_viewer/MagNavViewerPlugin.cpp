#include "MagNavViewerPlugin.h"

MagNavViewerPlugin::MagNavViewerPlugin(QWidget* parent)
    : IVisualPlugin(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_panel = new MagNavPanel(this);
    layout->addWidget(m_panel);
}

void MagNavViewerPlugin::onChannelData(const DataFrame& frame)
{
    m_panel->setFrame(frame);
}

QList<quint16> MagNavViewerPlugin::subscribedChannels()
{
    return {};
}

qint64 MagNavViewerPlugin::historyFrom()
{
    return 0;
}

QString MagNavViewerPlugin::name() const
{
    return QStringLiteral("MagNav Viewer");
}
