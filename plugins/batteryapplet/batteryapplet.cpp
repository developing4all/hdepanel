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
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "batteryapplet.h"
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <settings.h>
#include <QMenu>
#include "hpopupmenu.h"
#include <QApplication>
#if QT_VERSION >= 0x050000
#include <QGraphicsSceneContextMenuEvent>
#else
#include <QtGui/QGraphicsSceneContextMenuEvent>
#endif
#include <QFile>
#include <QDir>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <chrono>
#endif
#if QT_VERSION >= 0x050000
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#else
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsPixmapItem>
#endif
#include "textgraphicsitem.h"
#include "panelwindow.h"
#include <QFontMetrics>
#include <QIcon>
#include <QPainter>
#include "../../lib/dpisupport.h"

BatteryApplet::BatteryApplet(PanelWindow* panelWindow)
	: Applet(panelWindow)
{
    setObjectName("Battery");
    
    // Create text and icon items early
    m_textItem = new TextGraphicsItem(this);
    m_textItem->setColor(Qt::white);
    
    m_iconItem = new QGraphicsPixmapItem(this);
}

void BatteryApplet::setPanelWindow(PanelWindow *panelWindow)
{
    Applet::setPanelWindow(panelWindow);

    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setSingleShot(false);
        m_timer->setInterval(30000); // Update every 30 seconds
        connect(m_timer, SIGNAL(timeout()), this, SLOT(updateContent()));
    }

    if (m_textItem && m_panelWindow) {
        m_textItem->setFont(m_panelWindow->font());
    }
}

BatteryApplet::~BatteryApplet()
{
    close();
}

void BatteryApplet::close()
{
    if (m_textItem) {
        delete m_textItem;
        m_textItem = nullptr;
    }
    
    if (m_iconItem) {
        delete m_iconItem;
        m_iconItem = nullptr;
    }
    
    if (m_timer) {
        m_timer->stop();
        delete m_timer;
        m_timer = nullptr;
    }
}

void BatteryApplet::fontChanged()
{
    if (m_textItem && m_panelWindow) {
        m_textItem->setFont(m_panelWindow->font());
        updateContent();
    }
}

bool BatteryApplet::init()
{
    setInteractive(true);
    readSettings();
    
    // Find battery device using sysfs (non-blocking file read)
    m_batteryDevicePath = findBatteryDevice();
    
    if (m_batteryDevicePath.isEmpty()) {
        m_batteryPresent = false;
    } else {
        m_batteryPresent = true;
    }
    
    updateContent();
    
    if (m_timer) {
        m_timer->start();
    }
    return true;
}

void BatteryApplet::layoutChanged()
{
    if (!m_textItem || !m_panelWindow)
        return;

    const PanelWindow::Position pos = m_panelWindow->position();
    const bool verticalPanel = (pos == PanelWindow::Left || pos == PanelWindow::Right);

    const int availW = (m_size.width() > 0 ? m_size.width() : (verticalPanel ? m_panelWindow->panelWidth()
                                                                            : m_panelWindow->panelHeight()));
    const int availH = (m_size.height() > 0 ? m_size.height() : (verticalPanel ? m_panelWindow->panelHeight()
                                                                               : m_panelWindow->panelWidth()));

    const int innerPad = 6;
    const int usableH = qMax(0, availH - 2 * innerPad);

    QFontMetrics fm(m_panelWindow->font());
    
    // Calculate icon size
    int iconSize = adjustHardcodedPixelSize(24);
    if (verticalPanel) {
        iconSize = qMin(iconSize, usableH - (m_showPercentage ? fm.height() + 4 : 0));
    } else {
        iconSize = qMin(iconSize, usableH - 4);
    }
    iconSize = qMax(iconSize, adjustHardcodedPixelSize(16));
    
    int iconX = 0, iconY = 0;
    int textX = 0, textY = 0;
    
    if (!verticalPanel) {
        // Horizontal layout: icon on left, text on right (or just icon, or just text)
        int contentWidth = 0;
        if (m_showIcon) {
            contentWidth += iconSize + 4;
        }
        if (m_showPercentage && m_batteryPresent && !m_text.isEmpty()) {
            contentWidth += fm.horizontalAdvance(m_text);
        }
        
        int startX = (availW - contentWidth) / 2;
        if (startX < innerPad) startX = innerPad;
        
        if (m_showIcon) {
            iconX = startX;
            iconY = (availH - iconSize) / 2;
            startX += iconSize + 4;
        }
        
        if (m_showPercentage && m_batteryPresent && !m_text.isEmpty()) {
            textX = startX;
            textY = m_panelWindow->textBaseLine();
        }
    } else {
        // Vertical layout: icon on top, text below (or centered)
        // Use actual allocated size for centering, with fallback
        int appletHeight = m_size.height();
        if (appletHeight <= 0) {
            // Fallback if size not set yet
            appletHeight = m_panelWindow->panelWidth();
        }
        
        int contentHeight = 0;
        if (m_showIcon) {
            contentHeight += iconSize;
        }
        if (m_showPercentage && m_batteryPresent && !m_text.isEmpty()) {
            contentHeight += fm.height() + 4;
        }
        
        // Center content vertically within the allocated applet height
        int startY = (appletHeight - contentHeight) / 2;
        // Ensure non-negative
        if (startY < 0) startY = 0;
        
        if (m_showIcon) {
            iconX = (availW - iconSize) / 2 - adjustHardcodedPixelSize(4);
            iconY = startY;
            if (m_showPercentage && m_batteryPresent && !m_text.isEmpty()) {
                // Add spacing between icon and text (use adjustHardcodedPixelSize for DPI scaling)
                startY += iconSize + adjustHardcodedPixelSize(16);
            }
        }
        
        if (m_showPercentage && m_batteryPresent && !m_text.isEmpty()) {
            textX = (availW - fm.horizontalAdvance(m_text)) / 2;
            // Position text baseline at startY, accounting for font ascent
            // This centers the text visually within its allocated space
            textY = startY + (fm.height() - fm.ascent()) / 2;
        }
    }
    
    if (m_iconItem) {
        m_iconItem->setPos(iconX, iconY);
    }
    
    if (m_textItem) {
        m_textItem->setPos(textX, textY);
    }
}

