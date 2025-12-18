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

#include "panelsettings.h"
#include "ui_panelsettings.h"

#include "panelwindow.h"
#include "applet.h"

#include "settings.h"
#include <QTimer>
#include <QMetaObject>
#include <QMetaMethod>

#include <QDir>
#include <QStringList>
#include <QDebug>
#include <QIcon>
#include <QSettings>
#include <QColorDialog>
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
                    QString mDirName = item.fileName(); // The actual directory name
                    //QString mDesc = settings.value("Comment").toString();
                    //QString mDepth = settings.value("DisplayDepth", 32).toString();
                    QStringList mDirectories = settings.value("Directories").toStringList();
                    bool mHidden = settings.value("Hidden", false).toBool();
                    if(!mName.isEmpty() && !mDirectories.isEmpty() && !mHidden)
                    {
                        // Use the directory name, not the display name, as that's what the filesystem uses
                        themes << mDirName;
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
            [this](int) { handleThemeActivated(ui->theme->currentText()); });
    connect(ui->position, QOverload<int>::of(&QComboBox::activated),
            this, &PanelSettings::on_position_activated);
    connect(ui->screen, QOverload<int>::of(&QComboBox::activated),
            [this](int) { handleScreenActivated(ui->screen->currentText()); });
    connect(ui->font, QOverload<int>::of(&QFontComboBox::currentIndexChanged),
            [this](int) { handleFontActivated(ui->font->currentFont().family()); });
    connect(ui->fontSize, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PanelSettings::on_fontSize_valueChanged);
    connect(ui->backgroundColorButton, &QPushButton::clicked,
            this, &PanelSettings::on_backgroundColorButton_clicked);
    connect(ui->borderColorButton, &QPushButton::clicked,
            this, &PanelSettings::on_borderColorButton_clicked);
    connect(ui->backgroundColorTransparency, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PanelSettings::handleBackgroundColorTransparencyChanged);
    connect(ui->borderColorTransparency, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PanelSettings::handleBorderColorTransparencyChanged);
    connect(ui->panelHeight, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PanelSettings::on_panelHeight_valueChanged);
    connect(ui->panelWidth, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PanelSettings::on_panelWidth_valueChanged);
    connect(ui->appletsList, &QListWidget::itemSelectionChanged,
            this, &PanelSettings::on_appletsList_itemSelectionChanged);

    readSettings();
    
    // Initially hide the settings button
    if (ui->appletSettings) {
        ui->appletSettings->setVisible(false);
    }
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
    if (ui->position) {
        // Load position: try new format first, then migrate from old format
        QVariant posVariant = Settings::value(m_panel_id, "position", QVariant());
        int posIndex = 1; // Default to Bottom
        
        if (posVariant.isValid()) {
#if QT_VERSION >= 0x060000
            bool isString = (posVariant.metaType().id() == QMetaType::QString);
#else
            bool isString = (posVariant.type() == QVariant::String);
#endif
            if (isString) {
                QString pos = posVariant.toString();
                if (pos == "Top") posIndex = 0;
                else if (pos == "Bottom") posIndex = 1;
                else if (pos == "Left") posIndex = 2;
                else if (pos == "Right") posIndex = 3;
            } else {
                posIndex = posVariant.toInt();
            }
        } else {
            // Migrate from old format
            QVariant vpos = Settings::value(m_panel_id, "verticalPosition", 1);
            QVariant hpos = Settings::value(m_panel_id, "horizontalPosition", 1);
            
            bool isTop = false, isBottom = false, isLeft = false, isRight = false;
            
#if QT_VERSION >= 0x060000
            bool vposIsString = (vpos.metaType().id() == QMetaType::QString);
#else
            bool vposIsString = (vpos.type() == QVariant::String);
#endif
            if (vposIsString) {
                QString v = vpos.toString();
                isTop = (v == "Top");
                isBottom = (v == "Bottom");
            } else {
                int v = vpos.toInt();
                isTop = (v == 0);
                isBottom = (v == 1);
            }
            
#if QT_VERSION >= 0x060000
            bool hposIsString = (hpos.metaType().id() == QMetaType::QString);
#else
            bool hposIsString = (hpos.type() == QVariant::String);
#endif
            if (hposIsString) {
                QString h = hpos.toString();
                isLeft = (h == "Left");
                isRight = (h == "Right");
            } else {
                int h = hpos.toInt();
                isLeft = (h == 0);
                isRight = (h == 2);
            }
            
            if (isTop) posIndex = 0;
            else if (isBottom) posIndex = 1;
            else if (isLeft) posIndex = 2;
            else if (isRight) posIndex = 3;
        }
        
        ui->position->setCurrentIndex(posIndex);
    }
    if (ui->screen) {
        ui->screen->setCurrentText(Settings::value(m_panel_id, "screen", "0").toString());
    }
    
    if (m_panel && ui->font && ui->fontSize) {
        ui->font->setCurrentText(m_panel->font().family());
        ui->fontSize->setValue(m_panel->font().pointSize());
    }

    // Load color settings
    QColor bgColor = Settings::value(m_panel_id, "backgroundColor", QColor(0, 0, 0)).value<QColor>();
    int bgTransparency = Settings::value(m_panel_id, "backgroundColorTransparency", 128).toInt();
    QColor borderColor = Settings::value(m_panel_id, "borderColor", QColor(255, 255, 255)).value<QColor>();
    int borderTransparency = Settings::value(m_panel_id, "borderColorTransparency", 128).toInt();
    
    if (ui->backgroundColorButton) {
        ui->backgroundColorButton->setStyleSheet(QString("background-color: %1;").arg(bgColor.name()));
    }
    if (ui->backgroundColorTransparency) {
        ui->backgroundColorTransparency->blockSignals(true);
        ui->backgroundColorTransparency->setValue(bgTransparency);
        ui->backgroundColorTransparency->blockSignals(false);
    }
    if (ui->borderColorButton) {
        ui->borderColorButton->setStyleSheet(QString("background-color: %1;").arg(borderColor.name()));
    }
    if (ui->borderColorTransparency) {
        ui->borderColorTransparency->blockSignals(true);
        ui->borderColorTransparency->setValue(borderTransparency);
        ui->borderColorTransparency->blockSignals(false);
    }
    
    // Load size settings
    int panelHeight = Settings::value(m_panel_id, "panelHeight", 48).toInt();
    int panelWidth = Settings::value(m_panel_id, "panelWidth", 48).toInt();
    if (ui->panelHeight) {
        ui->panelHeight->blockSignals(true);
        ui->panelHeight->setValue(panelHeight);
        ui->panelHeight->blockSignals(false);
    }
    if (ui->panelWidth) {
        ui->panelWidth->blockSignals(true);
        ui->panelWidth->setValue(panelWidth);
        ui->panelWidth->blockSignals(false);
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
    
    // Check initial selection to show/hide settings button
    on_appletsList_itemSelectionChanged();
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

void PanelSettings::handleThemeActivated(const QString &theme)
{
    ((PanelApplication *)qApp)->setIconThemeName(theme);
    Settings::setValue("Main", "iconThemeName", theme);
}

void PanelSettings::on_position_activated(int index)
{
    if (!m_panel) return;
    
    // index: 0=Top, 1=Bottom, 2=Left, 3=Right
    PanelWindow::Position position;
    QString positionString;
    
    switch (index) {
        case 0:
            position = PanelWindow::Top;
            positionString = "Top";
            break;
        case 1:
            position = PanelWindow::Bottom;
            positionString = "Bottom";
            break;
        case 2:
            position = PanelWindow::Left;
            positionString = "Left";
            break;
        case 3:
            position = PanelWindow::Right;
            positionString = "Right";
            break;
        default:
            position = PanelWindow::Bottom;
            positionString = "Bottom";
            break;
    }
    
    m_panel->setPosition(position);
    Settings::setValue(m_panel_id, "position", positionString);
    
    // Force sync to ensure settings are written immediately
    Settings::s_settings->sync();
}

void PanelSettings::handleScreenActivated(const QString &screen)
{
//    qDebug() << screen;
    m_panel->setScreen(screen.toInt());
    Settings::setValue(m_panel_id, "screen", screen);
}

void PanelSettings::handleFontActivated(const QString &font_name)
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
    if (!m_panel || !ui->appletsList) {
        return;
    }
    
    QListWidgetItem* selectedItem = ui->appletsList->currentItem();
    if (!selectedItem) {
        return;
    }
    
    QString appletId = selectedItem->data(Qt::UserRole).toString();
    if (appletId.isEmpty()) {
        return;
    }
    
    // Find the applet instance in the panel
    Applet* applet = m_panel->getAppletById(appletId);
    if (!applet) {
        return;
    }
    
    // Check if the applet has a showConfigurationDialog() method using QMetaObject
    const QMetaObject* metaObj = applet->metaObject();
    int methodIndex = metaObj->indexOfMethod("showConfigurationDialog()");
    
    if (methodIndex != -1) {
        // Invoke the method
        QMetaMethod method = metaObj->method(methodIndex);
        method.invoke(applet, Qt::QueuedConnection);
    }
}

