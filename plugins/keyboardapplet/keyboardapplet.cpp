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

#include "keyboardapplet.h"
#include "keyboard.h"
#include "keyboardlayoutdialog.h"

#include "../../lib/textgraphicsitem.h"
#include "../../lib/panelwindow.h"
#include "../../lib/hpopupmenu.h"

#include <QApplication>
#include <QPainter>
#include <QGraphicsSceneContextMenuEvent>

/*
#include <qxt/QxtCore/qxtglobal.h>
#include <qxt/QxtGui/qxtglobalshortcut.h>
*/
#include <qxtglobal.h>
#include <qxtglobalshortcut.h>

KeyboardApplet::KeyboardApplet(PanelWindow *panelWindow)
    : Applet(panelWindow)
{
    setObjectName("Keyboard");
    if(panelWindow != 0)
    {
        setPanelWindow(panelWindow);
    }

    QSettings setting(this);
    m_supported_layouts = setting.value("layouts", QStringList() << "us").toStringList();
    m_current_layout = m_supported_layouts.first();


    forwardShortcut = new QxtGlobalShortcut(this);
    forwardShortcut->setShortcut(QKeySequence(setting.value("layoutsForward", "Meta+Space").toString()));
    connect(forwardShortcut, SIGNAL(activated()), this, SLOT(changeForward()));

    backwardShortcut = new QxtGlobalShortcut(this);
    backwardShortcut->setShortcut(QKeySequence(setting.value("layoutsBackward", "Ctrl+Meta+Space").toString()));
    connect(backwardShortcut, SIGNAL(activated()), this, SLOT(changeBackword()));
}

KeyboardApplet::~KeyboardApplet()
{
    delete m_textItem;
}

void KeyboardApplet::setPanelWindow(PanelWindow* panelWindow)
{
    Applet::setPanelWindow(panelWindow);

    m_textItem = new TextGraphicsItem(this);
    m_textItem->setColor(Qt::white);
    m_textItem->setFont(m_panelWindow->font());
}

bool KeyboardApplet::init()
{
    setInteractive(true);

    setCurrentLayout(m_current_layout);

    return true;
}

void KeyboardApplet::drawCurrentLayout(QString layout)
{
    if(layout.isEmpty())
    {
        layout = Keyboard::getCurrentLayout();
    }

    if(layout.isEmpty() || (m_current_layout != layout))
    {
        // Set layout to current layout
        setCurrentLayout(m_current_layout);
    }

    if(!layout.isEmpty())
    {
        qDebug() << layout;
        QImage img(30, 16, QImage::Format_RGB888);

        QPainter p(&img);
        p.fillRect(0,0,30,16, Qt::white);
        p.setPen(QPen(Qt::black));
        p.drawText(img.rect(), Qt::AlignCenter, layout);
        m_textItem->setImage(img);
    }
}

void KeyboardApplet::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    HPopupMenu menu;

    menu.addTitle("Keyboard Applet");
    menu.addAction(QIcon::fromTheme("preferences-desktop-keyboard"), "Configure keyboard", this, SLOT(showConfigurationDialog()));


    menu.addTitle("Panel");
    menu.addAction(QIcon::fromTheme("preferences-desktop"), "Configure Panel", m_panelWindow, SLOT(showConfigurationDialog()));

    menu.addAction(QIcon::fromTheme("list-add"), "Add Panel", QApplication::instance(), SLOT(addPanel()));
    menu.addAction(QIcon::fromTheme("list-remove"), "Remove Panel", m_panelWindow, SLOT(removePanel()));

    menu.exec(event->screenPos());
}

void KeyboardApplet::showConfigurationDialog()
{
    KeyboardLayoutDialog dialog;

    if(dialog.exec())
    {
        QSettings setting(this);
        m_supported_layouts = setting.value("layouts", QStringList() << "us").toStringList();
        m_current_layout = m_supported_layouts.first();
        forwardShortcut->setShortcut(QKeySequence(setting.value("layoutsForward", "Meta+Space").toString()));
        backwardShortcut->setShortcut(QKeySequence(setting.value("layoutsBackward", "Ctrl+Meta+Space").toString()));
    }
}

QSize KeyboardApplet::desiredSize()
{
#if QT_VERSION >= 0x050000
    return QSize(m_textItem->boundingRect().size().width() + 16,
                 m_textItem->boundingRect().size().height());
#else
    return QSize(m_textItem->boundingRect().size().width(),
                 m_textItem->boundingRect().size().height());
#endif
}

void KeyboardApplet::layoutChanged()
{
#if QT_VERSION >= 0x050000
    m_textItem->setPos(8,
        (m_panelWindow->height() - m_textItem->boundingRect().height()) / 2);
#else
    m_textItem->setPos(8, m_panelWindow->textBaseLine());
#endif
}


void KeyboardApplet::clicked()
{
    changeForward();
}

void KeyboardApplet::changeForward()
{
    if(m_supported_layouts.count() < 2)
        return;

    int pos = m_supported_layouts.indexOf(m_current_layout);

    if(pos >= (m_supported_layouts.count() - 1))
    {
        m_current_layout = m_supported_layouts.at(0);
    }
    else
    {
        m_current_layout = m_supported_layouts.at(pos+1);
    }
    setCurrentLayout(m_current_layout);
}

void KeyboardApplet::changeBackword()
{
    if(m_supported_layouts.count() < 2)
        return;

    int pos = m_supported_layouts.indexOf(m_current_layout);

    if(pos <= 0)
    {
        m_current_layout = m_supported_layouts.at(m_supported_layouts.count() -1);
    }
    else
    {
        m_current_layout = m_supported_layouts.at(pos-1);
    }
    setCurrentLayout(m_current_layout);
}

void KeyboardApplet::setCurrentLayout(QString layout)
{
    Keyboard::setLayout(layout);
    drawCurrentLayout(layout);
}


Applet* KeyboardAppletPlugin::createApplet(PanelWindow* panelWindow)
{
    return new KeyboardApplet(panelWindow);
}

Q_PLUGIN_METADATA(IID "hde.panel.keyboardapplet")