void BatteryApplet::updateContent()
{
    if (!m_textItem || !m_panelWindow)
        return;

    updateBatteryInfo();
    
    // Update text
    if (m_showPercentage && m_batteryPresent) {
        m_text = QString::number(m_batteryPercentage) + "%";
    } else if (!m_batteryPresent) {
        // Show "N/A" or empty when no battery
        m_text = "";
    } else {
        m_text = "";
    }
    
    m_textItem->setText(m_text);
    m_textItem->setVisible(m_showPercentage && m_batteryPresent && !m_text.isEmpty());
    
    // Update icon
    if (m_showIcon) {
        QString iconName = getBatteryIconName(m_batteryPercentage, m_batteryCharging, m_batteryPresent);
        const int iconSize = adjustHardcodedPixelSize(24);

        // Cache theme resolution; this can be surprisingly expensive on some systems.
        if (iconName != m_lastIconName || iconSize != m_lastIconSize) {
            m_lastIconName = iconName;
            m_lastIconSize = iconSize;

            QIcon icon = QIcon::fromTheme(iconName);
            if (icon.isNull()) {
                // Fallback to generic battery icon
                icon = QIcon::fromTheme("battery");
            }

            QPixmap pixmap = icon.pixmap(iconSize, iconSize);
            if (m_iconItem) {
                m_iconItem->setPixmap(pixmap);
            }
        }
        if (m_iconItem) {
            m_iconItem->setVisible(true);
        }
    } else {
        if (m_iconItem) {
            m_iconItem->setVisible(false);
        }
    }
    
    // Update tooltip
    QString tooltipText;
    if (m_batteryPresent) {
        if (m_batteryCharging) {
            tooltipText = tr("Battery: %1% (Charging)").arg(m_batteryPercentage);
        } else {
            tooltipText = tr("Battery: %1%").arg(m_batteryPercentage);
        }
    } else {
        tooltipText = tr("No battery");
    }
    setToolTip(tooltipText);

    layoutChanged();
    update();
}

QSize BatteryApplet::desiredSize()
{
    if (!m_panelWindow)
        return QSize(60, 24);

    const PanelWindow::Position pos = m_panelWindow->position();
    const bool verticalPanel = (pos == PanelWindow::Left || pos == PanelWindow::Right);

    QFontMetrics fm(m_panelWindow->font());

    if (!verticalPanel) {
        // Horizontal: calculate width based on icon + text
        int width = 0;
        if (m_showIcon) {
            width += adjustHardcodedPixelSize(24) + 4;
        }
        if (m_showPercentage && m_batteryPresent) {
            width += fm.horizontalAdvance("100%") + 8;
        }
        if (width == 0) width = adjustHardcodedPixelSize(32); // Minimum width (icon only)
        return QSize(width, m_panelWindow->panelHeight());
    }

    // Vertical panel
    int height = 0;
    if (m_showIcon && m_batteryPresent) {
        height += adjustHardcodedPixelSize(24);
    }
    if (m_showPercentage && m_batteryPresent) {
        height += fm.height() + adjustHardcodedPixelSize(4);
    }
    if (height == 0) height = 30; // Minimum height
    
    return QSize(-1, height);
}

void BatteryApplet::scheduleUpdate()
{
    if (m_timer) {
        m_timer->start();
    }
}

