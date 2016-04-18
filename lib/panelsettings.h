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

#ifndef PANELSETTINGS_H
#define PANELSETTINGS_H

#include <QDialog>

class PanelWindow;

namespace Ui {
class PanelSettings;
}

class PanelSettings : public QDialog
{
    Q_OBJECT

public:
    explicit PanelSettings(QString panel_id, QWidget *parent = 0);
    ~PanelSettings();
    void setPanelWindow(PanelWindow *panel);

private slots:
    void on_resetButton_clicked();

    void on_theme_activated(const QString &theme);

    void on_verticalPosition_activated(const QString &verticalPosition);

    void on_screen_activated(const QString &screen);

    void on_font_activated(const QString &font_name);

    void on_fontSize_valueChanged(int size);

    void on_appletUp_clicked();

    void on_appletDown_clicked();

    void on_appletAdd_clicked();

    void on_appletRemove_clicked();

    void on_appletSettings_clicked();

private:
    void readSettings();
    void fontChanged(const QString font);
    void applyAppletList();

    Ui::PanelSettings *ui;
    QString m_panel_id;
    PanelWindow *m_panel;
};

#endif // PANELSETTINGS_H
