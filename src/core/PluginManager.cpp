#include "core/PluginManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QSet>
#include <QStringList>
#include <QThread>

#include <utility>

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
    , m_ownerThread(QThread::currentThread())
{
}

PluginManager::~PluginManager()
{
    clear();
}

void PluginManager::clear()
{
    deactivateAll();
    m_physicalPlugins.clear();
    m_protocolPlugins.clear();
    m_visualPlugins.clear();
    m_controlPlugins.clear();
    for (LoadedPlugin* loaded : std::as_const(m_loaded)) {
        if (loaded->loader) {
            // 卸载动态库前必须先让活动物理连接关闭，否则后台读线程可能仍在库代码里运行。
            loaded->loader->unload();
        }
    }
    qDeleteAll(m_loaded);
    m_loaded.clear();
}

void PluginManager::scanPlugins(const QString& pluginDir)
{
    const QDir root(pluginDir);
    if (!root.exists()) {
        emit errorOccurred(tr("Plugin directory does not exist: %1").arg(pluginDir));
        return;
    }

    const QStringList filters = {
#if defined(Q_OS_WIN)
        "*.dll"
#elif defined(Q_OS_MACOS)
        "*.dylib"
#else
        "*.so"
#endif
    };

    const QFileInfoList files = root.entryInfoList(filters, QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo& file : files) {
        loadPluginFile(file.absoluteFilePath());
    }

    const QFileInfoList dirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& dir : dirs) {
        scanPlugins(dir.absoluteFilePath());
    }
}

void PluginManager::setWorkerThread(QThread* thread)
{
    m_workerThread = thread;
}

QList<IPhysicalPlugin*> PluginManager::physicalPlugins() const
{
    return m_physicalPlugins;
}

QList<IProtocolPlugin*> PluginManager::protocolPlugins() const
{
    return m_protocolPlugins;
}

QList<IVisualPlugin*> PluginManager::visualPlugins() const
{
    return m_visualPlugins;
}

QList<IControlPlugin*> PluginManager::controlPlugins() const
{
    return m_controlPlugins;
}

bool PluginManager::activatePhysical(const QString& name, const QVariantMap& config)
{
    for (IPhysicalPlugin* plugin : m_physicalPlugins) {
        if (plugin->name() != name) {
            continue;
        }

        if (m_activePhysical && m_activePhysical != plugin) {
            // 同一进程只维护一个活动设备；多设备调试按设计应打开多个工具实例。
            closePhysicalInThread(m_activePhysical);
            movePluginBackToOwner(m_activePhysical);
            emit physicalDeactivated();
        }

        moveObjectToThread(plugin, m_workerThread);
        if (!openPhysicalInThread(plugin, config)) {
            movePluginBackToOwner(plugin);
            emit errorOccurred(tr("Failed to open physical plugin: %1").arg(name));
            return false;
        }

        m_activePhysical = plugin;
        emit physicalActivated(plugin);
        return true;
    }

    emit errorOccurred(tr("Physical plugin not found: %1").arg(name));
    return false;
}

bool PluginManager::activateProtocol(const QString& name)
{
    for (IProtocolPlugin* plugin : m_protocolPlugins) {
        if (plugin->name() == name) {
            if (m_activeProtocol && m_activeProtocol != plugin) {
                movePluginBackToOwner(m_activeProtocol);
            }
            moveObjectToThread(plugin, m_workerThread);
            m_activeProtocol = plugin;
            emit protocolActivated(plugin);
            return true;
        }
    }

    emit errorOccurred(tr("Protocol plugin not found: %1").arg(name));
    return false;
}

void PluginManager::deactivateAll()
{
    if (m_activePhysical) {
        closePhysicalInThread(m_activePhysical);
        movePluginBackToOwner(m_activePhysical);
        m_activePhysical = nullptr;
        emit physicalDeactivated();
    }
    if (m_activeProtocol) {
        movePluginBackToOwner(m_activeProtocol);
    }
    m_activeProtocol = nullptr;
}

IPhysicalPlugin* PluginManager::activePhysical() const
{
    return m_activePhysical;
}

IProtocolPlugin* PluginManager::activeProtocol() const
{
    return m_activeProtocol;
}

