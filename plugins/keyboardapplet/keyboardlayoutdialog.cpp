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

#include "keyboardlayoutdialog.h"
#include "ui_keyboardlayoutdialog.h"

#include "keyboard.h"
#include <QSettings>
#include <QDebug>

KeyboardLayoutDialog::KeyboardLayoutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::KeyboardLayoutDialog)
{
    ui->setupUi(this);

    load();
}

KeyboardLayoutDialog::~KeyboardLayoutDialog()
{
    delete ui;
}

void KeyboardLayoutDialog::load()
{
    QSettings setting(this);
    QStringList allLayouts = Keyboard::getLayoutsList();
    QStringList layouts = setting.value("layouts", QStringList() << "us").toStringList();

    foreach (QString layout, layouts) {
        allLayouts.removeAll(layout);
    }
    allLayouts.removeDuplicates();

    ui->allLayouts->addItems(allLayouts);
    ui->currentLayouts->addItems(layouts);
}


void KeyboardLayoutDialog::on_addButton_clicked()
{
    if (ui->allLayouts->currentItem()) {
        QListWidgetItem *newItem = ui->allLayouts->currentItem()->clone();
        ui->currentLayouts->addItem(newItem);
        ui->currentLayouts->setCurrentItem(newItem);
        delete ui->allLayouts->currentItem();
    }
}

void KeyboardLayoutDialog::on_removeButton_clicked()
{
    if (ui->currentLayouts->currentItem()) {
        QListWidgetItem *newItem = ui->currentLayouts->currentItem()->clone();
        ui->allLayouts->addItem(newItem);
        ui->allLayouts->setCurrentItem(newItem);
        delete ui->currentLayouts->currentItem();
    }
}

void KeyboardLayoutDialog::on_buttonBox_accepted()
{
    QStringList stringList;
    for(int i = 0; i < ui->currentLayouts->count(); ++i)
    {
        QListWidgetItem* item = ui->currentLayouts->item(i);
        stringList << item->text();
    }

    QSettings setting(this);
    setting.setValue("layouts", stringList);
}
