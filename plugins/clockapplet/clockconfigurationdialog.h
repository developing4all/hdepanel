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

#ifndef CLOCKCONFIGURATIONDIALOG_H
#define CLOCKCONFIGURATIONDIALOG_H

#include <QDialog>

namespace Ui {
class ClockConfigurationDialog;
}

class ClockConfigurationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ClockConfigurationDialog(QString id, QWidget *parent = 0);
    ~ClockConfigurationDialog();

private slots:
    void on_buttonBox_accepted();

private:
    Ui::ClockConfigurationDialog *ui;
    QString m_id;
};

#endif // CLOCKCONFIGURATIONDIALOG_H



