#include "SliderWidgetPlugin.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

SliderWidgetPlugin::SliderWidgetPlugin(QWidget* parent)
    : IControlPlugin(parent)
{
    setStyleSheet(QStringLiteral(
        "SliderWidgetPlugin { background: #252526; color: #cccccc; }"
        "QLabel { color: #cccccc; }"
        "QSpinBox, QComboBox { background: #1e1e1e; color: #cccccc; border: 1px solid #3c3c3c; border-radius: 2px; min-height: 28px; max-height: 28px; padding-left: 6px; }"
        "QToolButton { background: #0e639c; color: #ffffff; border: 1px solid #1177bb; border-radius: 2px; font-weight: 600; min-height: 28px; max-height: 28px; padding: 0 12px; }"
        "QToolButton:hover { background: #1177bb; }"
        "QSlider::groove:horizontal { height: 4px; background: #3c3c3c; }"
        "QSlider::handle:horizontal { width: 14px; margin: -5px 0; background: #0e639c; border-radius: 7px; }"));
    buildUi();
}

QString SliderWidgetPlugin::name() const
{
    return QStringLiteral("Slider Widget");
}

void SliderWidgetPlugin::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto* form = new QFormLayout;
    m_channel = new QSpinBox(this);
    m_channel->setRange(0, 65535);
    m_channel->setValue(1);
    m_channel->setFixedHeight(28);

    m_min = new QSpinBox(this);
    m_min->setRange(-1000000, 1000000);
    m_min->setValue(0);
    m_min->setFixedHeight(28);
    m_max = new QSpinBox(this);
    m_max->setRange(-1000000, 1000000);
    m_max->setValue(1000);
    m_max->setFixedHeight(28);
    m_value = new QSpinBox(this);
    m_value->setRange(m_min->value(), m_max->value());
    m_value->setFixedHeight(28);

    m_width = new QComboBox(this);
    m_width->addItem(tr("uint8"), 1);
    m_width->addItem(tr("int16 LE"), 2);
    m_width->addItem(tr("int32 LE"), 4);
    m_width->setFixedHeight(28);

    form->addRow(tr("Channel"), m_channel);
    form->addRow(tr("Min"), m_min);
    form->addRow(tr("Max"), m_max);
    form->addRow(tr("Value"), m_value);
    form->addRow(tr("Payload"), m_width);
    root->addLayout(form);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(m_min->value(), m_max->value());
    root->addWidget(m_slider);

    auto* buttons = new QHBoxLayout;
    m_sendButton = new QToolButton(this);
    m_sendButton->setText(tr("Send"));
    m_sendButton->setFixedHeight(28);
    m_sendButton->setMinimumWidth(72);
    m_liveButton = new QToolButton(this);
    m_liveButton->setText(tr("Live"));
    m_liveButton->setCheckable(true);
    m_liveButton->setFixedHeight(28);
    m_liveButton->setMinimumWidth(72);
    buttons->addWidget(m_sendButton);
    buttons->addWidget(m_liveButton);
    buttons->addStretch(1);
    root->addLayout(buttons);
    root->addStretch(1);

    const auto syncRange = [this]() {
        if (m_min->value() > m_max->value()) {
            m_max->setValue(m_min->value());
        }
        m_slider->setRange(m_min->value(), m_max->value());
        m_value->setRange(m_min->value(), m_max->value());
    };

    connect(m_min, qOverload<int>(&QSpinBox::valueChanged), this, syncRange);
    connect(m_max, qOverload<int>(&QSpinBox::valueChanged), this, syncRange);
    connect(m_slider, &QSlider::valueChanged, m_value, &QSpinBox::setValue);
    connect(m_value, qOverload<int>(&QSpinBox::valueChanged), m_slider, &QSlider::setValue);
    connect(m_value, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        if (m_liveButton->isChecked()) {
            emitValue(value);
        }
    });
    connect(m_sendButton, &QToolButton::clicked, this, [this]() {
        emitValue(m_value->value());
    });
}

void SliderWidgetPlugin::emitValue(int value)
{
    QVariantMap command;
    // channel/value 给协议插件做语义编码，bytes 则提供一个默认可直接发送的 SL 小帧。
    command.insert(QStringLiteral("channel"), m_channel->value());
    command.insert(QStringLiteral("value"), value);
    command.insert(QStringLiteral("bytes"), encodeValue(value));
    command.insert(QStringLiteral("source"), QStringLiteral("slider_widget"));
    emit commandGenerated(command);
}

QByteArray SliderWidgetPlugin::encodeValue(int value) const
{
    QByteArray bytes;
    // "SL" + channel(大端) + value(小端) 是本控件的简易默认协议，方便 Raw/串口快速联调。
    bytes.append(static_cast<char>(0x53));
    bytes.append(static_cast<char>(0x4c));
    bytes.append(static_cast<char>((m_channel->value() >> 8) & 0xff));
    bytes.append(static_cast<char>(m_channel->value() & 0xff));

    const int width = m_width->currentData().toInt();
    for (int i = 0; i < width; ++i) {
        bytes.append(static_cast<char>((value >> (i * 8)) & 0xff));
    }
    return bytes;
}
