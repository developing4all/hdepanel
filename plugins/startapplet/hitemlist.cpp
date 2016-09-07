#include "hitemlist.h"

#include <QMouseEvent>
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QDebug>
#include <QPainter>

HItemList::HItemList(QWidget *parent) :
    QListWidget(parent)
{
    setDragEnabled(true);
}

void HItemList::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        dragStartPosition = event->pos();
    return QListWidget::mousePressEvent(event);
}

void HItemList::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return;
    if ((event->pos() - dragStartPosition).manhattanLength()
         < QApplication::startDragDistance())
        return;

    QListWidgetItem *item = currentItem();
    if(item == NULL)
        return;

    QString filename = "file://"+item->data(Qt::UserRole).toString();

    QMimeData *mimeData = new QMimeData;

    mimeData->setData("text/plain", filename.toUtf8());
    mimeData->setData("text/uri-list", filename.toUtf8());
    mimeData->setData("text/x-moz-url", filename.toUtf8());

    mimeData->setData("application/x-desktop", filename.toUtf8());
    mimeData->setData("application/x-qabstractitemmodeldatalist", filename.toUtf8());

    QList<QUrl> urls;
    urls.append(filename);
    mimeData->setUrls(urls);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(getItemLable(item));
    drag->setHotSpot(QPoint(drag->pixmap().width()/2, drag->pixmap().height()));

    Qt::DropAction dropAction = drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
    Q_UNUSED(dropAction)

    QListWidget::mouseMoveEvent(event);
}

QPixmap HItemList::getItemLable(QListWidgetItem *item) const
{
    QPixmap pixmap(200, 28);// = item->icon().pixmap(24,24);
    QPainter painter(&pixmap);
    painter.eraseRect(QRect(0,0,200,30));
    painter.drawPixmap(2,2,24,24,item->icon().pixmap(24,24));
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 14));
    painter.drawText(QRect(28,2,197,26), Qt::AlignLeft, item->text());
    //painter.setPen(Qt::black);
    //painter.drawRect(QRect(0,0,199,27));
   return pixmap;
}
