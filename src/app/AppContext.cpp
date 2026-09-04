#include "app/AppContext.h"

#include "core/DebugCore.h"

#include <QCoreApplication>
#include <QDir>

AppContext::AppContext()
    : m_core(DebugCore::instance())
{
    m_core->initialize();
}

DebugCore* AppContext::debugCore() const
{
    return m_core;
}

QString AppContext::pluginRoot() const
{
    const QString appPlugins = QDir(QCoreApplication::applicationDirPath())
                                   .filePath(QStringLiteral("plugins"));
    if (QDir(appPlugins).exists()) {
        return appPlugins;
    }

    const QString cwdPlugins = QDir(QDir::currentPath()).filePath(QStringLiteral("plugins"));
    return QDir(cwdPlugins).exists() ? cwdPlugins : appPlugins;
}

void AppContext::scanPlugins() const
{
    m_core->pluginManager()->scanPlugins(pluginRoot());
}
