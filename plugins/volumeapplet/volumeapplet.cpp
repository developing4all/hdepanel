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
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "volumeapplet.h"
#include "volumedialog.h"
#include <QtCore/QTimer>
#include <settings.h>
#include <QMenu>
#include "hpopupmenu.h"
#include <QApplication>
#if QT_VERSION >= 0x050000
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneWheelEvent>
#else
#include <QtGui/QGraphicsSceneContextMenuEvent>
#include <QtGui/QGraphicsSceneWheelEvent>
#endif
#include <QFile>
#include <QDir>
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
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusMessage>
#include <QProcess>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QRegularExpression>
#else
#include <QRegExp>
#endif
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "../../lib/dpisupport.h"
#include <QDebug>

VolumeApplet::VolumeApplet(PanelWindow* panelWindow)
	: Applet(panelWindow)
{
    setObjectName("Volume");
    
    // Create text and icon items early
    m_textItem = new TextGraphicsItem(this);
    m_textItem->setColor(Qt::white);
    
    m_iconItem = new QGraphicsPixmapItem(this);
    
    // Initialize PulseAudio D-Bus interface
    m_pulseInterface = new QDBusInterface(
        "org.PulseAudio1",
        "/org/pulseaudio/core1",
        "org.freedesktop.DBus.Properties",
        QDBusConnection::sessionBus(),
        this
    );
    
    m_coreInterface = new QDBusInterface(
        "org.PulseAudio1",
        "/org/pulseaudio/core1",
        "org.PulseAudio.Core1",
        QDBusConnection::sessionBus(),
        this
    );
}

void VolumeApplet::setPanelWindow(PanelWindow *panelWindow)
{
    Applet::setPanelWindow(panelWindow);

    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setSingleShot(false);
        m_timer->setInterval(1000); // Update every second
        connect(m_timer, SIGNAL(timeout()), this, SLOT(updateContent()));
    }

    if (m_textItem && m_panelWindow) {
        m_textItem->setFont(m_panelWindow->font());
    }
}

VolumeApplet::~VolumeApplet()
{
    close();
}

void VolumeApplet::close()
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
    
    if (m_mixerDialog) {
        delete m_mixerDialog;
        m_mixerDialog = nullptr;
    }
}

void VolumeApplet::fontChanged()
{
    if (m_textItem && m_panelWindow) {
        m_textItem->setFont(m_panelWindow->font());
        updateContent();
    }
}

bool VolumeApplet::init()
{
    setInteractive(true);
    readSettings();
    
    // Force initial icon update
    if (m_showIcon && m_iconItem) {
        m_lastIconName = "";
        m_lastIconSize = 0;
    }
    
    updateContent();
    
    // Ensure icon is visible after first update
    if (m_showIcon && m_iconItem) {
        if (m_iconItem->pixmap().isNull()) {
            // Create a fallback icon if still null
            const int iconSize = adjustHardcodedPixelSize(24);
            QPixmap pixmap(iconSize, iconSize);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(QColor(100, 150, 255));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(2, 2, iconSize - 4, iconSize - 4);
            m_iconItem->setPixmap(pixmap);
        }
        m_iconItem->setVisible(true);
    }
    
    if (m_timer) {
        m_timer->start();
    }
    
    // Connect to PulseAudio signals if available
    QDBusConnection::sessionBus().connect(
        "org.PulseAudio1",
        "/org/pulseaudio/core1",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        this,
        SLOT(onVolumeChanged())
    );
    
    return true;
}