void PanelSettings::on_appletsList_itemSelectionChanged()
{
    if (!m_panel || !ui->appletsList || !ui->appletSettings) {
        return;
    }
    
    QListWidgetItem* selectedItem = ui->appletsList->currentItem();
    if (!selectedItem) {
        ui->appletSettings->setVisible(false);
        return;
    }
    
    QString appletId = selectedItem->data(Qt::UserRole).toString();
    if (appletId.isEmpty()) {
        ui->appletSettings->setVisible(false);
        return;
    }
    
    // Find the applet instance in the panel
    Applet* applet = m_panel->getAppletById(appletId);
    if (!applet) {
        ui->appletSettings->setVisible(false);
        return;
    }
    
    // Check if the applet has a showConfigurationDialog() method
    const QMetaObject* metaObj = applet->metaObject();
    int methodIndex = metaObj->indexOfMethod("showConfigurationDialog()");
    
    // Show button only if the applet has a configuration dialog
    ui->appletSettings->setVisible(methodIndex != -1);
}

QString PanelSettings::translateAppletName(const QString &appletName)
{
    return PanelWindow::getAppletPluginName(appletName);
}

void PanelSettings::on_backgroundColorButton_clicked()
{
    QColor currentColor = Settings::value(m_panel_id, "backgroundColor", QColor(0, 0, 0)).value<QColor>();
    QColor color = QColorDialog::getColor(currentColor, this, tr("Choose Background Color"));
    if (color.isValid()) {
        Settings::setValue(m_panel_id, "backgroundColor", color);
        ui->backgroundColorButton->setStyleSheet(QString("background-color: %1;").arg(color.name()));
        if (m_panel) {
            m_panel->updateColors();
        }
    }
}

