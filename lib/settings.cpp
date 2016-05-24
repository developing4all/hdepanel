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

#include "settings.h"

#include <QSettings>
#include <QDebug>

QSettings *Settings::s_settings = 0;

Settings::Settings()
{
    Settings::s_settings = new QSettings();
}

void Settings::setGroup(const QString &group)
{
    if(Settings::s_settings->group() != group)
    {
        Settings::s_settings->endGroup();
        Settings::s_settings->beginGroup(group);
    }
}


QVariant Settings::value(const QString &group, const QString &key, const QVariant &defaultValue)
{
    Settings::setGroup(group);
    return Settings::s_settings->value(key,defaultValue);
}

QVariant Settings::value(const QString &key, const QVariant &defaultValue)
{
    return Settings::s_settings->value(key,defaultValue);
}

void Settings::setValue(const QString &group, const QString &key, const QVariant &value)
{
    Settings::setGroup(group);
    return Settings::s_settings->setValue(key,value);
}

void Settings::setValue(const QString &key, const QVariant &value)
{
    return Settings::s_settings->setValue(key,value);
}
