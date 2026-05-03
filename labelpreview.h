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

#ifndef LABEL_PREVIEW_H
#define LABEL_PREVIEW_H

#include <QSize>
#include <QPoint>
#include <QPixmap>
#include <QLabel>
#include <QRubberBand>

class LabelPreview : public QLabel
{
Q_OBJECT
    QRubberBand* rubberBand = nullptr;
    QPoint ruberBandStart;

public:
    LabelPreview(QWidget* parent);
    ~LabelPreview() = default;

    void mousePressEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;

signals:
    void rubberBandChanged(const QRect&);    
};

#endif // LABEL_PREVIEW_H
