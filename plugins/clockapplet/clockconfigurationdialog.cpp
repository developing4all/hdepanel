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

#include "clockconfigurationdialog.h"
#include "ui_clockconfigurationdialog.h"

#include <settings.h>

ClockConfigurationDialog::ClockConfigurationDialog(QString id, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ClockConfigurationDialog)
{
    ui->setupUi(this);
    m_id = id;

    // Load current setting (default to 12-hour format)
    bool use24Hour = Settings::value(m_id, "use24HourFormat", false).toBool();
    if (use24Hour) {
        ui->radioButton24Hour->setChecked(true);
    } else {
        ui->radioButton12Hour->setChecked(true);
    }
}

ClockConfigurationDialog::~ClockConfigurationDialog()
{
    delete ui;
}

void ClockConfigurationDialog::on_buttonBox_accepted()
{
    // Save the selected format
    bool use24Hour = ui->radioButton24Hour->isChecked();
    Settings::setValue(m_id, "use24HourFormat", use24Hour);
}