void VolumeApplet::layoutChanged()
{
    if (!m_textItem || !m_panelWindow)
        return;

    const PanelWindow::Position pos = m_panelWindow->position();
    const bool verticalPanel = (pos == PanelWindow::Left || pos == PanelWindow::Right);

    const int availW = (m_size.width() > 0 ? m_size.width() : (verticalPanel ? m_panelWindow->panelWidth()
                                                                            : m_panelWindow->panelHeight()));
    const int availH = (m_size.height() > 0 ? m_size.height() : (verticalPanel ? m_panelWindow->panelHeight()
                                                                               : m_panelWindow->panelWidth()));

    QFontMetrics fm(m_panelWindow->font());
    const int iconSize = adjustHardcodedPixelSize(24);
    
    int iconX = 0, iconY = 0;
    int textX = 0, textY = 0;
    
    if (!verticalPanel) {
        // Horizontal panel: icon and text side by side
        int contentWidth = 0;
        if (m_showIcon) {
            contentWidth += iconSize;
        }
        if (m_showPercentage && !m_text.isEmpty()) {
            if (m_showIcon) {
                contentWidth += adjustHardcodedPixelSize(6); // spacing
            }
            contentWidth += fm.horizontalAdvance(m_text);
        }
        
        int startX = (availW - contentWidth) / 2;
        if (startX < 0) startX = 0;
        
        if (m_showIcon) {
            iconX = startX;
            iconY = (availH - iconSize) / 2;
            if (m_showPercentage && !m_text.isEmpty()) {
                startX += iconSize + adjustHardcodedPixelSize(6);
            }
        }
        
        if (m_showPercentage && !m_text.isEmpty()) {
            textX = startX;
            textY = (availH - fm.height()) / 2 + fm.ascent();
        }
    } else {
        // Vertical panel: icon and text stacked
        int appletHeight = availH;
        int contentHeight = 0;
        if (m_showIcon) {
            contentHeight += iconSize;
        }
        if (m_showPercentage && !m_text.isEmpty()) {
            contentHeight += fm.height();
        }
        
        int startY = (appletHeight - contentHeight) / 2;
        if (startY < 0) startY = 0;
        
        if (m_showIcon) {
            iconX = (availW - iconSize) / 2;
            iconY = startY;
            if (m_showPercentage && !m_text.isEmpty()) {
                startY += iconSize + adjustHardcodedPixelSize(4);
            }
        }
        
        if (m_showPercentage && !m_text.isEmpty()) {
            textX = (availW - fm.horizontalAdvance(m_text)) / 2;
            textY = (startY + (fm.height() - fm.ascent()) / 2) + adjustHardcodedPixelSize(4);
        }
    }
    
    if (m_iconItem) {
        m_iconItem->setPos(iconX, iconY);
    }
    
    if (m_textItem) {
        m_textItem->setPos(textX, textY);
    }
}

void VolumeApplet::updateContent()
{
    if (!m_textItem || !m_panelWindow)
        return;

    updateVolumeInfo();
    
    // Update text
    if (m_showPercentage) {
        m_text = QString::number(m_volume) + "%";
    } else {
        m_text = "";
    }
    
    m_textItem->setText(m_text);
    m_textItem->setVisible(m_showPercentage && !m_text.isEmpty());
    
    // Update icon
    if (m_showIcon) {
        QString iconName = getVolumeIconName(m_volume, m_muted);
        const int iconSize = adjustHardcodedPixelSize(24);

        // Always update if icon name changed, size changed, or icon item doesn't have a pixmap
        bool needsUpdate = (iconName != m_lastIconName || iconSize != m_lastIconSize);
        if (m_iconItem && needsUpdate) {
            QPixmap currentPixmap = m_iconItem->pixmap();
            if (currentPixmap.isNull() || currentPixmap.size() != QSize(iconSize, iconSize)) {
                needsUpdate = true;
            }
        }

        if (needsUpdate) {
            m_lastIconName = iconName;
            m_lastIconSize = iconSize;

            QIcon icon = QIcon::fromTheme(iconName);
            if (icon.isNull()) {
                // Try fallback icons
                icon = QIcon::fromTheme("audio-volume-medium");
                if (icon.isNull()) {
                    icon = QIcon::fromTheme("audio-volume-high");
                    if (icon.isNull()) {
                        icon = QIcon::fromTheme("audio");
                        if (icon.isNull()) {
                            icon = QIcon::fromTheme("multimedia-volume-control");
                        }
                    }
                }
            }

            QPixmap pixmap = icon.pixmap(iconSize, iconSize);
            // If still null, create a simple colored rectangle as fallback
            if (pixmap.isNull()) {
                pixmap = QPixmap(iconSize, iconSize);
                pixmap.fill(Qt::transparent);
                QPainter painter(&pixmap);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setBrush(QColor(100, 150, 255));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(2, 2, iconSize - 4, iconSize - 4);
            }
            
            if (m_iconItem) {
                m_iconItem->setPixmap(pixmap);
                m_iconItem->setVisible(true);
            }
        } else {
            // Ensure icon is visible even if we didn't update it
            if (m_iconItem) {
                m_iconItem->setVisible(true);
            }
        }
    } else {
        if (m_iconItem) {
            m_iconItem->setVisible(false);
        }
    }
    
    // Update tooltip
    QString tooltipText;
    if (m_muted) {
        tooltipText = tr("Volume: Muted");
    } else {
        tooltipText = tr("Volume: %1%").arg(m_volume);
    }
    setToolTip(tooltipText);
    
    layoutChanged();
    update();
}

