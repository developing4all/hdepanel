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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <QString>
#include <QStringList>

/**
 * A class that handle all XKB needed functions
 */
class Keyboard
{
public:
    /**
     * Constructor
     */
    Keyboard(){}

    /**
     * get the Current layout using XKB library
     * @return QString current layout
     */
    static QString getCurrentLayout();

    /**
     * get the supported layout using XKB defenition file
     * @return QStringList supported layout list
     */
    static QStringList getLayoutsList();

    /**
     * Set current xkb layout
     * @param layout
     */
    static void setLayout(QString layout);
};

#endif // KEYBOARD_H
