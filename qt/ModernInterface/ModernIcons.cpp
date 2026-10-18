/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ModernInterface.h"
#include <QPainter>
#include <QPainterPath>
QIcon ModernInterface::icon(const QString& name, const QColor& color)
{
    QPixmap pixmap(72, 72);
    pixmap.setDevicePixelRatio(3);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.55, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QString key = name.toLower();
    if(key.contains("visual") || key.contains("map"))
    {
        p.drawRoundedRect(QRectF(3, 3, 18, 18), 3, 3);
        p.drawLine(9, 3, 9, 21);
        p.drawLine(15, 3, 15, 21);
        p.drawLine(3, 9, 21, 9);
        p.drawLine(3, 15, 21, 15);
    }
    else if(key.contains("effects"))
    {
        QPainterPath path;
        path.moveTo(12, 2);
        path.lineTo(14.5, 9.5);
        path.lineTo(22, 12);
        path.lineTo(14.5, 14.5);
        path.lineTo(12, 22);
        path.lineTo(9.5, 14.5);
        path.lineTo(2, 12);
        path.lineTo(9.5, 9.5);
        path.closeSubpath();
        p.drawPath(path);
    }
    else if((key.contains("light") || key.contains("govee") || key.contains("yeelight"))
        && !key.contains("lenovo"))
    {
        p.drawEllipse(QRectF(6, 2, 12, 12));
        p.drawLine(9, 14, 9, 18);
        p.drawLine(15, 14, 15, 18);
        p.drawLine(9, 18, 15, 18);
        p.drawLine(10, 21, 14, 21);
    }
    else if(key.contains("settings") || key.contains("appearance"))
    {
        p.drawLine(5, 3, 5, 21);
        p.drawLine(12, 3, 12, 21);
        p.drawLine(19, 3, 19, 21);
        p.setBrush(QColor("#26262d"));
        p.drawEllipse(QPointF(5, 8), 2.5, 2.5);
        p.drawEllipse(QPointF(12, 16), 2.5, 2.5);
        p.drawEllipse(QPointF(19, 10), 2.5, 2.5);
    }
    else if(key.contains("sdk") || key.contains("server") || key.contains("client"))
    {
        p.drawRoundedRect(QRectF(3, 3, 18, 7), 2, 2);
        p.drawRoundedRect(QRectF(3, 14, 18, 7), 2, 2);
        p.drawPoint(6, 6);
        p.drawPoint(6, 17);
        p.drawLine(16, 6, 18, 6);
        p.drawLine(16, 17, 18, 17);
    }
    else if(key.contains("information") || key.contains("about"))
    {
        p.drawEllipse(QRectF(3, 3, 18, 18));
        p.drawPoint(12, 7);
        p.drawLine(12, 11, 12, 17);
    }
    else if(key.contains("mouse") || key.contains("basilisk"))
    {
        p.drawRoundedRect(QRectF(6, 2, 12, 20), 6, 6);
        p.drawLine(12, 3, 12, 9);
        p.drawLine(6, 10, 18, 10);
    }
    else if(key.contains("keyboard") || key.contains("blackwidow") || key.contains("lenovo"))
    {
        p.drawRoundedRect(QRectF(2, 5, 20, 14), 2, 2);
        for(int y : { 9, 12 })
            for(int x : { 6, 10, 14, 18 })
                p.drawPoint(x, y);
        p.drawLine(8, 16, 16, 16);
    }
    else
    {
        p.drawRoundedRect(QRectF(3, 3, 18, 14), 2, 2);
        p.drawLine(12, 17, 12, 21);
        p.drawLine(8, 21, 16, 21);
    }
    return QIcon(pixmap);
}