void VolumeApplet::updateVolumeInfo()
{
    m_volume = getVolume();
    m_muted = isMuted();
}

QString VolumeApplet::getVolumeIconName(int volume, bool muted)
{
    if (muted) {
        return "audio-volume-muted";
    } else if (volume == 0) {
        return "audio-volume-zero";
    } else if (volume < 33) {
        return "audio-volume-low";
    } else if (volume < 66) {
        return "audio-volume-medium";
    } else {
        return "audio-volume-high";
    }
}

int VolumeApplet::getVolume()
{
    // Try PulseAudio D-Bus first
    if (m_coreInterface && m_coreInterface->isValid()) {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            "org.PulseAudio1",
            "/org/pulseaudio/core1",
            "org.PulseAudio.Core1",
            "GetSinkInfoList"
        );
        QDBusReply<QVariant> reply = QDBusConnection::sessionBus().call(msg);
        if (reply.isValid()) {
            QVariantList args = reply.value().toList();
            if (!args.isEmpty()) {
                // Get the default sink (first one)
                QVariantMap sink = args[0].toMap();
                QVariant volume = sink.value("Volume");
                if (volume.isValid()) {
                    QVariantList volumes = volume.toList();
                    if (!volumes.isEmpty()) {
                        // PulseAudio uses 0-65535, convert to 0-100
                        int vol = volumes[0].toInt();
                        return (vol * 100) / 65535;
                    }
                }
            }
        }
    }
    
    // Fallback to pactl command
    QProcess process;
    process.start("pactl", QStringList() << "get-sink-volume" << "@DEFAULT_SINK@");
    process.waitForFinished(1000);
    if (process.exitCode() == 0) {
        QByteArray output = process.readAllStandardOutput();
        QString outputStr = QString::fromUtf8(output);
        // Parse output like "Volume: front-left: 32768 /  50% / -18.06 dB"
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QRegularExpression rx("(\\d+)%");
        QRegularExpressionMatch match = rx.match(outputStr);
        if (match.hasMatch()) {
            return match.captured(1).toInt();
        }
#else
        QRegExp rx("(\\d+)%");
        if (rx.indexIn(outputStr) != -1) {
            return rx.cap(1).toInt();
        }
#endif
    }
    
    return 50; // Default
}

bool VolumeApplet::isMuted()
{
    // Try PulseAudio D-Bus first
    if (m_coreInterface && m_coreInterface->isValid()) {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            "org.PulseAudio1",
            "/org/pulseaudio/core1",
            "org.PulseAudio.Core1",
            "GetSinkInfoList"
        );
        QDBusReply<QVariant> reply = QDBusConnection::sessionBus().call(msg);
        if (reply.isValid()) {
            QVariantList args = reply.value().toList();
            if (!args.isEmpty()) {
                QVariantMap sink = args[0].toMap();
                QVariant mute = sink.value("Mute");
                if (mute.isValid()) {
                    return mute.toBool();
                }
            }
        }
    }
    
    // Fallback to pactl command
    QProcess process;
    process.start("pactl", QStringList() << "get-sink-mute" << "@DEFAULT_SINK@");
    process.waitForFinished(1000);
    if (process.exitCode() == 0) {
        QByteArray output = process.readAllStandardOutput();
        QString outputStr = QString::fromUtf8(output);
        return outputStr.contains("yes", Qt::CaseInsensitive);
    }
    
    return false;
}

