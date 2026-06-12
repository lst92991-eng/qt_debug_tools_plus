#include "MainWindow.h"

#include "app/DeviceConfigDialog.h"
#include "ui_MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QSize>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSet>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_ui(std::make_unique<Ui::MainWindow>())
    , m_core(DebugCore::instance())
{
    m_core->initialize();
    buildUi();
    connect(m_core, &DebugCore::errorOccurred, this, [this](const QString& message) {
        statusBar()->showMessage(message, 6000);
        appendActivity(tr("Error: %1").arg(message));
    });
    connect(m_core, &DebugCore::framePublished, this, [this](const DataFrame& frame) {
        if (frame.direction == FrameDirection::Receive) {
            ++m_rxFrames;
        }
        updateTrafficCounters();
    });
    connect(m_core, &DebugCore::commandSent, this, [this](const QByteArray& bytes) {
        Q_UNUSED(bytes)
        ++m_txCommands;
        updateTrafficCounters();
    });

    scanPlugins();
    populatePluginUi();
    setConnected(false);
}

MainWindow::~MainWindow()
{
    detachPluginPages();
    m_core->pluginManager()->clear();
}

void MainWindow::buildUi()
{
    m_ui->setupUi(this);

    m_physicalCombo = m_ui->physicalCombo;
    m_protocolCombo = m_ui->protocolCombo;
    m_configButton = m_ui->configButton;
    m_connectButton = m_ui->connectButton;
    m_disconnectButton = m_ui->disconnectButton;
    m_statusLabel = m_ui->statusLabel;
    m_pluginSummaryLabel = m_ui->pluginSummaryLabel;
    m_rxCounterLabel = m_ui->rxCounterLabel;
    m_txCounterLabel = m_ui->txCounterLabel;
    m_stagePhysicalValue = m_ui->stagePhysicalValue;
    m_stageProtocolValue = m_ui->stageProtocolValue;
    m_configTitleLabel = m_ui->configTitleLabel;
    m_configTable = m_ui->configTable;
    m_sideStack = m_ui->sideStack;
    m_activityConfigButton = m_ui->activityConfigButton;
    m_activityPluginsButton = m_ui->activityPluginsButton;
    m_activityDashboardButton = m_ui->activityDashboardButton;
    m_activityOutputButton = m_ui->activityOutputButton;
    m_visualTabs = m_ui->visualTabs;
    m_controlTabs = m_ui->controlTabs;
    m_pluginTree = m_ui->pluginTree;
    m_activityLog = m_ui->activityLog;

    m_ui->sideHeader->setFixedHeight(52);
    m_ui->editorHeader->setFixedHeight(52);
    m_ui->commandHeader->setFixedHeight(52);
    m_ui->panelHeader->setFixedHeight(36);
    m_ui->activitySettingsButton->hide();

    const auto setupActivityButton = [this](QToolButton* button, QStyle::StandardPixmap icon) {
        button->setText(QString());
        button->setIcon(style()->standardIcon(icon));
        button->setIconSize(QSize(22, 22));
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setFixedSize(52, 44);
    };
    setupActivityButton(m_activityConfigButton, QStyle::SP_FileDialogDetailedView);
    setupActivityButton(m_activityPluginsButton, QStyle::SP_DirOpenIcon);
    setupActivityButton(m_activityDashboardButton, QStyle::SP_ComputerIcon);
    setupActivityButton(m_activityOutputButton, QStyle::SP_ArrowRight);

    const auto setupButton = [](QPushButton* button, int width) {
        button->setFixedHeight(28);
        button->setMinimumWidth(width);
    };
    setupButton(m_configButton, 104);
    setupButton(m_connectButton, 88);
    setupButton(m_disconnectButton, 104);
    setupButton(m_ui->saveHistoryButton, 104);
    setupButton(m_ui->loadHistoryButton, 104);
    setupButton(m_ui->clearHistoryButton, 64);
    m_statusLabel->setFixedHeight(28);
    m_physicalCombo->setFixedHeight(28);
    m_protocolCombo->setFixedHeight(28);

    m_configTable->verticalHeader()->hide();
    m_configTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_configTable->setAlternatingRowColors(true);
    m_configTable->verticalHeader()->setDefaultSectionSize(26);
    m_configTable->setColumnWidth(0, 136);

    m_ui->mainSplitter->setSizes({285, 1020});
    m_ui->editorSplitter->setSizes({760, 340});
    m_ui->verticalSplitter->setSizes({620, 180});

    connect(m_ui->actionSaveSession, &QAction::triggered, this, &MainWindow::saveSession);
    connect(m_ui->actionLoadSession, &QAction::triggered, this, &MainWindow::loadSession);
    connect(m_ui->actionSaveHistory, &QAction::triggered, this, &MainWindow::saveHistory);
    connect(m_ui->actionLoadHistory, &QAction::triggered, this, &MainWindow::loadHistory);
    connect(m_ui->actionClearHistory, &QAction::triggered, this, &MainWindow::clearHistory);
    connect(m_ui->actionExit, &QAction::triggered, qApp, &QApplication::quit);

    connect(m_ui->actionRescanPlugins, &QAction::triggered, this, [this]() {
        detachPluginPages();
        m_core->pluginManager()->clear();
        scanPlugins();
        populatePluginUi();
    });

    connect(m_ui->actionChannelMap, &QAction::triggered, this, &MainWindow::editChannelMap);
    connect(m_ui->actionSerialBaudScan, &QAction::triggered, this, &MainWindow::scanSerialBaudRates);
    connect(m_ui->saveHistoryButton, &QPushButton::clicked, this, &MainWindow::saveHistory);
    connect(m_ui->loadHistoryButton, &QPushButton::clicked, this, &MainWindow::loadHistory);
    connect(m_ui->clearHistoryButton, &QPushButton::clicked, this, &MainWindow::clearHistory);

    connect(m_configButton, &QPushButton::clicked, this, &MainWindow::applySelectedPhysicalConfig);
    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::connectDevice);
    connect(m_disconnectButton, &QPushButton::clicked, this, &MainWindow::disconnectDevice);
    connect(m_physicalCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        m_stagePhysicalValue->setText(text.isEmpty() ? tr("-") : text);
        updateConfigPage();
    });
    connect(m_protocolCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        m_stageProtocolValue->setText(text.isEmpty() ? tr("-") : text);
        updateConfigPage();
    });
    connect(m_activityConfigButton, &QToolButton::clicked, this, [this]() { showSidePage(0); });
    connect(m_activityPluginsButton, &QToolButton::clicked, this, [this]() { showSidePage(1); });
    connect(m_activityDashboardButton, &QToolButton::clicked, this, [this]() {
        setActivityButtonState(m_activityDashboardButton);
        m_visualTabs->setFocus();
    });
    connect(m_activityOutputButton, &QToolButton::clicked, this, [this]() {
        setActivityButtonState(m_activityOutputButton);
        m_controlTabs->setFocus();
    });

    showSidePage(0);
    appendActivity(tr("Workspace ready"));
}

