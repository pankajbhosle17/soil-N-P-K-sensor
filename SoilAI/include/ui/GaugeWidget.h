#pragma once
#include <QWidget>
#include <QString>
#include <QColor>

// ============================================================
// GaugeWidget
// Self-painted circular indicator (QPainter, no external libs).
// Shows a value within [min,max] as an arc, plus a label and unit.
// Reused by every Dashboard sensor card (pH, N, P, K, Temp, Humidity).
// ============================================================
class GaugeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit GaugeWidget(QString label, QString unit, double minVal, double maxVal,
                          QColor accent = QColor("#5fe3a1"), QWidget *parent = nullptr);

    QSize sizeHint() const override { return QSize(140, 160); }

public slots:
    void setValue(double v);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_label;
    QString m_unit;
    double m_min, m_max;
    double m_value;
    QColor m_accent;
};
