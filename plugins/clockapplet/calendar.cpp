#include "calendar.h"
#include "ui_calendar.h"

#include <QDebug>
#include <QFocusEvent>

Calendar::Calendar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Calendar)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);
}

Calendar::~Calendar()
{
    delete ui;
}

void Calendar::setFocused()
{
    //qDebug() << "setting focus";
    setFocus(Qt::PopupFocusReason);
}


void Calendar::focusOutEvent(QFocusEvent *event)
{
    Q_UNUSED(event)
    //if(m_contextMenu->isVisible())
    //    return;
    //ui->calendarWidget->clearFocus();
    //clearFocus();

    // If the click inside the calendar
    if (geometry().contains(QCursor::pos())) {
        setFocus(Qt::PopupFocusReason);
    }else
    {
        //ui->calendarWidget->clearFocus();
        //clearFocus();
        //qDebug() << "Hiding";
        hide();
    }
}
