#include "RawControlPlugin.h"

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <algorithm>

RawControlPlugin::RawControlPlugin(QWidget* parent)
    : IControlPlugin(parent)
{
    setStyleSheet(QStringLiteral(
        "RawControlPlugin { background: #252526; color: #cccccc; }"
        "QLabel, QToolButton { color: #cccccc; }"
        "QLineEdit, QSpinBox, QListWidget { background: #1e1e1e; color: #cccccc; border: 1px solid #3c3c3c; border-radius: 2px; min-height: 28px; }"
        "QLineEdit, QSpinBox { max-height: 28px; padding-left: 6px; }"
        "QPushButton, QToolButton { background: #0e639c; color: #ffffff; border: 1px solid #1177bb; border-radius: 2px; font-weight: 600; min-height: 28px; max-height: 28px; padding: 0 12px; }"
        "QPushButton:hover, QToolButton:hover { background: #1177bb; }"
        "QListWidget::item:selected { background: #094771; color: #ffffff; }"));
    loadPresets();
    buildUi();
    validateInput();
}

QString RawControlPlugin::name() const
{
    return QStringLiteral("Raw Control");
}

bool RawControlPlugin::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        // 命令行式历史浏览：上/下键只影响输入框，不抢其它控件的键盘事件。
        if (keyEvent->key() == Qt::Key_Up && !m_history.isEmpty()) {
            if (m_historyIndex < 0) {
                m_historyIndex = m_history.size() - 1;
            } else {
                m_historyIndex = std::max(0, m_historyIndex - 1);
            }
            m_input->setText(m_history.at(m_historyIndex));
            return true;
        }
        if (keyEvent->key() == Qt::Key_Down && !m_history.isEmpty()) {
            if (m_historyIndex >= 0 && m_historyIndex < m_history.size() - 1) {
                ++m_historyIndex;
                m_input->setText(m_history.at(m_historyIndex));
            } else {
                m_historyIndex = -1;
                m_input->clear();
            }
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void RawControlPlugin::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto* inputRow = new QHBoxLayout;
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("A5 01 7F"));
    m_input->installEventFilter(this);
    m_input->setFixedHeight(28);

    m_sendButton = new QToolButton(this);
    m_sendButton->setText(tr("Send"));
    m_sendButton->setFixedHeight(28);
    m_sendButton->setMinimumWidth(72);

    inputRow->addWidget(new QLabel(tr("HEX:"), this));
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_sendButton);
    root->addLayout(inputRow);

    auto* periodicRow = new QHBoxLayout;
    m_periodicButton = new QToolButton(this);
    m_periodicButton->setText(tr("Periodic"));
    m_periodicButton->setCheckable(true);
    m_periodicButton->setFixedHeight(28);
    m_periodicButton->setMinimumWidth(88);
    m_interval = new QSpinBox(this);
    m_interval->setRange(1, 600000);
    m_interval->setValue(100);
    m_interval->setSuffix(tr(" ms"));
    m_interval->setFixedHeight(28);
    m_interval->setMinimumWidth(96);
    periodicRow->addWidget(m_periodicButton);
    periodicRow->addWidget(m_interval);
    periodicRow->addStretch(1);
    root->addLayout(periodicRow);

    auto* presetHeader = new QHBoxLayout;
    presetHeader->addWidget(new QLabel(tr("Quick Send"), this));
    presetHeader->addStretch(1);
    auto* addButton = new QPushButton(tr("Add"), this);
    auto* removeButton = new QPushButton(tr("Remove"), this);
    addButton->setFixedHeight(28);
    addButton->setMinimumWidth(64);
    removeButton->setFixedHeight(28);
    removeButton->setMinimumWidth(76);
    presetHeader->addWidget(addButton);
    presetHeader->addWidget(removeButton);
    root->addLayout(presetHeader);

    m_presetsList = new QListWidget(this);
    m_presetsList->setContextMenuPolicy(Qt::CustomContextMenu);
    root->addWidget(m_presetsList, 1);
    refreshPresetList();

    connect(m_input, &QLineEdit::textChanged, this, &RawControlPlugin::validateInput);
    connect(m_input, &QLineEdit::returnPressed, this, [this]() { sendCurrent(false); });
    connect(m_sendButton, &QToolButton::clicked, this, [this]() { sendCurrent(false); });
    connect(m_periodicButton, &QToolButton::toggled, this, [this](bool checked) {
        if (checked) {
            if (!isValidHex(m_input->text())) {
                m_periodicButton->setChecked(false);
                return;
            }
            m_periodicTimer.start(m_interval->value());
        } else {
            m_periodicTimer.stop();
        }
    });
    connect(&m_periodicTimer, &QTimer::timeout, this, [this]() { sendCurrent(true); });
    connect(m_interval, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        if (m_periodicTimer.isActive()) {
            m_periodicTimer.start(value);
        }
    });
    connect(addButton, &QPushButton::clicked, this, &RawControlPlugin::addPreset);
    connect(removeButton, &QPushButton::clicked, this, &RawControlPlugin::removeSelectedPreset);
    connect(m_presetsList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item) {
            m_input->setText(item->data(Qt::UserRole).toString());
        }
    });
    connect(m_presetsList, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* item = m_presetsList->itemAt(pos);
        if (!item) {
            return;
        }
        QMenu menu(this);
        QAction* sendAction = menu.addAction(tr("Send Now"));
        if (menu.exec(m_presetsList->viewport()->mapToGlobal(pos)) == sendAction) {
            sendPreset(item);
        }
    });
}

