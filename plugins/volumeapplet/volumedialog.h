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

#ifndef VOLUMEDIALOG_H
#define VOLUMEDIALOG_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QTimer>

class VolumeApplet;

class VolumeDialog : public QWidget
{
    Q_OBJECT

public:
    explicit VolumeDialog(VolumeApplet* applet, QWidget* parent = nullptr);
    ~VolumeDialog();

    void updateVolumes();

protected:
    void focusOutEvent(QFocusEvent* event) override;
    bool event(QEvent* event) override;

private slots:
    void onVolumeChanged(int value);
    void onMuteToggled();
    void onSourceVolumeChanged(int value);
    void onSourceMuteToggled();
    void onUpdateTimer();

private:
    void setupUI();
    void addSinkControl(const QString& name, const QString& description, int volume, bool muted);
    void addSourceControl(const QString& name, const QString& description, int volume, bool muted);
    void setSinkVolume(const QString& sinkName, int volume);
    void setSinkMute(const QString& sinkName, bool muted);
    void setSourceVolume(const QString& sourceName, int volume);
    void setSourceMute(const QString& sourceName, bool muted);
    void clearControls();

    VolumeApplet* m_applet;
    QVBoxLayout* m_mainLayout;
    QScrollArea* m_scrollArea;
    QWidget* m_scrollContent;
    QVBoxLayout* m_contentLayout;
    
    QTimer* m_updateTimer;
    
    struct Control {
        QString name;
        QSlider* slider;
        QPushButton* muteButton;
        QLabel* volumeLabel;
        bool isSource;
    };
    
    QList<Control> m_controls;
};

#endif // VOLUMEDIALOG_H

