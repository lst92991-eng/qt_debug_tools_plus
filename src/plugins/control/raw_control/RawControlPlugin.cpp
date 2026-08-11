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
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int ProjectHeaderSize = 7;
constexpr int SessionOffset = ProjectHeaderSize + 4;
constexpr quint32 AgvHeartbeatId = 0x101;

quint8 byteAt(const QByteArray& data, int offset)
{
    return static_cast<quint8>(data.at(offset));
}

quint32 projectFrameId(const QByteArray& frame)
{
    if (frame.size() < ProjectHeaderSize
        || byteAt(frame, 0) != 0xca || byteAt(frame, 1) != 0xfd) {
        return 0xffffffffU;
    }
    return (static_cast<quint32>(byteAt(frame, 2)) << 24)
        | (static_cast<quint32>(byteAt(frame, 3)) << 16)
        | (static_cast<quint32>(byteAt(frame, 4)) << 8)
        | static_cast<quint32>(byteAt(frame, 5));
}

bool isAgvCommandId(quint32 canId)
{
    return canId == 0x080 || canId == 0x091 || canId == 0x101
        || canId == 0x111 || canId == 0x121;
}

quint16 crc16Ccitt(const QByteArray& data, int offset, int length)
{
    quint16 crc = 0xffff;
    for (int index = 0; index < length; ++index) {
        crc ^= static_cast<quint16>(byteAt(data, offset + index)) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) != 0
                ? static_cast<quint16>((crc << 1) ^ 0x1021)
                : static_cast<quint16>(crc << 1);
        }
    }
    return crc;
}

void putLe16(QByteArray* data, int offset, quint16 value)
{
    (*data)[offset] = static_cast<char>(value & 0xff);
    (*data)[offset + 1] = static_cast<char>((value >> 8) & 0xff);
}

void putLe32(QByteArray* data, int offset, quint32 value)
{
    (*data)[offset] = static_cast<char>(value & 0xff);
    (*data)[offset + 1] = static_cast<char>((value >> 8) & 0xff);
    (*data)[offset + 2] = static_cast<char>((value >> 16) & 0xff);
    (*data)[offset + 3] = static_cast<char>((value >> 24) & 0xff);
}

bool updateAgvSession(QByteArray* frame, quint32 session)
{
    if (!frame || !isAgvCommandId(projectFrameId(*frame))) {
        return false;
    }
    const int payloadSize = byteAt(*frame, 6);
    if (payloadSize < 16 || frame->size() != ProjectHeaderSize + payloadSize) {
        return false;
    }

    putLe32(frame, SessionOffset, session);
    const int crcOffset = ProjectHeaderSize + payloadSize - 2;
    putLe16(frame, crcOffset,
            crc16Ccitt(*frame, ProjectHeaderSize, payloadSize - 2));
    return true;
}

