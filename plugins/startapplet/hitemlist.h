#ifndef HITEMLIST_H
#define HITEMLIST_H

#include <QListWidget>
#include <QPoint>
class HItemList : public QListWidget
{
    Q_OBJECT
public:
    explicit HItemList(QWidget *parent = 0);

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    //QStringList mimeTypes() const;
    //QMimeData * mimeData( const QList<QListWidgetItem *> items ) const;
signals:

public slots:

private:
    QPixmap getItemLable(QListWidgetItem *item) const;
    QPoint dragStartPosition;
};

#endif // HITEMLIST_H