void VolumeApplet::setVolume(int volume)
{
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    // Convert to PulseAudio range (0-65535)
    int paVolume = (volume * 65535) / 100;
    
    // Use pactl command (more reliable than D-Bus)
    QProcess process;
    process.start("pactl", QStringList() << "set-sink-volume" << "@DEFAULT_SINK@" << QString::number(paVolume));
    process.waitForFinished(1000);
    
    updateContent();
}

void VolumeApplet::increaseVolume()
{
    int current = getVolume();
    setVolume(current + m_volumeStep);
}

void VolumeApplet::decreaseVolume()
{
    int current = getVolume();
    setVolume(current - m_volumeStep);
}

void VolumeApplet::toggleMute()
{
    QProcess process;
    process.start("pactl", QStringList() << "set-sink-mute" << "@DEFAULT_SINK@" << "toggle");
    process.waitForFinished(1000);
    updateContent();
}

void VolumeApplet::clicked()
{
    // Toggle the mixer dialog
    if (m_mixerDialog && m_mixerDialog->isVisible()) {
        // Dialog is open, close it
        m_mixerDialog->hide();
    } else {
        // Dialog is closed or doesn't exist, show it
        showMixerDialog();
    }
}

void VolumeApplet::showMixerDialog()
{
    if (!m_mixerDialog) {
        m_mixerDialog = new VolumeDialog(this);
    }
    
    if (m_mixerDialog) {
        m_mixerDialog->updateVolumes();
        
        if (!m_panelWindow) return;
        
        QPoint pos = localToScreen(QPoint(0, 0));
        QSize dialogSize = m_mixerDialog->sizeHint();
        
        // Position dialog above or below panel
        PanelWindow::Position panelPos = m_panelWindow->position();
        if (panelPos == PanelWindow::Top) {
            pos.setY(pos.y() + m_size.height() + 5);
        } else if (panelPos == PanelWindow::Bottom) {
            pos.setY(pos.y() - dialogSize.height() - 5);
        } else if (panelPos == PanelWindow::Left) {
            pos.setX(pos.x() + m_size.width() + 5);
        } else {
            pos.setX(pos.x() - dialogSize.width() - 5);
        }
        
        m_mixerDialog->move(pos);
        m_mixerDialog->show();
        m_mixerDialog->raise();
        m_mixerDialog->setFocus();
        m_mixerDialog->activateWindow();
    }
}

void VolumeApplet::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if (event->delta() > 0) {
        increaseVolume();
    } else {
        decreaseVolume();
    }
    event->accept();
}

void VolumeApplet::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;
    
    QAction *muteAction = menu.addAction(m_muted ? tr("Unmute") : tr("Mute"));
    connect(muteAction, &QAction::triggered, this, &VolumeApplet::toggleMute);
    
    menu.addSeparator();
    
    QAction *mixerAction = menu.addAction(tr("Mixer"));
    connect(mixerAction, &QAction::triggered, this, &VolumeApplet::showMixerDialog);
    
    menu.exec(event->screenPos());
}

void VolumeApplet::onVolumeChanged()
{
    updateContent();
}

void VolumeApplet::onMuteChanged()
{
    updateContent();
}

