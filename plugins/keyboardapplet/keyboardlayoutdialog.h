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

#ifndef KEYBOARDLAYOUTDIALOG_H
#define KEYBOARDLAYOUTDIALOG_H

#include <QDialog>

namespace Ui {
class KeyboardLayoutDialog;
}

/**
 * @brief The Keyboard Layout Dialog class
 */
class KeyboardLayoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KeyboardLayoutDialog(QWidget *parent = 0);
    ~KeyboardLayoutDialog();

private slots:
    void on_addButton_clicked();

    void on_removeButton_clicked();

    void on_buttonBox_accepted();

private:
    Ui::KeyboardLayoutDialog *ui;
    void load();
};

#endif // KEYBOARDLAYOUTDIALOG_H
