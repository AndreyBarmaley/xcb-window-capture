/***************************************************************************
 *   Copyright © 2026 by Andrey Afletdinov <public.irkutsk@gmail.com>      *
 *                                                                         *
 *   Part of the XcbWindowCapture                                          *
 *   https://github.com/AndreyBarmaley/xcb-window-capture                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <QMouseEvent>

#include "labelpreview.h"

LabelPreview::LabelPreview(QWidget* parent) : QLabel(parent)
{
    setMouseTracking(true);
    rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
}

void LabelPreview::mousePressEvent(QMouseEvent* ev)
{
    if(ev->button() == Qt::LeftButton)
    {
        ruberBandStart = ev->pos();
        rubberBand->hide();
    }
}

void LabelPreview::mouseReleaseEvent(QMouseEvent* ev)
{
    if(ev->button() == Qt::LeftButton)
    {
        rubberBand->setGeometry(QRect(ruberBandStart, ev->pos()));
        rubberBand->hide();
        emit rubberBandChanged(rubberBand->geometry());
    }
}

void LabelPreview::mouseMoveEvent(QMouseEvent* ev)
{
    if(ev->buttons() & Qt::LeftButton)
    {
        rubberBand->setGeometry(QRect(ruberBandStart, ev->pos()));
        rubberBand->show();
    }
}
