/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "spacerwidget.h"
#include "toolbutton.h"
#include <QPainter>
#include <QLinearGradient>

static int fixedWidth=0;

SpacerWidget::SpacerWidget(QWidget *parent)
    : QWidget(parent)
{
    if (0==fixedWidth) {
        ToolButton tb(parent);
        tb.ensurePolished();
        fixedWidth=tb.sizeHint().width()*0.75;
    }
    setFixedWidth(fixedWidth);
}

void SpacerWidget::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);
    QPainter p(this);
    QColor col(palette().text().color());
    col.setAlphaF(0.333);
    QPoint start(width()/2, 0);
    QPoint end(width()/2, height()-1);
    QLinearGradient grad(start, end);
    grad.setColorAt(0.4, col);
    grad.setColorAt(0.6, col);
    col.setAlphaF(0);
    grad.setColorAt(0.1, col);
    grad.setColorAt(0.9, col);
    p.setPen(QPen(QBrush(grad), 1));
    p.drawLine(start, end);
}