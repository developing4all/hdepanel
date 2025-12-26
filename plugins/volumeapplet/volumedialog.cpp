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

#include "volumedialog.h"
#include "volumeapplet.h"
#include <QApplication>
#include <QScreen>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QFocusEvent>
#include <QEvent>

VolumeDialog::VolumeDialog(VolumeApplet* applet, QWidget* parent)
    : QWidget(parent)
    , m_applet(applet)
    , m_mainLayout(nullptr)
    , m_scrollArea(nullptr)
    , m_scrollContent(nullptr)
    , m_contentLayout(nullptr)
    , m_updateTimer(nullptr)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_DeleteOnClose, false);
    
    setMinimumWidth(300);
    setMaximumWidth(400);
    setMinimumHeight(200);
    setMaximumHeight(600);
    
    setupUI();
    
    // Auto-update every 2 seconds
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(2000);
    connect(m_updateTimer, &QTimer::timeout, this, &VolumeDialog::onUpdateTimer);
    m_updateTimer->start();
    
    // Set focus policy to receive focus events
    setFocusPolicy(Qt::ClickFocus);
}

VolumeDialog::~VolumeDialog()
{
}

void VolumeDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(0);
    
    // Title
    QLabel* titleLabel = new QLabel(tr("Volume Mixer"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: white; padding: 5px;");
    m_mainLayout->addWidget(titleLabel);
    
    // Scroll area
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet("QScrollArea { background: transparent; }");
    
    m_scrollContent = new QWidget();
    m_contentLayout = new QVBoxLayout(m_scrollContent);
    m_contentLayout->setContentsMargins(0, 5, 0, 5);
    m_contentLayout->setSpacing(10);
    m_contentLayout->addStretch();
    
    m_scrollArea->setWidget(m_scrollContent);
    m_mainLayout->addWidget(m_scrollArea);
    
    setStyleSheet(
        "QWidget { background-color: rgba(40, 40, 40, 240); border: 1px solid rgba(100, 100, 100, 200); border-radius: 8px; }"
        "QLabel { color: white; }"
        "QSlider::groove:horizontal { height: 4px; background: rgba(100, 100, 100, 150); border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 16px; height: 16px; background: white; border-radius: 8px; margin: -6px 0; }"
        "QSlider::sub-page:horizontal { background: #4CAF50; border-radius: 2px; }"
        "QPushButton { color: white; padding: 4px 8px; border: 1px solid rgba(100, 100, 100, 150); border-radius: 4px; background: rgba(60, 60, 60, 200); }"
        "QPushButton:hover { background-color: rgba(80, 80, 80, 200); }"
        "QPushButton:pressed { background-color: rgba(100, 100, 100, 200); }"
    );
}

void VolumeDialog::updateVolumes()
{
    clearControls();
    
    // Get sinks (output devices)
    QList<QVariantMap> sinks = m_applet->getSinks();
    for (const QVariantMap& sink : sinks) {
        addSinkControl(
            sink.value("name").toString(),
            sink.value("description").toString(),
            sink.value("volume").toInt(),
            sink.value("muted").toBool()
        );
    }
    
    // Get sources (input devices)
    QList<QVariantMap> sources = m_applet->getSources();
    if (!sources.isEmpty()) {
        // Add separator
        QFrame* separator = new QFrame(m_scrollContent);
        separator->setFrameShape(QFrame::HLine);
        separator->setStyleSheet("color: rgba(150, 150, 150, 200);");
        m_contentLayout->insertWidget(m_contentLayout->count() - 1, separator);
        
        QLabel* inputLabel = new QLabel(tr("Input Devices"), m_scrollContent);
        QFont labelFont = inputLabel->font();
        labelFont.setBold(true);
        inputLabel->setFont(labelFont);
        inputLabel->setStyleSheet("color: white; padding: 5px 0;");
        m_contentLayout->insertWidget(m_contentLayout->count() - 1, inputLabel);
    }
    
    for (const QVariantMap& source : sources) {
        addSourceControl(
            source.value("name").toString(),
            source.value("description").toString(),
            source.value("volume").toInt(),
            source.value("muted").toBool()
        );
    }
    
    m_scrollContent->adjustSize();
    adjustSize();
}

void VolumeDialog::addSinkControl(const QString& name, const QString& description, int volume, bool muted)
{
    QFrame* frame = new QFrame(m_scrollContent);
    frame->setStyleSheet("QFrame { background: transparent; }");
    QVBoxLayout* frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(5, 5, 5, 5);
    frameLayout->setSpacing(5);
    
    // Description label
    QLabel* descLabel = new QLabel(description, frame);
    descLabel->setStyleSheet("color: white; font-weight: bold;");
    frameLayout->addWidget(descLabel);
    
    // Volume control row
    QHBoxLayout* controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(10);
    
    // Mute button
    QPushButton* muteButton = new QPushButton(muted ? tr("Unmute") : tr("Mute"), frame);
    muteButton->setFixedWidth(60);
    muteButton->setProperty("sinkName", name);
    muteButton->setProperty("isSource", false);
    connect(muteButton, &QPushButton::clicked, this, &VolumeDialog::onMuteToggled);
    controlLayout->addWidget(muteButton);
    
    // Volume slider
    QSlider* slider = new QSlider(Qt::Horizontal, frame);
    slider->setRange(0, 100);
    slider->setValue(volume);
    slider->setProperty("sinkName", name);
    slider->setProperty("isSource", false);
    connect(slider, &QSlider::valueChanged, this, &VolumeDialog::onVolumeChanged);
    controlLayout->addWidget(slider);
    
    // Volume label
    QLabel* volumeLabel = new QLabel(QString::number(volume) + "%", frame);
    volumeLabel->setStyleSheet("color: white; min-width: 40px;");
    volumeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    controlLayout->addWidget(volumeLabel);
    
    frameLayout->addLayout(controlLayout);
    
    Control control;
    control.name = name;
    control.slider = slider;
    control.muteButton = muteButton;
    control.volumeLabel = volumeLabel;
    control.isSource = false;
    m_controls.append(control);
    
    m_contentLayout->insertWidget(m_contentLayout->count() - 1, frame);
}

void VolumeDialog::addSourceControl(const QString& name, const QString& description, int volume, bool muted)
{
    QFrame* frame = new QFrame(m_scrollContent);
    frame->setStyleSheet("QFrame { background: transparent; }");
    QVBoxLayout* frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(5, 5, 5, 5);
    frameLayout->setSpacing(5);
    
    // Description label
    QLabel* descLabel = new QLabel(description, frame);
    descLabel->setStyleSheet("color: white; font-weight: bold;");
    frameLayout->addWidget(descLabel);
    
    // Volume control row
    QHBoxLayout* controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(10);
    
    // Mute button
    QPushButton* muteButton = new QPushButton(muted ? tr("Unmute") : tr("Mute"), frame);
    muteButton->setFixedWidth(60);
    muteButton->setProperty("sourceName", name);
    muteButton->setProperty("isSource", true);
    connect(muteButton, &QPushButton::clicked, this, &VolumeDialog::onSourceMuteToggled);
    controlLayout->addWidget(muteButton);
    
    // Volume slider
    QSlider* slider = new QSlider(Qt::Horizontal, frame);
    slider->setRange(0, 100);
    slider->setValue(volume);
    slider->setProperty("sourceName", name);
    slider->setProperty("isSource", true);
    connect(slider, &QSlider::valueChanged, this, &VolumeDialog::onSourceVolumeChanged);
    controlLayout->addWidget(slider);
    
    // Volume label
    QLabel* volumeLabel = new QLabel(QString::number(volume) + "%", frame);
    volumeLabel->setStyleSheet("color: white; min-width: 40px;");
    volumeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    controlLayout->addWidget(volumeLabel);
    
    frameLayout->addLayout(controlLayout);
    
    Control control;
    control.name = name;
    control.slider = slider;
    control.muteButton = muteButton;
    control.volumeLabel = volumeLabel;
    control.isSource = true;
    m_controls.append(control);
    
    m_contentLayout->insertWidget(m_contentLayout->count() - 1, frame);
}

void VolumeDialog::onVolumeChanged(int value)
{
    QSlider* slider = qobject_cast<QSlider*>(sender());
    if (!slider) return;
    
    QString sinkName = slider->property("sinkName").toString();
    if (sinkName.isEmpty()) return;
    
    setSinkVolume(sinkName, value);
    
    // Update label
    for (Control& control : m_controls) {
        if (control.slider == slider && !control.isSource) {
            control.volumeLabel->setText(QString::number(value) + "%");
            break;
        }
    }
}

void VolumeDialog::onMuteToggled()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    QString sinkName = button->property("sinkName").toString();
    if (sinkName.isEmpty()) return;
    
    // Get current mute state
    bool currentlyMuted = (button->text() == tr("Unmute"));
    setSinkMute(sinkName, !currentlyMuted);
    
    // Update button text
    button->setText(currentlyMuted ? tr("Mute") : tr("Unmute"));
}

