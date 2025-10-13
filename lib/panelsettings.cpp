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
#if QT_VERSION < 0x060000
#include <QDesktopWidget>
#endif
#include <QScreen>

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
    QDialog(parent),
    ui(new Ui::PanelSettings)
{
    m_panel_id = panel_id;
    ui->setupUi(this);

    // Manually connect signals that are missing from the UI file
    connect(ui->theme, QOverload<int>::of(&QComboBox::activated),
            [this](int) { on_theme_activated(ui->theme->currentText()); });
    connect(ui->verticalPosition, QOverload<int>::of(&QComboBox::activated),
            this, &PanelSettings::on_verticalPosition_activated);
    connect(ui->horizontalPosition, QOverload<int>::of(&QComboBox::activated),
            this, &PanelSettings::on_horizontalPosition_activated);
    connect(ui->screen, QOverload<int>::of(&QComboBox::activated),
            [this](int) { on_screen_activated(ui->screen->currentText()); });
    connect(ui->font, QOverload<int>::of(&QFontComboBox::currentIndexChanged),
            [this](int) { on_font_activated(ui->font->currentFont().family()); });
    connect(ui->fontSize, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PanelSettings::on_fontSize_valueChanged);

    readSettings();
}

PanelSettings::~PanelSettings()
{
    delete ui;
}

void PanelSettings::setPanelWindow(PanelWindow *panel)
{
    m_panel = panel;

    if (!ui) {
        qDebug() << "PanelSettings::setPanelWindow: ui is null!";
        return;
    }
    
    if (ui->theme) {
        ui->theme->setCurrentText(QIcon::themeName());
    }
    if (ui->verticalPosition) {
        // Load position as index: 0=Top, 1=Bottom
        int verticalPos = Settings::value(m_panel_id, "verticalPosition", 1).toInt();
        qDebug() << "PanelSettings::setPanelWindow() - Setting vertical position index to:" << verticalPos;
        ui->verticalPosition->setCurrentIndex(verticalPos);
    }
    if (ui->horizontalPosition) {
        // Load position as index: 0=Left, 1=Center, 2=Right
        int horizontalPos = Settings::value(m_panel_id, "horizontalPosition", 1).toInt();
        ui->horizontalPosition->setCurrentIndex(horizontalPos);
    }
    if (ui->screen) {
        ui->screen->setCurrentText(Settings::value(m_panel_id, "screen", "0").toString());
    }
    
    if (m_panel && ui->font && ui->fontSize) {
        ui->font->setCurrentText(m_panel->font().family());
        ui->fontSize->setValue(m_panel->font().pointSize());
    }

    // applets
    if (m_panel && ui->appletsList) {
        // Get applets from settings instead of panel to avoid memory corruption
        QStringList applets = Settings::value(m_panel_id, "applets", QStringList()).toStringList();
        foreach(QString applet_id, applets)
        {
            int index = applet_id.lastIndexOf("_");
            QString applet_name = applet_id.left(index);
            QString translated_name = translateAppletName(applet_name);
            QListWidgetItem *item = new QListWidgetItem(translated_name);
            item->setData(Qt::UserRole, applet_id);
            ui->appletsList->addItem(item);
        }
    }

    //ui->appletsList->addItems(applets);
    //ui->appletsList->addItems(Settings::value(m_panel_id, "applets", QStringList()).toStringList());
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
#if QT_VERSION >= 0x050000
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (int screen = 0; screen < screens.size(); ++screen) {
        ui->screen->addItem(QString::number(screen));
    }
#else
    for(int screen = 0; screen < QApplication::desktop()->screenCount(); screen++)
    {
        ui->screen->addItem(QString::number(screen));
    }
#endif
}

void PanelSettings::on_theme_activated(const QString &theme)
{
    ((PanelApplication *)qApp)->setIconThemeName(theme);
    Settings::setValue("General", "iconThemeName", theme);
}

void PanelSettings::on_verticalPosition_activated(int index)
{
    qDebug() << "PanelSettings::on_verticalPosition_activated() - Called with index:" << index;

    // index: 0=Top, 1=Bottom
    PanelWindow::Anchor verticalAnchor = (index == 0) ? PanelWindow::Min : PanelWindow::Max;

    qDebug() << "PanelSettings::on_verticalPosition_activated() - Setting vertical anchor to:" << verticalAnchor;
    m_panel->setVerticalAnchor(verticalAnchor);
    Settings::setValue(m_panel_id, "verticalPosition", index);
}

void PanelSettings::on_horizontalPosition_activated(int index)
{
    qDebug() << "PanelSettings::on_horizontalPosition_activated() - Called with index:" << index;

    // index: 0=Left, 1=Center, 2=Right
    PanelWindow::Anchor horizontalAnchor = PanelWindow::Center; // Default to center
    
    if(index == 0)
        horizontalAnchor = PanelWindow::Min;
    else if(index == 1)
        horizontalAnchor = PanelWindow::Center;
    else if(index == 2)
        horizontalAnchor = PanelWindow::Max;

    m_panel->setHorizontalAnchor(horizontalAnchor);
    Settings::setValue(m_panel_id, "horizontalPosition", index);
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
        applets << ui->appletsList->item(i)->data(Qt::UserRole).toString();
    }

    Settings::setValue(m_panel_id, "applets", applets );
    m_panel->resetApplets();
}

void PanelSettings::on_appletAdd_clicked()
{
    AppletsListDialog dialog(this);

    if(dialog.exec())
    {
        QString applet_id = dialog.currentApplet();
        if(!applet_id.isEmpty())
        {
            int index = applet_id.lastIndexOf("_");
            QString applet_name = applet_id.left(index);
            QString translated_name = translateAppletName(applet_name);
            QListWidgetItem *item = new QListWidgetItem(translated_name);
            item->setData(Qt::UserRole, applet_id);
            ui->appletsList->addItem(item);
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

QString PanelSettings::translateAppletName(const QString &appletName)
{
    // Translate applet names for the settings dialog
    if (appletName == "StartApplet") return tr("Start Menu");
    if (appletName == "ApplicationsMenuApplet") return tr("Applications Menu");
    if (appletName == "DockApplet") return tr("Task Bar");
    if (appletName == "TrayApplet") return tr("System Tray");
    if (appletName == "ClockApplet") return tr("Clock");
    if (appletName == "KeyboardApplet") return tr("Keyboard Layout");
    
    // Return original name if no translation found
    return appletName;
}