void MainWindow::scanPlugins()
{
    const QString root = pluginRoot();
    m_core->pluginManager()->scanPlugins(root);
    appendActivity(tr("Scanned plugins"));
    statusBar()->showMessage(tr("Plugins scanned"), 5000);
}

void MainWindow::populatePluginUi()
{
    PluginManager* manager = m_core->pluginManager();

    m_physicalCombo->clear();
    for (IPhysicalPlugin* plugin : manager->physicalPlugins()) {
        m_physicalCombo->addItem(plugin->name());
        m_physicalConfigs.insert(plugin->name(), plugin->defaultConfig());
    }
    m_stagePhysicalValue->setText(m_physicalCombo->currentText().isEmpty() ? tr("-") : m_physicalCombo->currentText());
    updateConfigPage();

    m_protocolCombo->clear();
    for (IProtocolPlugin* plugin : manager->protocolPlugins()) {
        m_protocolCombo->addItem(plugin->name());
    }
    m_stageProtocolValue->setText(m_protocolCombo->currentText().isEmpty() ? tr("-") : m_protocolCombo->currentText());

    m_visualTabs->clear();
    for (IVisualPlugin* plugin : manager->visualPlugins()) {
        m_visualTabs->addTab(plugin, plugin->name());
        m_core->channelHub()->subscribe(plugin, plugin->subscribedChannels());
    }

    m_controlTabs->clear();
    for (IControlPlugin* plugin : manager->controlPlugins()) {
        m_controlTabs->addTab(plugin, plugin->name());
        connect(plugin, &IControlPlugin::commandGenerated, m_core, &DebugCore::sendCommand);
    }

    const bool canConnect = m_physicalCombo->count() > 0 && m_protocolCombo->count() > 0;
    m_connectButton->setEnabled(canConnect);
    m_configButton->setEnabled(m_physicalCombo->count() > 0);
    updatePluginOverview();
    appendActivity(tr("Loaded %1 physical, %2 protocol, %3 visual, %4 control plugins")
                       .arg(manager->physicalPlugins().size())
                       .arg(manager->protocolPlugins().size())
                       .arg(manager->visualPlugins().size())
                       .arg(manager->controlPlugins().size()));
}