void PanelSettings::on_borderColorButton_clicked()
{
    QColor currentColor = Settings::value(m_panel_id, "borderColor", QColor(255, 255, 255)).value<QColor>();
    QColor color = QColorDialog::getColor(currentColor, this, tr("Choose Border Color"));
    if (color.isValid()) {
        Settings::setValue(m_panel_id, "borderColor", color);
        ui->borderColorButton->setStyleSheet(QString("background-color: %1;").arg(color.name()));
        if (m_panel) {
            m_panel->updateColors();
        }
    }
}

void PanelSettings::handleBackgroundColorTransparencyChanged(int value)
{
    Settings::setValue(m_panel_id, "backgroundColorTransparency", value);
    Settings::s_settings->sync(); // Ensure settings are saved immediately
    if (m_panel) {
        m_panel->updateColors();
    }
}

void PanelSettings::handleBorderColorTransparencyChanged(int value)
{
    Settings::setValue(m_panel_id, "borderColorTransparency", value);
    Settings::s_settings->sync(); // Ensure settings are saved immediately
    if (m_panel) {
        m_panel->updateColors();
    }
}

void PanelSettings::on_panelHeight_valueChanged(int value)
{
    // Save to settings immediately to prevent loss on tab switch
    Settings::setValue(m_panel_id, "panelHeight", value);
    Settings::s_settings->sync(); // Ensure settings are saved immediately
    
    if (m_panel) {
        m_panel->setPanelHeight(value);
        // Update the spinbox value in case it was adjusted by setPanelHeight()
        // Use a small delay to avoid recursion
        QTimer::singleShot(0, this, [this]() {
            if (ui->panelHeight && m_panel && ui->panelHeight->value() != m_panel->panelHeight()) {
                ui->panelHeight->blockSignals(true);
                ui->panelHeight->setValue(m_panel->panelHeight());
                ui->panelHeight->blockSignals(false);
            }
        });
    }
}

void PanelSettings::on_panelWidth_valueChanged(int value)
{
    // Save to settings immediately to prevent loss on tab switch
    Settings::setValue(m_panel_id, "panelWidth", value);
    Settings::s_settings->sync(); // Ensure settings are saved immediately
    
    if (m_panel) {
        m_panel->setPanelWidth(value);
        // Update the spinbox value in case it was adjusted by setPanelWidth()
        // Use a small delay to avoid recursion
        QTimer::singleShot(0, this, [this]() {
            if (ui->panelWidth && m_panel && ui->panelWidth->value() != m_panel->panelWidth()) {
                ui->panelWidth->blockSignals(true);
                ui->panelWidth->setValue(m_panel->panelWidth());
                ui->panelWidth->blockSignals(false);
            }
        });
    }
}
