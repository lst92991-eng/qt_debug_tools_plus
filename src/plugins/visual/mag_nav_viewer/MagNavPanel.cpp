#include "MagNavPanel.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <limits>

MagNavPanel::MagNavPanel(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(760, 520);
    initializeChannels();
}

void MagNavPanel::setFrame(const DataFrame& frame)
{
    QVector<ChannelSample> numeric;
    for (const ChannelSample& sample : frame.channels) {
        if (!std::isnan(sample.value)) {
            numeric.push_back(sample);
        }
    }
    if (numeric.isEmpty()) {
        return;
    }

    std::sort(numeric.begin(), numeric.end(), [](const ChannelSample& lhs, const ChannelSample& rhs) {
        return lhs.index < rhs.index;
    });

    m_values.clear();
    m_labels.clear();
    for (const ChannelSample& sample : numeric) {
        m_values.push_back(sample.value);
        m_labels.push_back(sample.name.isEmpty() ? QStringLiteral("CH%1").arg(m_labels.size()) : sample.name);
    }

    const bool hasBranchData = frame.attributes.contains(QStringLiteral("left_branch"))
        || frame.attributes.contains(QStringLiteral("straight"))
        || frame.attributes.contains(QStringLiteral("right_branch"));
    if (hasBranchData) {
        m_leftBranch = frame.attributes.value(QStringLiteral("left_branch"), m_leftBranch).toDouble();
        m_straight = frame.attributes.value(QStringLiteral("straight"), m_straight).toDouble();
        m_rightBranch = frame.attributes.value(QStringLiteral("right_branch"), m_rightBranch).toDouble();
        m_hasBranchData = true;
    }
    m_threshold = frame.attributes.value(QStringLiteral("threshold"), m_threshold).toDouble();
    if (frame.attributes.contains(QStringLiteral("can_id"))) {
        m_source = QStringLiteral("CAN 0x%1").arg(frame.attributes.value(QStringLiteral("can_id")).toUInt(), 0, 16).toUpper();
    } else if (frame.attributes.contains(QStringLiteral("modbus_station"))) {
        m_source = QStringLiteral("Modbus station %1").arg(frame.attributes.value(QStringLiteral("modbus_station")).toInt());
    }
    m_lastTimestampUs = frame.timestamp_us;
    m_ageTimer.restart();
    update();
}

void MagNavPanel::setThreshold(double threshold)
{
    m_threshold = threshold;
    update();
}

QSize MagNavPanel::minimumSizeHint() const
{
    return QSize(760, 520);
}

