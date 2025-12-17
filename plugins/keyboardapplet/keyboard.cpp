/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * Copyright: 2015-2025 Haydar Alkaduhimi
 * Authors:
 *   Haydar Alkaduhimi <haydar@developing4all.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3.0 of the License, or (at your option) any later version.
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

#include <QApplication>
#include <QDebug>
#include <QtCore/QProcessEnvironment>

#include <QtXml>


// X11
#include <X11/XKBlib.h>

QString Keyboard::getCurrentLayout()
{
    QString layout;

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return layout;
    XkbDescRec* kbd = XkbAllocKeyboard();
    if (kbd) {
        if (XkbGetNames(dpy, XkbSymbolsNameMask, kbd) == Success && kbd->names && kbd->names->symbols) {
            Atom symName = kbd->names->symbols;
            char* name = XGetAtomName(dpy, symName);
            if (name) {
                const QString layoutString = QString::fromLatin1(name);
                if(layoutString.split("+").count() == 3) {
                    layout = layoutString.split("+")[1];
                }
                XFree(name);
            }
        }
        XkbFreeKeyboard(kbd, XkbAllNamesMask, True);
    }
    XCloseDisplay(dpy);
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
    // Check if we're running on Wayland
    QString sessionType = qgetenv("XDG_SESSION_TYPE");
    QString waylandDisplay = qgetenv("WAYLAND_DISPLAY");
    
    if (sessionType == "wayland" || !waylandDisplay.isEmpty()) {
        // For Wayland, use gsettings to change keyboard layout
        // This works with GNOME Shell and other gsettings-compatible compositors
        QString gsettingsCmd = QString("gsettings set org.gnome.desktop.input-sources sources \"[('xkb', '%1')]\"").arg(layout);
        int result = system(gsettingsCmd.toLatin1());
        if (result == 0) {
            qDebug() << "Keyboard layout changed to" << layout << "via gsettings (Wayland)";
        } else {
            qWarning() << "Failed to change keyboard layout via gsettings, falling back to setxkbmap";
            system("setxkbmap " + layout.toLatin1());
        }
    } else {
        // For X11, use setxkbmap
        qDebug() << "Changing keyboard layout to" << layout << "via setxkbmap (X11)";
        system("setxkbmap " + layout.toLatin1());
    }
}