void PluginManager::loadPluginFile(const QString& path)
{
    for (LoadedPlugin* loaded : std::as_const(m_loaded)) {
        if (loaded->path == path) {
            return;
        }
    }

    auto* loaded = new LoadedPlugin;
    loaded->loader.reset(new QPluginLoader(path));
    loaded->path = path;
    loaded->meta = loaded->loader->metaData().value("MetaData").toObject();
    if (!metadataSupportsCurrentPlatform(loaded->meta)) {
        // 平台不匹配不是错误：Windows/Linux 插件会同时出现在源码树里，运行时只加载本平台。
        delete loaded;
        return;
    }

    loaded->instance = loaded->loader->instance();

    if (!loaded->instance) {
        emit errorOccurred(tr("Failed to load %1: %2")
                               .arg(QFileInfo(path).fileName(), loaded->loader->errorString()));
        delete loaded;
        return;
    }

    registerInstance(loaded->instance, loaded->meta);
    m_loaded.append(loaded);
}

void PluginManager::registerInstance(QObject* instance, const QJsonObject& meta)
{
    // 接口类型由 qobject_cast 验证，JSON type 只作为额外防线，防止插件放错目录或元数据写错。
    if (auto* plugin = qobject_cast<IPhysicalPlugin*>(instance)) {
        if (metadataMatchesType(meta, "physical")) {
            m_physicalPlugins.append(plugin);
        }
        return;
    }

    if (auto* plugin = qobject_cast<IProtocolPlugin*>(instance)) {
        if (metadataMatchesType(meta, "protocol")) {
            m_protocolPlugins.append(plugin);
        }
        return;
    }

    if (auto* plugin = qobject_cast<IVisualPlugin*>(instance)) {
        if (metadataMatchesType(meta, "visual")) {
            m_visualPlugins.append(plugin);
        }
        return;
    }

    if (auto* plugin = qobject_cast<IControlPlugin*>(instance)) {
        if (metadataMatchesType(meta, "control")) {
            m_controlPlugins.append(plugin);
        }
        return;
    }

    emit errorOccurred(tr("Loaded object does not implement a known plugin interface: %1")
                           .arg(instance->objectName()));
}

bool PluginManager::metadataMatchesType(const QJsonObject& meta, const QString& expectedType) const
{
    const QString type = meta.value("type").toString();
    if (type.isEmpty() || type == expectedType) {
        return true;
    }
    emit const_cast<PluginManager*>(this)->errorOccurred(
        tr("Plugin metadata type mismatch: expected %1, got %2").arg(expectedType, type));
    return false;
}

bool PluginManager::metadataSupportsCurrentPlatform(const QJsonObject& meta) const
{
    const QJsonArray platforms = meta.value(QStringLiteral("platforms")).toArray();
    if (platforms.isEmpty()) {
        return true;
    }

    const QString platform = currentPlatform();
    for (const QJsonValue& value : platforms) {
        const QString entry = value.toString().toLower();
        if (entry == platform || entry == QStringLiteral("all")) {
            return true;
        }
    }
    return false;
}

QString PluginManager::currentPlatform() const
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#else
    return QStringLiteral("unknown");
#endif
}

void PluginManager::moveObjectToThread(QObject* object, QThread* targetThread)
{
    if (!object || !targetThread || object->thread() == targetThread) {
        return;
    }

    if (object->thread() == QThread::currentThread()) {
        object->moveToThread(targetThread);
        return;
    }

    QMetaObject::invokeMethod(
        object,
        [object, targetThread]() {
            object->moveToThread(targetThread);
        },
        Qt::BlockingQueuedConnection);
}

void PluginManager::movePluginBackToOwner(QObject* object)
{
    moveObjectToThread(object, m_ownerThread ? m_ownerThread : QCoreApplication::instance()->thread());
}

bool PluginManager::openPhysicalInThread(IPhysicalPlugin* plugin, const QVariantMap& config)
{
    if (!plugin) {
        return false;
    }

    bool opened = false;
    if (plugin->thread() == QThread::currentThread()) {
        opened = plugin->open(config);
    } else {
        QMetaObject::invokeMethod(
            plugin,
            [plugin, config, &opened]() {
                opened = plugin->open(config);
            },
            Qt::BlockingQueuedConnection);
    }
    return opened;
}

void PluginManager::closePhysicalInThread(IPhysicalPlugin* plugin)
{
    if (!plugin) {
        return;
    }

    if (plugin->thread() == QThread::currentThread()) {
        plugin->close();
        return;
    }

    QMetaObject::invokeMethod(
        plugin,
        [plugin]() {
            plugin->close();
        },
        Qt::BlockingQueuedConnection);
}