void MagNavPanel::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(30, 30, 30));

    const QRect area = rect().adjusted(18, 18, -18, -18);
    painter.setPen(QPen(QColor(60, 60, 60), 1));
    painter.setBrush(QColor(37, 37, 38));
    painter.drawRoundedRect(area, 8, 8);

    const QRect header(area.left() + 18, area.top() + 14, area.width() - 36, 54);
    painter.setPen(QColor(245, 245, 245));
    painter.setFont(QFont(painter.font().family(), 18, QFont::Bold));
    painter.drawText(header, Qt::AlignVCenter | Qt::AlignLeft, tr("MagNav Sensor Monitor"));

    const QString status = isOnline() ? tr("ONLINE") : tr("TIMEOUT");
    const QColor statusColor = isOnline() ? QColor(44, 172, 102) : QColor(214, 75, 75);
    painter.setFont(QFont(painter.font().family(), 10, QFont::Bold));
    painter.setBrush(statusColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(QRect(header.right() - 104, header.top() + 10, 92, 28), 14, 14);
    painter.setPen(Qt::white);
    painter.drawText(QRect(header.right() - 104, header.top() + 10, 92, 28), Qt::AlignCenter, status);

    const QRect branchRect(area.left() + 22, area.top() + 84, area.width() - 44, 46);
    const QVector<QPair<QString, double>> branches = {
        {tr("Left branch"), m_leftBranch},
        {tr("Straight"), m_straight},
        {tr("Right branch"), m_rightBranch},
    };
    int bestBranch = -1;
    double bestScore = std::numeric_limits<double>::infinity();
    if (m_hasBranchData) {
        for (int i = 0; i < branches.size(); ++i) {
            const double score = std::abs(branches.at(i).second);
            if (score < bestScore) {
                bestScore = score;
                bestBranch = i;
            }
        }
    }
    const int branchWidth = branchRect.width() / 3;
    for (int i = 0; i < branches.size(); ++i) {
        const QRect chip(branchRect.left() + i * branchWidth + 16, branchRect.top() + 4, branchWidth - 32, 34);
        const bool active = i == bestBranch;
        painter.setBrush(active ? QColor(14, 99, 156) : QColor(45, 45, 48));
        painter.setPen(QPen(active ? QColor(17, 119, 187) : QColor(76, 76, 76), 1));
        painter.drawRoundedRect(chip, 17, 17);
        painter.setPen(active ? Qt::white : QColor(212, 212, 212));
        painter.setFont(QFont(painter.font().family(), 11, QFont::Bold));
        const QString text = m_hasBranchData
            ? QStringLiteral("%1  %2").arg(branches.at(i).first).arg(branches.at(i).second, 0, 'f', 0)
            : branches.at(i).first;
        painter.drawText(chip, Qt::AlignCenter, text);
    }

    const QRect sensor(area.left() + 50, area.top() + 150, area.width() - 100, 112);
    painter.setBrush(QColor(24, 25, 28));
    painter.setPen(QPen(QColor(70, 70, 72), 2));
    painter.drawRoundedRect(sensor, 6, 6);
    painter.setBrush(QColor(28, 133, 123));
    painter.setPen(Qt::NoPen);
    painter.drawRect(sensor.adjusted(8, 34, -8, -34));
    painter.setPen(Qt::white);
    painter.setFont(QFont(painter.font().family(), 14, QFont::Bold));
    painter.drawText(sensor.adjusted(0, 28, 0, -26), Qt::AlignCenter, tr("AGV Magnetic Navigation Sensor"));

    const int count = std::max(1, static_cast<int>(m_values.size()));
    const int ledSpacing = sensor.width() / (count + 1);
    for (int i = 0; i < count; ++i) {
        const double value = i < m_values.size() ? m_values.at(i) : 0.0;
        const bool lit = value >= m_threshold;
        const bool peak = i == maxChannel();
        const QPoint center(sensor.left() + ledSpacing * (i + 1), sensor.bottom() - 24);
        painter.setPen(QPen(QColor(21, 25, 29), 2));
        painter.setBrush(peak ? QColor(255, 218, 86) : (lit ? QColor(115, 221, 64) : QColor(101, 111, 119)));
        painter.drawEllipse(center, 8, 8);
    }

    const QRect chart(area.left() + 32, area.top() + 286, area.width() - 64, area.bottom() - area.top() - 314);
    painter.setPen(QColor(60, 60, 60));
    painter.setBrush(QColor(30, 30, 30));
    painter.drawRoundedRect(chart, 5, 5);

    const int gap = 6;
    const int barWidth = std::max(18, (chart.width() - gap * (count + 1)) / count);
    for (int i = 0; i < count; ++i) {
        const double value = i < m_values.size() ? m_values.at(i) : 0.0;
        const int x = chart.left() + gap + i * (barWidth + gap);
        const int barMaxHeight = chart.height() - 48;
        const int h = static_cast<int>(barMaxHeight * normalizedValue(value));
        const QRect rail(x, chart.top() + 16, barWidth, barMaxHeight);
        painter.setBrush(QColor(45, 45, 48));
        painter.setPen(QPen(QColor(76, 76, 76), 1));
        painter.drawRoundedRect(rail, 4, 4);
        QRect fill(rail.left() + 1, rail.bottom() - h + 1, rail.width() - 2, std::max(2, h - 1));
        painter.setPen(Qt::NoPen);
        painter.setBrush(i == maxChannel() ? QColor(204, 167, 0) : QColor(55, 148, 255));
        painter.drawRoundedRect(fill, 3, 3);
        painter.setPen(QColor(220, 220, 220));
        painter.setFont(QFont(painter.font().family(), 9, QFont::Bold));
        painter.drawText(rail, Qt::AlignCenter, QString::number(value, 'f', 0));
        painter.setFont(QFont(painter.font().family(), 8));
        painter.setPen(QColor(156, 156, 156));
        painter.drawText(QRect(x - 2, chart.bottom() - 25, barWidth + 4, 18), Qt::AlignCenter, QStringLiteral("%1").arg(i));
    }
}

void MagNavPanel::initializeChannels()
{
    constexpr int channelCount = 8;
    m_values.fill(0.0, channelCount);
    m_labels.clear();
    for (int i = 0; i < m_values.size(); ++i) {
        m_labels.push_back(QStringLiteral("MAG%1").arg(i));
    }
}

double MagNavPanel::normalizedValue(double value) const
{
    const double scale = std::max(100.0, maxValue());
    return std::clamp(value / scale, 0.0, 1.0);
}

double MagNavPanel::centerOffset() const
{
    double weighted = 0.0;
    double sum = 0.0;
    for (int i = 0; i < m_values.size(); ++i) {
        weighted += i * m_values.at(i);
        sum += m_values.at(i);
    }
    if (sum <= 0.0) {
        return 0.0;
    }
    return weighted / sum - (m_values.size() - 1) / 2.0;
}

int MagNavPanel::maxChannel() const
{
    if (m_values.isEmpty() || maxValue() <= 0.0) {
        return -1;
    }
    return static_cast<int>(std::distance(m_values.begin(), std::max_element(m_values.begin(), m_values.end())));
}

double MagNavPanel::maxValue() const
{
    if (m_values.isEmpty()) {
        return 0.0;
    }
    return *std::max_element(m_values.begin(), m_values.end());
}

bool MagNavPanel::isOnline() const
{
    return m_ageTimer.isValid() && m_ageTimer.elapsed() < 3000;
}