QList<QVariantMap> VolumeApplet::getSinks()
{
    QList<QVariantMap> sinks;
    
    QProcess process;
    process.start("pactl", QStringList() << "-f" << "json" << "list" << "sinks");
    process.waitForFinished(2000);
    if (process.exitCode() == 0) {
        QByteArray output = process.readAllStandardOutput();
        QJsonDocument doc = QJsonDocument::fromJson(output);
        if (doc.isArray()) {
            QJsonArray array = doc.array();
            for (const QJsonValue &value : array) {
                if (value.isObject()) {
                    QJsonObject obj = value.toObject();
                    QVariantMap map;
                    map["name"] = obj.value("name").toString();
                    map["description"] = obj.value("description").toString();
                    // Parse volume
                    QJsonValue volume = obj.value("volume");
                    if (volume.isObject()) {
                        QJsonObject volObj = volume.toObject();
                        QJsonValue frontLeft = volObj.value("front-left");
                        if (frontLeft.isObject()) {
                            int vol = frontLeft.toObject().value("value").toInt();
                            map["volume"] = (vol * 100) / 65535;
                        }
                    }
                    // Parse mute
                    QJsonValue mute = obj.value("mute");
                    map["muted"] = mute.toBool();
                    sinks.append(map);
                }
            }
        }
    }
    
    return sinks;
}

QList<QVariantMap> VolumeApplet::getSources()
{
    QList<QVariantMap> sources;
    
    QProcess process;
    process.start("pactl", QStringList() << "-f" << "json" << "list" << "sources");
    process.waitForFinished(2000);
    if (process.exitCode() == 0) {
        QByteArray output = process.readAllStandardOutput();
        QJsonDocument doc = QJsonDocument::fromJson(output);
        if (doc.isArray()) {
            QJsonArray array = doc.array();
            for (const QJsonValue &value : array) {
                if (value.isObject()) {
                    QJsonObject obj = value.toObject();
                    // Skip monitor sources
                    if (obj.value("name").toString().contains(".monitor")) {
                        continue;
                    }
                    QVariantMap map;
                    map["name"] = obj.value("name").toString();
                    map["description"] = obj.value("description").toString();
                    // Parse volume
                    QJsonValue volume = obj.value("volume");
                    if (volume.isObject()) {
                        QJsonObject volObj = volume.toObject();
                        QJsonValue frontLeft = volObj.value("front-left");
                        if (frontLeft.isObject()) {
                            int vol = frontLeft.toObject().value("value").toInt();
                            map["volume"] = (vol * 100) / 65535;
                        }
                    }
                    // Parse mute
                    QJsonValue mute = obj.value("mute");
                    map["muted"] = mute.toBool();
                    sources.append(map);
                }
            }
        }
    }
    
    return sources;
}

void VolumeApplet::readSettings()
{
    m_showPercentage = Settings::value(id(), "showPercentage", true).toBool();
    m_showIcon = Settings::value(id(), "showIcon", true).toBool();
    m_volumeStep = Settings::value(id(), "volumeStep", 5).toInt();
}

void VolumeApplet::showConfigurationDialog()
{
    // TODO: Implement configuration dialog
}

QSize VolumeApplet::desiredSize()
{
    if (!m_panelWindow) {
        return QSize(50, 30);
    }

    const PanelWindow::Position pos = m_panelWindow->position();
    const bool verticalPanel = (pos == PanelWindow::Left || pos == PanelWindow::Right);
    QFontMetrics fm(m_panelWindow->font());
    const int iconSize = adjustHardcodedPixelSize(24);
    const int padding = adjustHardcodedPixelSize(8);

    if (!verticalPanel) {
        int width = padding * 2;
        if (m_showIcon) {
            width += iconSize;
        }
        if (m_showPercentage) {
            if (m_showIcon) {
                width += adjustHardcodedPixelSize(6);
            }
            width += fm.horizontalAdvance("100%");
        }
        return QSize(width, m_panelWindow->panelHeight());
    } else {
        // Vertical panel: minimize height like battery applet
        int height = 0;
        if (m_showIcon) {
            height += iconSize;
        }
        if (m_showPercentage) {
            height += fm.height() + adjustHardcodedPixelSize(4);
        }
        if (height == 0) height = 30; // Minimum height
        return QSize(-1, height);
    }
}

void VolumeApplet::scheduleUpdate()
{
    if (m_timer && !m_timer->isActive()) {
        m_timer->start();
    }
}