void RawControlPlugin::validateInput()
{
    const bool empty = m_input->text().trimmed().isEmpty();
    const bool valid = !empty && isValidHex(m_input->text());
    m_sendButton->setEnabled(valid);
    if (empty) {
        m_input->setStyleSheet(QString());
    } else if (!valid) {
        m_input->setStyleSheet(QStringLiteral("QLineEdit { border: 1px solid #f14c4c; background: #2a1f1f; color: #f5f5f5; min-height: 28px; max-height: 28px; padding-left: 6px; }"));
    } else {
        m_input->setStyleSheet(QStringLiteral("QLineEdit { border: 1px solid #2ea043; background: #1e1e1e; color: #cccccc; min-height: 28px; max-height: 28px; padding-left: 6px; }"));
    }
}

void RawControlPlugin::sendCurrent(bool periodic)
{
    const QString normalized = normalizeHex(m_input->text());
    if (!isValidHex(normalized)) {
        validateInput();
        return;
    }

    QVariantMap command;
    // 同时放 bytes 和 hex：协议插件优先用 bytes，日志/会话仍能展示用户输入的十六进制文本。
    command.insert(QStringLiteral("bytes"), parseHex(normalized));
    command.insert(QStringLiteral("hex"), normalized);
    if (periodic) {
        command.insert(QStringLiteral("period_ms"), m_interval->value());
    } else {
        addHistory(normalized);
    }
    emit commandGenerated(command);
}

void RawControlPlugin::addHistory(const QString& hex)
{
    if (hex.isEmpty()) {
        return;
    }

    m_history.removeAll(hex);
    m_history.push_back(hex);
    while (m_history.size() > 100) {
        m_history.removeFirst();
    }
    m_historyIndex = -1;
}

QByteArray RawControlPlugin::parseHex(const QString& text) const
{
    const QString compact = normalizeHex(text);
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

QString RawControlPlugin::normalizeHex(const QString& text) const
{
    QString compact = text;
    compact.remove(QRegularExpression(QStringLiteral("[\\s,;:_-]")));
    return compact.toUpper();
}

bool RawControlPlugin::isValidHex(const QString& text) const
{
    const QString compact = normalizeHex(text);
    if (compact.isEmpty() || compact.size() % 2 != 0) {
        return false;
    }
    static const QRegularExpression rx(QStringLiteral("^[0-9A-F]+$"));
    return rx.match(compact).hasMatch();
}

void RawControlPlugin::loadPresets()
{
    QFile file(presetsPath());
    if (!file.open(QIODevice::ReadOnly)) {
        // 首次启动给两个无害示例，帮助用户看到预设列表的交互形态。
        m_presets = {
            {QStringLiteral("Ping"), QStringLiteral("A5 00 01 FF")},
            {QStringLiteral("Zero"), QStringLiteral("00")}
        };
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonArray array = doc.array();
    for (const QJsonValue& value : array) {
        const QJsonObject obj = value.toObject();
        const QString hex = normalizeHex(obj.value(QStringLiteral("hex")).toString());
        if (!isValidHex(hex)) {
            continue;
        }
        m_presets.push_back({obj.value(QStringLiteral("name")).toString(hex), hex});
    }
}

void RawControlPlugin::savePresets() const
{
    const QString path = presetsPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray array;
    for (const Preset& preset : m_presets) {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), preset.name);
        obj.insert(QStringLiteral("hex"), preset.hex);
        array.push_back(obj);
    }

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    }
}

void RawControlPlugin::refreshPresetList()
{
    if (!m_presetsList) {
        return;
    }

    m_presetsList->clear();
    for (const Preset& preset : m_presets) {
        auto* item = new QListWidgetItem(QStringLiteral("%1    %2").arg(preset.name, preset.hex));
        item->setData(Qt::UserRole, preset.hex);
        m_presetsList->addItem(item);
    }
}

QString RawControlPlugin::presetsPath() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    // 预设是用户偏好，不随工程构建输出走，避免重编译/清 build 时丢失。
    return QDir(base).filePath(QStringLiteral("raw_control_presets.json"));
}

void RawControlPlugin::addPreset()
{
    const QString hex = normalizeHex(m_input->text());
    if (!isValidHex(hex)) {
        validateInput();
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Preset Name"), tr("Name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }

    m_presets.push_back({name.trimmed(), hex});
    savePresets();
    refreshPresetList();
}

void RawControlPlugin::removeSelectedPreset()
{
    const int row = m_presetsList->currentRow();
    if (row < 0 || row >= m_presets.size()) {
        return;
    }

    m_presets.removeAt(row);
    savePresets();
    refreshPresetList();
}

void RawControlPlugin::sendPreset(QListWidgetItem* item)
{
    if (!item) {
        return;
    }
    m_input->setText(item->data(Qt::UserRole).toString());
    sendCurrent(false);
}
