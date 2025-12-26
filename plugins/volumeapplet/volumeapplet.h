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

#ifndef VOLUMEAPPLET_H
#define VOLUMEAPPLET_H

#include "../../lib/applet.h"
#include <QtCore>
#include <QtDBus>

class QTimer;
class TextGraphicsItem;
class QGraphicsPixmapItem;
class VolumeDialog;

class VolumeApplet: public Applet
{
	Q_OBJECT
public:
    VolumeApplet(PanelWindow* panelWindow = 0);
	~VolumeApplet();
    void close();
    void setPanelWindow(PanelWindow* panelWindow);

	bool init();
    void startPlugin(){}

    QSize desiredSize();

public slots:
    void fontChanged();
    void clicked();
    void showConfigurationDialog();
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;

protected:
	void layoutChanged();
    bool isHighlighted(){ return isUnderMouse();}
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private slots:
	void updateContent();
    void onVolumeChanged();
    void onMuteChanged();

private:
	void scheduleUpdate();
    void readSettings();
    void updateVolumeInfo();
    QString getVolumeIconName(int volume, bool muted);
    void setVolume(int volume);
    void increaseVolume();
    void decreaseVolume();
    void toggleMute();
    int getVolume();
    bool isMuted();
    QList<QVariantMap> getSinks();
    QList<QVariantMap> getSources();
    
    friend class VolumeDialog;
    void showMixerDialog();

	QTimer* m_timer = nullptr;
	QString m_text;
	TextGraphicsItem* m_textItem = nullptr;
    QGraphicsPixmapItem* m_iconItem = nullptr;
    
    // Volume info
    int m_volume = 0;
    bool m_muted = false;
    bool m_showPercentage = true;
    bool m_showIcon = true;
    
    // PulseAudio D-Bus interface
    QDBusInterface* m_pulseInterface = nullptr;
    QDBusInterface* m_coreInterface = nullptr;
    
    // Mixer dialog
    VolumeDialog* m_mixerDialog = nullptr;
    
    // Cache icon lookups
    QString m_lastIconName;
    int m_lastIconSize = 0;
    
    // Volume step (percentage points)
    int m_volumeStep = 5;
};



class VolumeAppletPlugin: public QObject, public AppletPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "hde.panel.appletplugin")
    Q_INTERFACES(AppletPlugin)

public:
    VolumeAppletPlugin(){}
    ~VolumeAppletPlugin(){}

    Applet* createApplet(PanelWindow* panelWindow) {return new VolumeApplet(panelWindow);}
    QString name() const override { return tr("Volume"); }
};
#endif

