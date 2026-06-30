#include "GaugeWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

GaugeWidget::GaugeWidget(QString label, QString unit, double minVal, double maxVal,
                          QColor accent, QWidget *parent)
    : QWidget(parent), m_label(std::move(label)), m_unit(std::move(unit)),
      m_min(minVal), m_max(maxVal), m_value(minVal), m_accent(accent)
{
    setMinimumSize(120, 140);
}

void GaugeWidget::setValue(double v)
{
    m_value = std::clamp(v, m_min, m_max);
    update(); // schedules repaint
}

void GaugeWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int side = std::min(width(), height() - 30);
    const QRectF arcRect((width() - side) / 2.0, 8, side - 16, side - 16);

    // Background track
    QPen trackPen(QColor("#243140"), 10, Qt::SolidLine, Qt::RoundCap);
    p.setPen(trackPen);
    p.drawArc(arcRect, 225 * 16, -270 * 16);

    // Value arc
    const double fraction = (m_max > m_min) ? (m_value - m_min) / (m_max - m_min) : 0.0;
    QPen valuePen(m_accent, 10, Qt::SolidLine, Qt::RoundCap);
    p.setPen(valuePen);
    p.drawArc(arcRect, 225 * 16, static_cast<int>(-270 * 16 * fraction));

    // Center value text
    p.setPen(QColor("#e8f0f5"));
    QFont valueFont = p.font();
    valueFont.setPointSize(14);
    valueFont.setBold(true);
    p.setFont(valueFont);
    const QString valueText = QString::number(m_value, 'f', 1) + " " + m_unit;
    p.drawText(arcRect, Qt::AlignCenter, valueText);

    // Label below the arc
    p.setPen(QColor("#8aa0b2"));
    QFont labelFont = p.font();
    labelFont.setPointSize(9);
    labelFont.setBold(false);
    p.setFont(labelFont);
    QRectF labelRect(0, arcRect.bottom() + 2, width(), 20);
    p.drawText(labelRect, Qt::AlignCenter, m_label);
}
