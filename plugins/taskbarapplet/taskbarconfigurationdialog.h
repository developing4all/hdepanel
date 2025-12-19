#ifndef DOCKCONFIGURATIONDIALOG_H
#define DOCKCONFIGURATIONDIALOG_H

#include <QDialog>

namespace Ui {
class TaskBarConfigurationDialog;
}

class TaskBarConfigurationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TaskBarConfigurationDialog(QString id,QWidget *parent = 0);
    ~TaskBarConfigurationDialog();

private slots:
    void on_buttonBox_accepted();
    void on_buttonColorButton_clicked();
    void on_focusColorButton_clicked();
    void buttonColorTransparency_changed(int value);
    void focusColorTransparency_changed(int value);

private:
    Ui::TaskBarConfigurationDialog *ui;
    QString m_id;
};

#endif // DOCKCONFIGURATIONDIALOG_H
