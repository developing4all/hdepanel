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

#include "keyboard.h"

#include <QX11Info>
#include <QDebug>

#include <QtXml>


// X11
#include <X11/XKBlib.h>

QString Keyboard::getCurrentLayout()
{
    QString layout;

    XkbDescRec* _kbdDescPtr = XkbAllocKeyboard();
    XkbGetNames(QX11Info::display(), XkbSymbolsNameMask, _kbdDescPtr);
    Atom symName = _kbdDescPtr -> names -> symbols;
    QString layoutString = XGetAtomName(QX11Info::display(), symName);
    if(layoutString.split("+").count() == 3)
    {
        layout = layoutString.split("+")[1];
    }

    return layout;
}

QStringList Keyboard::getLayoutsList()
{
    QStringList layoutsList;

    // Create a document to write XML
    QDomDocument document;

    // Open a file for reading
    QFile file("/usr/share/X11/xkb/rules/base.xml");
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {

    }
    else
    {
        // loading
        if(!document.setContent(&file))
        {
            qDebug() << "Failed to load the file for reading.";
            return layoutsList;
        }
        file.close();
    }

    // Getting root element
    QDomElement root = document.firstChildElement();

    QDomNodeList nodes = root.elementsByTagName("layoutList");

    qDebug() << "# nodes = " << nodes.count();
    for(int i = 0; i < nodes.count(); i++)
    {
        QDomNode elm = nodes.at(i);
        if(elm.isElement())
        {
            QDomNodeList layouts = elm.toElement().elementsByTagName("layout");

            qDebug() << "# layouts = " << layouts.count();
            for(int i = 0; i < layouts.count(); i++)
            {
                QDomNode layout = layouts.at(i);
                if(layout.isElement())
                {
                    QDomElement e = layout.firstChildElement("configItem");
                    layoutsList << e.toElement().firstChildElement("name").text();
                    qDebug() << e.toElement().firstChildElement("name").text();
                }
            }
        }
    }

    return layoutsList;
}

void Keyboard::setLayout(QString layout)
{
    // Using system call for now
    // maybe we should change later to use the xkb library
    system("setxkbmap " + layout.toLatin1());
}