void MainWindow::applySelectedPhysicalConfig()
{
    IPhysicalPlugin* plugin = selectedPhysical();
    if (!plugin) {
        return;
    }

    const QString key = plugin->name();
    QVariantMap config = m_physicalConfigs.value(key, plugin->defaultConfig());
    for (int row = 0; row < m_configTable->rowCount(); ++row) {
        const QTableWidgetItem* keyItem = m_configTable->item(row, 0);
        const QTableWidgetItem* valueItem = m_configTable->item(row, 1);
        if (!keyItem || !valueItem) {
            continue;
        }
        config.insert(keyItem->text(), valueItem->text().trimmed());
    }
    m_physicalConfigs.insert(key, config);
    updateConfigPage();
    appendActivity(tr("Applied config for %1").arg(key));
    statusBar()->showMessage(tr("Config applied"), 2500);
}

void MainWindow::connectDevice()
{
    IPhysicalPlugin* physical = selectedPhysical();
    IProtocolPlugin* protocol = selectedProtocol();
    if (!physical || !protocol) {
        return;
    }

    PluginManager* manager = m_core->pluginManager();
    if (!manager->activateProtocol(protocol->name())) {
        return;
    }

    QObject::disconnect(m_activeStatusConnection);
    const QVariantMap config = m_physicalConfigs.value(physical->name(), physical->defaultConfig());
    appendActivity(tr("Opening %1 through %2").arg(physical->name(), protocol->name()));
    if (!manager->activatePhysical(physical->name(), config)) {
        setConnected(false);
        return;
    }

    m_activeStatusConnection = connect(physical, &IPhysicalPlugin::statusChanged, this, &MainWindow::setConnected);
    setConnected(true);
    appendActivity(tr("Connected to %1").arg(physical->name()));
}

void MainWindow::disconnectDevice()
{
    QObject::disconnect(m_activeStatusConnection);
    m_activeStatusConnection = {};
    m_core->pluginManager()->deactivateAll();
    setConnected(false);
    appendActivity(tr("Disconnected"));
}

