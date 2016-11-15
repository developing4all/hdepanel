#ifndef DOCKCONFIGURATIONDIALOG_H
#define DOCKCONFIGURATIONDIALOG_H

#include <QDialog>

namespace Ui {
class DockConfigurationDialog;
}

class DockConfigurationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DockConfigurationDialog(QString id,QWidget *parent = 0);
    ~DockConfigurationDialog();

private slots:
    void on_buttonBox_accepted();

private:
    Ui::DockConfigurationDialog *ui;
    QString m_id;
};

#endif // DOCKCONFIGURATIONDIALOG_H
