/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * This Files has been imported to hde from qtpanel
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

#include "dpisupport.h"

#include <QtGlobal>
#if QT_VERSION >= 0x060000
#include <QApplication>
#include <QScreen>
#elif QT_VERSION >= 0x050000
#include <QApplication>
#include <QScreen>
#include <QDesktopWidget>
#else
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#endif

int adjustHardcodedPixelSize(int size)
{
#if QT_VERSION >= 0x060000
    QScreen* screen = QGuiApplication::primaryScreen();
    const int dpi = screen ? int(screen->logicalDotsPerInchX()) : 96;
#elif QT_VERSION >= 0x050000
    QScreen* screen = QGuiApplication::primaryScreen();
    const int dpi = screen ? int(screen->logicalDotsPerInchX()) : 96;
#else
    const int dpi = qApp->desktop()->logicalDpiX();
#endif
    return size * dpi / 96;
}
