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

#include "panelsettings.h"
#include "ui_panelsettings.h"

#include "panelwindow.h"

#include "settings.h"

#include <QDir>
#include <QStringList>
#include <QDebug>
#include <QIcon>
#include <QSettings>
#include <QDesktopWidget>

#include "panelapplication.h"

#include "appletslistdialog.h"

static QStringList listIconThemes() {
    QStringList themes;

    foreach(QString theme_path, QIcon::themeSearchPaths())
    {
        QDir theme_dir(theme_path);
        foreach(QFileInfo item, theme_dir.entryInfoList() )
        {
            if(item.isDir())
            {
                QString fileName, mainSection;
                if(QFile::exists(item.absoluteFilePath() + "/index.desktop")) {
                    fileName = item.absoluteFilePath() + "/index.desktop";
                    mainSection="KDE Icon Theme";
                } else {
                    fileName = item.absoluteFilePath() + "/index.theme";
                    mainSection="Icon Theme";
                }
                if(QFile::exists(fileName))
                {
                    QSettings settings(fileName, QSettings::IniFormat);
                    settings.beginGroup(mainSection);
                    QString mName = settings.value("Name").toString();
                    //QString mDesc = settings.value("Comment").toString();
                    //QString mDepth = settings.value("DisplayDepth", 32).toString();
                    QStringList mDirectories = settings.value("Directories").toStringList();
                    if(!mName.isEmpty() && !mDirectories.isEmpty())
                    {
                        themes << mName.toLower();
                    }
                }
            }
        }

    }

   return themes;
}


PanelSettings::PanelSettings(QString panel_id, QWidget *parent) :
    m_panel_id(panel_id),
    QDialog(parent),
    ui(new Ui::PanelSettings)
{
    ui->setupUi(this);

    readSettings();
}

PanelSettings::~PanelSettings()
{
    delete ui;
}

void PanelSettings::setPanelWindow(PanelWindow *panel)
{
    m_panel = panel;

    ui->theme->setCurrentText(QIcon::themeName());
    ui->verticalPosition->setCurrentText(Settings::value(m_panel_id, "verticalPosition", "Bottom").toString());
    ui->screen->setCurrentText(Settings::value(m_panel_id, "screen", "0").toString());
    ui->font->setCurrentText(m_panel->font().family());
    ui->fontSize->setValue(m_panel->font().pointSize());

    ui->appletsList->addItems(Settings::value(m_panel_id, "applets", QStringList()).toStringList());
}

void PanelSettings::on_resetButton_clicked()
{

}

void PanelSettings::readSettings()
{
    // Get themes
    QStringList themes = listIconThemes();
    // Fill themes combo box
    ui->theme->addItems(themes);

    // Fill screen numbers
    for(int screen = 0; screen < QApplication::desktop()->screenCount(); screen++)
    {
        ui->screen->addItem(QString::number(screen));
    }
}

void PanelSettings::on_theme_activated(const QString &theme)
{
    ((PanelApplication *)qApp)->setIconThemeName(theme);
    Settings::setValue("General", "iconThemeName", theme);
}

void PanelSettings::on_verticalPosition_activated(const QString &verticalPosition)
{
//    qDebug() << verticalPosition;

    PanelWindow::Anchor m_verticalAnchor;

    if(verticalPosition == "Top")
        m_verticalAnchor = PanelWindow::Min;
    else if(verticalPosition == "Bottom")
        m_verticalAnchor = PanelWindow::Max;

    m_panel->setVerticalAnchor(m_verticalAnchor);
    Settings::setValue(m_panel_id, "verticalPosition", verticalPosition);
}

void PanelSettings::on_screen_activated(const QString &screen)
{
//    qDebug() << screen;
    m_panel->setScreen(screen.toInt());
    Settings::setValue(m_panel_id, "screen", screen);
}

void PanelSettings::on_font_activated(const QString &font_name)
{
    return fontChanged(font_name + " " + QString::number(ui->fontSize->value()));
}

void PanelSettings::on_fontSize_valueChanged(int size)
{
    return fontChanged(ui->font->currentFont().family() + " " + QString::number(size));
}

void PanelSettings::fontChanged(const QString font)
{
//    qDebug() << font;
    m_panel->setFontName(font);
    Settings::setValue(m_panel_id, "fontName", font);
}

void PanelSettings::on_appletUp_clicked()
{
    int row = ui->appletsList->currentRow();
    if(row > 0)
    {
        QListWidgetItem *item = ui->appletsList->takeItem(row);
        ui->appletsList->insertItem(row -1, item);
        ui->appletsList->setCurrentItem(item);
        applyAppletList();
    }
}

void PanelSettings::on_appletDown_clicked()
{
    int row = ui->appletsList->currentRow();
    if(row < ui->appletsList->count()-1)
    {
        QListWidgetItem *item = ui->appletsList->takeItem(row);
        ui->appletsList->insertItem(row +1, item);
        ui->appletsList->setCurrentItem(item);
        applyAppletList();
    }
}

void PanelSettings::applyAppletList()
{
    QStringList applets;
    for (int i = 0; i < ui->appletsList->count() ; ++i)
    {
        applets << ui->appletsList->item(i)->text();
    }

    Settings::setValue(m_panel_id, "applets", applets );
    m_panel->resetApplets();
}

void PanelSettings::on_appletAdd_clicked()
{
    AppletsListDialog dialog(this);

    if(dialog.exec())
    {
        QString applet = dialog.currentApplet();
        if(!applet.isEmpty())
        {
            ui->appletsList->addItem(applet);
            applyAppletList();
        }
    }
}

void PanelSettings::on_appletRemove_clicked()
{
    if(!ui->appletsList->selectedItems().isEmpty())
    {
        qDeleteAll(ui->appletsList->selectedItems());
        applyAppletList();
    }
}

void PanelSettings::on_appletSettings_clicked()
{
}
