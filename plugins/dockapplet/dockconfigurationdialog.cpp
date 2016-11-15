#include "dockconfigurationdialog.h"
#include "ui_dockconfigurationdialog.h"

#include <settings.h>

DockConfigurationDialog::DockConfigurationDialog(QString id, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DockConfigurationDialog)
{
    ui->setupUi(this);
    m_id = id;

    this->ui->only_current_screen->setChecked(Settings::value(m_id, "only_current_screen", false).toBool());
    this->ui->only_current_desktop->setChecked(Settings::value(m_id, "only_current_desktop", true).toBool());
    this->ui->only_minimized->setChecked(Settings::value(m_id, "only_minimized", false).toBool());

}

DockConfigurationDialog::~DockConfigurationDialog()
{
    delete ui;
}

void DockConfigurationDialog::on_buttonBox_accepted()
{
    Settings::setValue(m_id, "only_current_screen", ui->only_current_screen->isChecked());
    Settings::setValue(m_id, "only_current_desktop", ui->only_current_desktop->isChecked());
    Settings::setValue(m_id, "only_minimized", ui->only_minimized->isChecked());
}
