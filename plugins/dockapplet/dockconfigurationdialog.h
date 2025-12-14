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
    void on_buttonColorButton_clicked();
    void on_focusColorButton_clicked();
    void on_buttonColorTransparency_changed(int value);
    void on_focusColorTransparency_changed(int value);

private:
    Ui::DockConfigurationDialog *ui;
    QString m_id;
};

#endif // DOCKCONFIGURATIONDIALOG_H
