#include "startapplet.h"

#include "../../lib/textgraphicsitem.h"
#include "../../lib/panelwindow.h"
#include "startwindow.h"

#include <QIcon>
#include <QApplication>
#include <QDesktopWidget>
#include <QDebug>

StartApplet::StartApplet(PanelWindow* panelWindow)
    : Applet(panelWindow)
{
    setObjectName("Start");
    if(panelWindow != 0)
    {
        setPanelWindow(panelWindow);
    }
    m_start = new StartWindow;
}

StartApplet::~StartApplet()
{
    if(m_start != NULL)
    {
        delete m_start;
    }
}

void StartApplet::setPanelWindow(PanelWindow *panelWindow)
{
    Applet::setPanelWindow(panelWindow);

    m_textItem = new TextGraphicsItem(this);
    m_textItem->setColor(Qt::white);
    m_textItem->setFont(m_panelWindow->font());
    m_textItem->setText("Start");
#if QT_VERSION >= 0x050000
    m_textItem->setImage(QImage(QIcon::fromTheme("start-here").pixmap(22,22).toImage()));
#endif
}

QSize StartApplet::desiredSize()
{
#if QT_VERSION >= 0x050000
    return QSize(m_textItem->boundingRect().size().width() + 16,
                 m_textItem->boundingRect().size().height());
#else
    return QSize(m_textItem->boundingRect().size().width(),
                 m_textItem->boundingRect().size().height());
#endif
}

bool StartApplet::init()
{
    setInteractive(true);
    return m_start->init();
}


void StartApplet::clicked()
{
    int x = localToScreen(QPoint(0, m_size.height())).x();
    int y = localToScreen(QPoint(0, m_size.height())).y();

    if(y >= QApplication::desktop()->screenGeometry(m_panelWindow->screen()).height() )
    {
        y = y - m_start->height() - m_size.height();
    }
/*
    qDebug() << "Current desktop width: " << QApplication::desktop()->screenGeometry(m_panelWindow->screen()).width();
    qDebug() << "Start width: " << m_start->width();
    qDebug() << "m_size.width(): " << m_size.width();
    qDebug() << "X: " << x;
*/
    if((x - m_start->width() + m_size.width()) >= QApplication::desktop()->screenGeometry(m_panelWindow->screen()).width())
    {
        x = localToScreen(QPoint(0, m_size.height())).x() - m_start->width() + m_size.width();
    }
    m_start->move(x,y);

    m_start->show();
    m_start->setFocused();
}

void StartApplet::layoutChanged()
{
#if QT_VERSION >= 0x050000
    m_textItem->setPos(8,
        (m_panelWindow->height() - m_textItem->boundingRect().height()) / 2);
#else
    m_textItem->setPos(8, m_panelWindow->textBaseLine());
#endif
}


Applet* StartAppletPlugin::createApplet(PanelWindow* panelWindow)
{
    return new StartApplet(panelWindow);
}

Q_PLUGIN_METADATA(IID "hde.panel.startapplet")