void VolumeDialog::onSourceVolumeChanged(int value)
{
    QSlider* slider = qobject_cast<QSlider*>(sender());
    if (!slider) return;
    
    QString sourceName = slider->property("sourceName").toString();
    if (sourceName.isEmpty()) return;
    
    setSourceVolume(sourceName, value);
    
    // Update label
    for (Control& control : m_controls) {
        if (control.slider == slider && control.isSource) {
            control.volumeLabel->setText(QString::number(value) + "%");
            break;
        }
    }
}

void VolumeDialog::onSourceMuteToggled()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    QString sourceName = button->property("sourceName").toString();
    if (sourceName.isEmpty()) return;
    
    // Get current mute state
    bool currentlyMuted = (button->text() == tr("Unmute"));
    setSourceMute(sourceName, !currentlyMuted);
    
    // Update button text
    button->setText(currentlyMuted ? tr("Mute") : tr("Unmute"));
}

void VolumeDialog::setSinkVolume(const QString& sinkName, int volume)
{
    int paVolume = (volume * 65535) / 100;
    QProcess process;
    process.start("pactl", QStringList() << "set-sink-volume" << sinkName << QString::number(paVolume));
    process.waitForFinished(1000);
}

void VolumeDialog::setSinkMute(const QString& sinkName, bool muted)
{
    QProcess process;
    process.start("pactl", QStringList() << "set-sink-mute" << sinkName << (muted ? "1" : "0"));
    process.waitForFinished(1000);
}

void VolumeDialog::setSourceVolume(const QString& sourceName, int volume)
{
    int paVolume = (volume * 65535) / 100;
    QProcess process;
    process.start("pactl", QStringList() << "set-source-volume" << sourceName << QString::number(paVolume));
    process.waitForFinished(1000);
}

void VolumeDialog::setSourceMute(const QString& sourceName, bool muted)
{
    QProcess process;
    process.start("pactl", QStringList() << "set-source-mute" << sourceName << (muted ? "1" : "0"));
    process.waitForFinished(1000);
}

void VolumeDialog::clearControls()
{
    // Remove all widgets except the stretch
    QLayoutItem* item;
    while ((item = m_contentLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    m_contentLayout->addStretch();
    m_controls.clear();
}

void VolumeDialog::onUpdateTimer()
{
    // Periodically refresh volumes from PulseAudio
    updateVolumes();
}

void VolumeDialog::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    // Close dialog when it loses focus (user clicked elsewhere)
    hide();
}

bool VolumeDialog::event(QEvent* event)
{
    // Handle window activation changes
    if (event->type() == QEvent::WindowDeactivate) {
        // Close when window loses activation
        hide();
        return true;
    }
    return QWidget::event(event);
}