QString displayHex(const QByteArray& data)
{
    return QString::fromLatin1(data.toHex(' ').toUpper());
}

} // namespace

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
    auto* sendPresetButton = new QPushButton(tr("Send Selected"), this);
    addButton->setFixedHeight(28);
    addButton->setMinimumWidth(64);
    removeButton->setFixedHeight(28);
    removeButton->setMinimumWidth(76);
    sendPresetButton->setFixedHeight(28);
    sendPresetButton->setMinimumWidth(112);
    sendPresetButton->setEnabled(false);
    presetHeader->addWidget(sendPresetButton);
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
            beginAgvSession();
            const QString normalized = normalizeHex(m_input->text());
            if (!isValidHex(normalized)) {
                m_periodicButton->setChecked(false);
                return;
            }
            // Lock the periodic source when the timer starts. The editable input
            // may then be used to prepare one-shot commands without changing the
            // frame emitted by an already-running safety heartbeat.
            m_periodicHex = normalized;
            m_periodicTimer.start(m_interval->value());
        } else {
            m_periodicTimer.stop();
            m_periodicHex.clear();
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
    connect(sendPresetButton, &QPushButton::clicked, this, [this]() {
        sendPreset(m_presetsList->currentItem());
    });
    connect(m_presetsList, &QListWidget::currentItemChanged, this,
            [sendPresetButton](QListWidgetItem* current) {
                sendPresetButton->setEnabled(current != nullptr);
            });
    connect(m_presetsList, &QListWidget::itemDoubleClicked,
            this, &RawControlPlugin::sendPreset);
    connect(m_presetsList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        // While a periodic frame is active, presets are one-shot commands only.
        // Use their context-menu action without replacing the visible heartbeat.
        if (item && !m_periodicTimer.isActive()) {
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
    const QString normalized = periodic ? m_periodicHex : normalizeHex(m_input->text());
    if (!isValidHex(normalized)) {
        if (periodic) {
            m_periodicTimer.stop();
            m_periodicHex.clear();
            m_periodicButton->setChecked(false);
        } else {
            validateInput();
        }
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
            {QStringLiteral("Heartbeat 100ms"), QStringLiteral("CA FD 00 00 01 01 10 01 00 01 00 78 56 34 12 00 00 00 00 00 00 32 0C")},
            {QStringLiteral("Clear Fault"), QStringLiteral("CA FD 00 00 00 91 10 01 02 64 00 78 56 34 12 00 00 00 00 00 00 B5 3B")},
            {QStringLiteral("Move 300mm"), QStringLiteral("CA FD 00 00 01 21 20 01 00 65 00 78 56 34 12 2C 01 00 00 00 00 00 00 00 00 00 00 32 00 00 00 00 00 00 00 00 00 C5 CE")},
            {QStringLiteral("Move 500mm"), QStringLiteral("CA FD 00 00 01 21 20 01 00 65 00 78 56 34 12 F4 01 00 00 00 00 00 00 00 00 00 00 32 00 00 00 00 00 00 00 00 00 EF 42")},
            {QStringLiteral("Move 1000mm"), QStringLiteral("CA FD 00 00 01 21 20 01 00 65 00 78 56 34 12 E8 03 00 00 00 00 00 00 00 00 00 00 32 00 00 00 00 00 00 00 00 00 86 BD")},
            {QStringLiteral("Stop"), QStringLiteral("CA FD 00 00 00 91 10 01 01 66 00 78 56 34 12 00 00 00 00 00 00 FA F2")},
            {QStringLiteral("Emergency Stop"), QStringLiteral("CA FD 00 00 00 80 10 01 01 67 00 78 56 34 12 00 00 00 00 00 00 8F F1")}
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

    const QString normalized = normalizeHex(item->data(Qt::UserRole).toString());
    if (!isValidHex(normalized)) {
        return;
    }

    // Quick Send must not replace the live input: that field is also the source
    // for periodic transmission (for example, a safety heartbeat). Replacing it
    // here would cause a one-shot command to be retransmitted on every timer tick.
    QVariantMap command;
    command.insert(QStringLiteral("bytes"), parseHex(normalized));
    command.insert(QStringLiteral("hex"), normalized);
    addHistory(normalized);
    emit commandGenerated(command);
}

bool RawControlPlugin::beginAgvSession()
{
    QByteArray heartbeat = parseHex(m_input->text());
    if (projectFrameId(heartbeat) != AgvHeartbeatId) {
        return false;
    }

    quint32 session = QRandomGenerator::global()->generate();
    if (session == 0) {
        session = 1;
    }

    // 每次启动心跳都建立新会话，使固定快捷序号可以安全重复使用。
    updateAgvSession(&heartbeat, session);
    m_input->setText(displayHex(heartbeat));
    for (Preset& preset : m_presets) {
        QByteArray frame = parseHex(preset.hex);
        if (updateAgvSession(&frame, session)) {
            preset.hex = displayHex(frame);
        }
    }
    refreshPresetList();
    return true;
}
