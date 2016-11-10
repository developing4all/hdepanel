#include "appletslistdialog.h"
#include "ui_appletslistdialog.h"

#include <QDir>
#include <QApplication>
#include <QDebug>
#include <QDateTime>

AppletsListDialog::AppletsListDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AppletsListDialog)
{
    ui->setupUi(this);

    QDir plugDir = QDir(qApp->applicationDirPath() + "/plugins");

    // If does not exists check the standard plugin directory
    if((!plugDir.exists()) && QDir("/usr/lib/hde/panel/plugins").exists())
    {
        plugDir.cd("/usr/lib/hde/panel/plugins");
    }

    QStringList plugins = plugDir.entryList(QStringList("lib*applet.so"),QDir::Files | QDir::NoSymLinks);

    if(!plugins.isEmpty())
    {
        foreach (QString plugin, plugins) {
            // Remove lib from the begin and .so from the end
            plugin.replace(0, 3, "").replace(".so","").replace("applet", "Applet");
            // Captilize the first letter
            plugin[0] = plugin.at(0).toTitleCase();

            QString applet_id = plugin  + "_" + QString::number(QDateTime::currentMSecsSinceEpoch());
            QListWidgetItem *item = new QListWidgetItem(plugin);
            item->setData(Qt::UserRole, applet_id);
            ui->aplletsList->addItem(item);
        }
    }
}

AppletsListDialog::~AppletsListDialog()
{
    delete ui;
}

QString AppletsListDialog::currentApplet()
{
    QString applet;

    if(ui->aplletsList->currentItem() != NULL)
    {
        applet = ui->aplletsList->currentItem()->data(Qt::UserRole).toString();
    }

    return applet;
}
