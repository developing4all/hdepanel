/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * Copyright: 2015-2016 Haydar Alkaduhimi
 * Authors:
 *   Haydar Alkaduhimi <haydar@hosting4all.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "testapplet.h"

#include "../../lib/textgraphicsitem.h"
#include "../../lib/panelwindow.h"

#include <QDebug>

TestApplet::TestApplet(PanelWindow* panelWindow)
    : Applet(panelWindow)
{
    setObjectName("Test");
    if(panelWindow != 0)
    {
        setPanelWindow(panelWindow);
    }
}

void TestApplet::setPanelWindow(PanelWindow* panelWindow)
{
    Applet::setPanelWindow(panelWindow);

    m_textItem = new TextGraphicsItem(this);
    m_textItem->setColor(Qt::white);
    m_textItem->setFont(m_panelWindow->font());
    m_textItem->setText("Applications");
#if QT_VERSION >= 0x050000
    // TODO: add oslogo to act like M$_WIN
    m_textItem->setImage(QImage("/usr/share/icons/hicolor/22x22/apps/webkit"));
#endif
}


QSize TestApplet::desiredSize()
{
#if QT_VERSION >= 0x050000
    return QSize(m_textItem->boundingRect().size().width() + 16,
                 m_textItem->boundingRect().size().height());
#else
    return QSize(m_textItem->boundingRect().size().width(),
                 m_textItem->boundingRect().size().height());
#endif
}

void TestApplet::layoutChanged()
{
#if QT_VERSION >= 0x050000
    m_textItem->setPos(8,
        (m_panelWindow->height() - m_textItem->boundingRect().height()) / 2);
#else
    m_textItem->setPos(8, m_panelWindow->textBaseLine());
#endif
}


Applet* TestAppletPlugin::createApplet(PanelWindow* panelWindow)
{
    return new TestApplet(panelWindow);
}

Q_PLUGIN_METADATA(IID "hde.panel.testapplet")
