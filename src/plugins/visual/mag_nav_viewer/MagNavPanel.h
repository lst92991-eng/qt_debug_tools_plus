#pragma once

#include "sdk/DataFrame.h"

#include <QElapsedTimer>
#include <QStringList>
#include <QWidget>

class MagNavPanel : public QWidget {
    Q_OBJECT
public:
    explicit MagNavPanel(QWidget* parent = nullptr);

    void setFrame(const DataFrame& frame);
    void setThreshold(double threshold);
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void initializeChannels();
    double normalizedValue(double value) const;
    double centerOffset() const;
    int maxChannel() const;
    double maxValue() const;
    bool isOnline() const;

    QVector<double> m_values;
    QStringList m_labels;
    double m_threshold = 25.0;
    double m_leftBranch = 0.0;
    double m_straight = 0.0;
    double m_rightBranch = 0.0;
    bool m_hasBranchData = false;
    QString m_source = QStringLiteral("Waiting for data");
    qint64 m_lastTimestampUs = 0;
    QElapsedTimer m_ageTimer;
};