void BatteryApplet::clicked()
{
    // Could show detailed battery info dialog here
}

void BatteryApplet::readSettings()
{
    m_showPercentage = Settings::value(m_id, "showPercentage", true).toBool();
    m_showIcon = Settings::value(m_id, "showIcon", true).toBool();
}

void BatteryApplet::showConfigurationDialog()
{
    // TODO: Create configuration dialog
    // For now, just toggle settings
    m_showPercentage = !m_showPercentage;
    Settings::setValue(m_id, "showPercentage", m_showPercentage);
    updateContent();
}

void BatteryApplet::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    HPopupMenu menu;

    menu.addTitle(tr("Battery Applet"));
    menu.addAction(QIcon::fromTheme("preferences-other"), tr("Configure Battery"), this, SLOT(showConfigurationDialog()));

    menu.addTitle(tr("Panel"));
    menu.addAction(QIcon::fromTheme("preferences-desktop"), tr("Configure Panel"), m_panelWindow, SLOT(showConfigurationDialog()));

    menu.addAction(QIcon::fromTheme("list-add"), tr("Add Panel"), QApplication::instance(), SLOT(addPanel()));
    menu.addAction(QIcon::fromTheme("list-remove"), tr("Remove Panel"), m_panelWindow, SLOT(removePanel()));

    menu.exec(event->screenPos());
}

void BatteryApplet::updateBatteryInfo()
{
    if (m_batteryDevicePath.isEmpty()) {
        m_batteryPresent = false;
        return;
    }
    
    // Read battery capacity from sysfs
    QString capacityStr;
    if (!readBatteryFile(m_batteryDevicePath + "/capacity", capacityStr)) {
        m_batteryPresent = false;
        return;
    }
    
    bool ok;
    int capacity = capacityStr.trimmed().toInt(&ok);
    if (!ok || capacity < 0 || capacity > 100) {
        m_batteryPresent = false;
        return;
    }
    m_batteryPercentage = capacity;
    
    // Read battery status (Charging, Discharging, Full, etc.)
    QString statusStr;
    if (readBatteryFile(m_batteryDevicePath + "/status", statusStr)) {
        statusStr = statusStr.trimmed().toLower();
        m_batteryCharging = (statusStr == "charging" || statusStr == "full");
    } else {
        // If we can't read status, assume discharging
        m_batteryCharging = false;
    }
    
    m_batteryPresent = true;
}

QString BatteryApplet::getBatteryIconName(int percentage, bool charging, bool present)
{
    if (!present) {
        return "battery-missing";
    }
    
    if (charging) {
        if (percentage >= 90) return "battery-full-charging";
        if (percentage >= 75) return "battery-good-charging";
        if (percentage >= 50) return "battery-medium-charging";
        if (percentage >= 25) return "battery-low-charging";
        return "battery-caution-charging";
    } else {
        if (percentage >= 90) return "battery-full";
        if (percentage >= 75) return "battery-good";
        if (percentage >= 50) return "battery-medium";
        if (percentage >= 25) return "battery-low";
        if (percentage >= 10) return "battery-caution";
        return "battery-empty";
    }
}

QString BatteryApplet::findBatteryDevice()
{
    // Look for battery devices in /sys/class/power_supply
    QDir powerSupplyDir("/sys/class/power_supply");
    if (!powerSupplyDir.exists()) {
        return QString();
    }
    
    // List all power supply devices
    QStringList entries = powerSupplyDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QString& entry : entries) {
        QString devicePath = powerSupplyDir.absoluteFilePath(entry);
        
        // Check if it has capacity file first (faster check)
        QString capacityFile = devicePath + "/capacity";
        if (!QFile::exists(capacityFile)) {
            continue;
        }
        
        // Check if this is a battery (has type file with "Battery" value)
        QString typeFile = devicePath + "/type";
        QString typeContent;
        if (readBatteryFile(typeFile, typeContent)) {
            QString type = typeContent.trimmed().toLower();
            if (type == "battery") {
                return devicePath;
            }
        } else {
            // If we can't read type file, but capacity exists, assume it's a battery
            // (some systems don't have type file)
            return devicePath;
        }
    }
    
    // Fallback: look for BAT0 or BAT1 (common battery names)
    QStringList commonNames = {"BAT0", "BAT1", "battery"};
    for (const QString& name : commonNames) {
        QString devicePath = powerSupplyDir.absoluteFilePath(name);
        if (QFile::exists(devicePath + "/capacity")) {
            return devicePath;
        }
    }
    
    return QString();
}

bool BatteryApplet::readBatteryFile(const QString& path, QString& content)
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    // Use raw readAll (avoid QTextStream overhead / locale / buffering edge cases).
    content = QString::fromUtf8(file.readAll());
    return true;
}

