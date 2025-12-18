#include "taskbarconfigurationdialog.h"
#include "ui_taskbarconfigurationdialog.h"

#include <settings.h>
#include <QColorDialog>

TaskBarConfigurationDialog::TaskBarConfigurationDialog(QString id, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TaskBarConfigurationDialog)
{
    ui->setupUi(this);
    m_id = id;

    this->ui->only_current_screen->setChecked(Settings::value(m_id, "only_current_screen", false).toBool());
    this->ui->only_current_desktop->setChecked(Settings::value(m_id, "only_current_desktop", true).toBool());
    this->ui->only_minimized->setChecked(Settings::value(m_id, "only_minimized", false).toBool());

    // Load color settings
    QColor buttonColor = Settings::value(m_id, "buttonColor", QColor(255, 255, 255)).value<QColor>();
    int buttonTransparency = Settings::value(m_id, "buttonColorTransparency", 80).toInt();
    QColor focusColor = Settings::value(m_id, "focusColor", QColor(0, 0, 0)).value<QColor>();
    int focusTransparency = Settings::value(m_id, "focusColorTransparency", 128).toInt();

    if (ui->buttonColorButton) {
        ui->buttonColorButton->setStyleSheet(QString("background-color: %1;").arg(buttonColor.name()));
    }
    if (ui->buttonColorTransparency) {
        ui->buttonColorTransparency->setValue(buttonTransparency);
    }
    if (ui->focusColorButton) {
        ui->focusColorButton->setStyleSheet(QString("background-color: %1;").arg(focusColor.name()));
    }
    if (ui->focusColorTransparency) {
        ui->focusColorTransparency->setValue(focusTransparency);
    }

    // Connect signals
    connect(ui->buttonColorButton, &QPushButton::clicked, this, &TaskBarConfigurationDialog::on_buttonColorButton_clicked);
    connect(ui->focusColorButton, &QPushButton::clicked, this, &TaskBarConfigurationDialog::on_focusColorButton_clicked);
    connect(ui->buttonColorTransparency, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TaskBarConfigurationDialog::on_buttonColorTransparency_changed);
    connect(ui->focusColorTransparency, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TaskBarConfigurationDialog::on_focusColorTransparency_changed);
}

TaskBarConfigurationDialog::~TaskBarConfigurationDialog()
{
    delete ui;
}

void TaskBarConfigurationDialog::on_buttonBox_accepted()
{
    Settings::setValue(m_id, "only_current_screen", ui->only_current_screen->isChecked());
    Settings::setValue(m_id, "only_current_desktop", ui->only_current_desktop->isChecked());
    Settings::setValue(m_id, "only_minimized", ui->only_minimized->isChecked());
    
    // Save color settings
    QColor buttonColor = Settings::value(m_id, "buttonColor", QColor(255, 255, 255)).value<QColor>();
    Settings::setValue(m_id, "buttonColor", buttonColor);
    Settings::setValue(m_id, "buttonColorTransparency", ui->buttonColorTransparency->value());
    
    QColor focusColor = Settings::value(m_id, "focusColor", QColor(0, 0, 0)).value<QColor>();
    Settings::setValue(m_id, "focusColor", focusColor);
    Settings::setValue(m_id, "focusColorTransparency", ui->focusColorTransparency->value());
}

void TaskBarConfigurationDialog::on_buttonColorButton_clicked()
{
    QColor currentColor = Settings::value(m_id, "buttonColor", QColor(255, 255, 255)).value<QColor>();
    QColor color = QColorDialog::getColor(currentColor, this, tr("Choose Button Color"));
    if (color.isValid()) {
        Settings::setValue(m_id, "buttonColor", color);
        ui->buttonColorButton->setStyleSheet(QString("background-color: %1;").arg(color.name()));
    }
}

void TaskBarConfigurationDialog::on_focusColorButton_clicked()
{
    QColor currentColor = Settings::value(m_id, "focusColor", QColor(0, 0, 0)).value<QColor>();
    QColor color = QColorDialog::getColor(currentColor, this, tr("Choose Focus Color"));
    if (color.isValid()) {
        Settings::setValue(m_id, "focusColor", color);
        ui->focusColorButton->setStyleSheet(QString("background-color: %1;").arg(color.name()));
    }
}

void TaskBarConfigurationDialog::on_buttonColorTransparency_changed(int value)
{
    Settings::setValue(m_id, "buttonColorTransparency", value);
}

void TaskBarConfigurationDialog::on_focusColorTransparency_changed(int value)
{
    Settings::setValue(m_id, "focusColorTransparency", value);
}
