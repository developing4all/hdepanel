#ifndef APPLETSLISTDIALOG_H
#define APPLETSLISTDIALOG_H

#include <QDialog>

namespace Ui {
class AppletsListDialog;
}

class AppletsListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AppletsListDialog(QWidget *parent = 0);
    ~AppletsListDialog();
    QString currentApplet();

private:
    Ui::AppletsListDialog *ui;
};

#endif // APPLETSLISTDIALOG_H