void MainWindow::saveSession()
{
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save Session"),
        QDir::home().filePath(QStringLiteral("mcu_debug_session.json")),
        tr("MCU Debug Session (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QJsonObject root;
    root.insert(QStringLiteral("physical"), m_physicalCombo->currentText());
    root.insert(QStringLiteral("protocol"), m_protocolCombo->currentText());
    root.insert(QStringLiteral("channel_metadata"), QJsonObject::fromVariantMap(m_core->channelMetadata()));

    QJsonObject configs;
    for (auto it = m_physicalConfigs.constBegin(); it != m_physicalConfigs.constEnd(); ++it) {
        configs.insert(it.key(), QJsonObject::fromVariantMap(it.value()));
    }
    root.insert(QStringLiteral("physical_configs"), configs);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        statusBar()->showMessage(tr("Failed to save session: %1").arg(file.errorString()), 6000);
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    appendActivity(tr("Saved session to %1").arg(QDir::toNativeSeparators(path)));
    statusBar()->showMessage(tr("Session saved"), 3000);
}

void MainWindow::loadSession()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Load Session"),
        QDir::homePath(),
        tr("MCU Debug Session (*.json)"));
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        statusBar()->showMessage(tr("Failed to load session: %1").arg(file.errorString()), 6000);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();
    const QJsonObject configs = root.value(QStringLiteral("physical_configs")).toObject();
    for (auto it = configs.constBegin(); it != configs.constEnd(); ++it) {
        m_physicalConfigs.insert(it.key(), it.value().toObject().toVariantMap());
    }
    m_core->setChannelMetadata(root.value(QStringLiteral("channel_metadata")).toObject().toVariantMap());

    const int physicalIdx = m_physicalCombo->findText(root.value(QStringLiteral("physical")).toString());
    if (physicalIdx >= 0) {
        m_physicalCombo->setCurrentIndex(physicalIdx);
    }
    const int protocolIdx = m_protocolCombo->findText(root.value(QStringLiteral("protocol")).toString());
    if (protocolIdx >= 0) {
        m_protocolCombo->setCurrentIndex(protocolIdx);
    }
    appendActivity(tr("Loaded session from %1").arg(QDir::toNativeSeparators(path)));
    statusBar()->showMessage(tr("Session loaded"), 3000);
}

void MainWindow::saveHistory()
{
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save History"),
        QDir::home().filePath(QStringLiteral("mcu_debug_history.mcdr")),
        tr("MCU Debug History (*.mcdr)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!m_core->ringBufferPool()->saveToFile(path, &error)) {
        statusBar()->showMessage(tr("Failed to save history: %1").arg(error), 6000);
        return;
    }
    appendActivity(tr("Saved history to %1").arg(QDir::toNativeSeparators(path)));
    statusBar()->showMessage(tr("History saved"), 3000);
}

void MainWindow::loadHistory()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Load History"),
        QDir::homePath(),
        tr("MCU Debug History (*.mcdr)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!m_core->ringBufferPool()->loadFromFile(path, &error)) {
        statusBar()->showMessage(tr("Failed to load history: %1").arg(error), 6000);
        return;
    }
    appendActivity(tr("Loaded history from %1").arg(QDir::toNativeSeparators(path)));
    statusBar()->showMessage(tr("History loaded"), 3000);
}

void MainWindow::clearHistory()
{
    m_core->ringBufferPool()->clear();
    m_rxFrames = 0;
    updateTrafficCounters();
    appendActivity(tr("Cleared history"));
    statusBar()->showMessage(tr("History cleared"), 3000);
}

void MainWindow::editChannelMap()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Channel Map"));
    dialog.resize(520, 420);

    auto* root = new QVBoxLayout(&dialog);
    auto* table = new QTableWidget(&dialog);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({tr("Index"), tr("Name"), tr("Unit")});
    root->addWidget(table, 1);

    QSet<quint16> channels;
    for (quint16 channel : m_core->ringBufferPool()->activeChannels()) {
        channels.insert(channel);
    }
    const QVariantMap metadata = m_core->channelMetadata();
    for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
        bool ok = false;
        const quint16 channel = static_cast<quint16>(it.key().toUShort(&ok));
        if (ok) {
            channels.insert(channel);
        }
    }

    QList<quint16> sortedChannels = channels.values();
    std::sort(sortedChannels.begin(), sortedChannels.end());
    table->setRowCount(sortedChannels.size());
    for (int row = 0; row < sortedChannels.size(); ++row) {
        const quint16 channel = sortedChannels.at(row);
        const QVariantMap meta = metadata.value(QString::number(channel)).toMap();
        auto* indexItem = new QTableWidgetItem(QString::number(channel));
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 0, indexItem);
        table->setItem(row, 1, new QTableWidgetItem(meta.value(QStringLiteral("name")).toString()));
        table->setItem(row, 2, new QTableWidgetItem(meta.value(QStringLiteral("unit")).toString()));
    }
    table->resizeColumnsToContents();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    for (int row = 0; row < table->rowCount(); ++row) {
        const auto* indexItem = table->item(row, 0);
        if (!indexItem) {
            continue;
        }
        const quint16 channel = static_cast<quint16>(indexItem->text().toUShort());
        const QString name = table->item(row, 1) ? table->item(row, 1)->text().trimmed() : QString();
        const QString unit = table->item(row, 2) ? table->item(row, 2)->text().trimmed() : QString();
        m_core->setChannelMetadata(channel, name, unit);
    }
    appendActivity(tr("Channel map updated"));
    statusBar()->showMessage(tr("Channel map updated"), 3000);
}

void MainWindow::scanSerialBaudRates()
{
    const QList<qint32> baudRates = {
        9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600, 1000000, 2000000
    };

    QString report;
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    if (ports.isEmpty()) {
        report = tr("No serial ports found.");
    }

    for (const QSerialPortInfo& info : ports) {
        report += tr("Port %1 (%2)\n").arg(info.portName(), info.description());
        for (qint32 baud : baudRates) {
            QSerialPort port(info);
            port.setBaudRate(baud);
            port.setDataBits(QSerialPort::Data8);
            port.setParity(QSerialPort::NoParity);
            port.setStopBits(QSerialPort::OneStop);
            port.setFlowControl(QSerialPort::NoFlowControl);
            if (port.open(QIODevice::ReadWrite)) {
                report += tr("  %1: open ok\n").arg(baud);
                port.close();
            } else {
                report += tr("  %1: %2\n").arg(baud).arg(port.errorString());
            }
        }
        report += QLatin1Char('\n');
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Serial Baud Scan"));
    dialog.resize(640, 480);
    auto* root = new QVBoxLayout(&dialog);
    auto* text = new QPlainTextEdit(&dialog);
    text->setReadOnly(true);
    text->setPlainText(report);
    root->addWidget(text, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(buttons);
    dialog.exec();
    appendActivity(tr("Serial baud scan finished"));
}

void MainWindow::detachPluginPages()
{
    if (m_visualTabs) {
        while (m_visualTabs->count() > 0) {
            QWidget* page = m_visualTabs->widget(0);
            m_visualTabs->removeTab(0);
            if (page) {
                page->setParent(nullptr);
            }
        }
    }

    if (m_controlTabs) {
        while (m_controlTabs->count() > 0) {
            QWidget* page = m_controlTabs->widget(0);
            m_controlTabs->removeTab(0);
            if (page) {
                page->setParent(nullptr);
            }
        }
    }
}

void MainWindow::showSidePage(int index)
{
    if (!m_sideStack) {
        return;
    }
    m_sideStack->setCurrentIndex(index);
    const bool configActive = index == 0;
    m_ui->sideTitle->setText(configActive ? tr("CONFIGURATION") : tr("PLUGINS"));
    setActivityButtonState(configActive ? m_activityConfigButton : m_activityPluginsButton);
}

void MainWindow::setActivityButtonState(QToolButton* activeButton)
{
    const QList<QToolButton*> buttons = {
        m_activityConfigButton,
        m_activityPluginsButton,
        m_activityDashboardButton,
        m_activityOutputButton
    };
    for (QToolButton* button : buttons) {
        if (!button) {
            continue;
        }
        button->setProperty("active", button == activeButton);
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
}

void MainWindow::updatePluginOverview()
{
    PluginManager* manager = m_core->pluginManager();
    const int physicalCount = manager->physicalPlugins().size();
    const int protocolCount = manager->protocolPlugins().size();
    const int visualCount = manager->visualPlugins().size();
    const int controlCount = manager->controlPlugins().size();

    m_pluginSummaryLabel->setText(tr("Plugins: %1 physical / %2 protocol / %3 visual / %4 control")
                                      .arg(physicalCount)
                                      .arg(protocolCount)
                                      .arg(visualCount)
                                      .arg(controlCount));

    m_pluginTree->clear();
    const auto addGroup = [this](const QString& title, const QStringList& names) {
        const int count = names.size();
        auto* group = new QTreeWidgetItem(m_pluginTree, {title, QString::number(count)});
        group->setExpanded(true);
        for (const QString& name : names) {
            new QTreeWidgetItem(group, {name, QString()});
        }
    };
    QStringList physicalNames;
    for (IPhysicalPlugin* plugin : manager->physicalPlugins()) {
        physicalNames.push_back(tr("%1  v%2").arg(plugin->name(), plugin->version()));
    }
    QStringList protocolNames;
    for (IProtocolPlugin* plugin : manager->protocolPlugins()) {
        protocolNames.push_back(tr("%1  v%2").arg(plugin->name(), plugin->version()));
    }
    QStringList visualNames;
    for (IVisualPlugin* plugin : manager->visualPlugins()) {
        visualNames.push_back(plugin->name());
    }
    QStringList controlNames;
    for (IControlPlugin* plugin : manager->controlPlugins()) {
        controlNames.push_back(plugin->name());
    }

    addGroup(tr("Physical"), physicalNames);
    addGroup(tr("Protocol"), protocolNames);
    addGroup(tr("Visual"), visualNames);
    addGroup(tr("Control"), controlNames);
    m_pluginTree->resizeColumnToContents(0);
}

void MainWindow::updateConfigPage()
{
    IPhysicalPlugin* plugin = selectedPhysical();
    if (!plugin || !m_configTable) {
        return;
    }

    const QString key = plugin->name();
    m_configTitleLabel->setText(tr("%1 Configuration").arg(key));
    const QVariantMap config = m_physicalConfigs.value(key, plugin->defaultConfig());

    m_configTable->clearContents();
    m_configTable->setRowCount(config.size());
    m_configTable->setColumnCount(2);
    m_configTable->setHorizontalHeaderLabels({tr("Key"), tr("Value")});
    int row = 0;
    for (auto it = config.constBegin(); it != config.constEnd(); ++it) {
        auto* keyItem = new QTableWidgetItem(it.key());
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        m_configTable->setItem(row, 0, keyItem);
        m_configTable->setItem(row, 1, new QTableWidgetItem(it.value().toString()));
        ++row;
    }
    m_configTable->resizeColumnToContents(0);
    m_configTable->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::appendActivity(const QString& message)
{
    if (!m_activityLog) {
        return;
    }
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    m_activityLog->appendPlainText(QStringLiteral("[%1] %2").arg(timestamp, message));
}

void MainWindow::updateTrafficCounters()
{
    m_rxCounterLabel->setText(tr("RX frames: %1").arg(m_rxFrames));
    m_txCounterLabel->setText(tr("TX commands: %1").arg(m_txCommands));
}

void MainWindow::setConnected(bool connected)
{
    m_statusLabel->setText(connected ? tr("Connected") : tr("Disconnected"));
    m_connectButton->setEnabled(!connected && m_physicalCombo->count() > 0 && m_protocolCombo->count() > 0);
    m_disconnectButton->setEnabled(connected);
    m_physicalCombo->setEnabled(!connected);
    m_protocolCombo->setEnabled(!connected);
    m_configButton->setEnabled(!connected && m_physicalCombo->count() > 0);
    m_configTable->setEnabled(!connected);
}

IPhysicalPlugin* MainWindow::selectedPhysical() const
{
    const QString name = m_physicalCombo->currentText();
    for (IPhysicalPlugin* plugin : m_core->pluginManager()->physicalPlugins()) {
        if (plugin->name() == name) {
            return plugin;
        }
    }
    return nullptr;
}

IProtocolPlugin* MainWindow::selectedProtocol() const
{
    const QString name = m_protocolCombo->currentText();
    for (IProtocolPlugin* plugin : m_core->pluginManager()->protocolPlugins()) {
        if (plugin->name() == name) {
            return plugin;
        }
    }
    return nullptr;
}

QString MainWindow::pluginRoot() const
{
    const QString appPlugins = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins"));
    if (QDir(appPlugins).exists()) {
        return appPlugins;
    }

    const QString cwdPlugins = QDir(QDir::currentPath()).filePath(QStringLiteral("plugins"));
    if (QDir(cwdPlugins).exists()) {
        return cwdPlugins;
    }

    return appPlugins;
}
