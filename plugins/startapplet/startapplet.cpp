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
/*
    connect(DesktopApplications::instance(), SIGNAL(applicationUpdated(DesktopApplication)), this, SLOT(applicationUpdated(DesktopApplication)));
    connect(DesktopApplications::instance(), SIGNAL(applicationRemoved(QString)), this, SLOT(applicationRemoved(QString)));

    QList<DesktopApplication> apps = DesktopApplications::instance()->applications();
    foreach(const DesktopApplication& app, apps)
    {
        applicationUpdated(app);
    }

    m_menu->addSeparator();
    m_menu->addAction(QIcon::fromTheme("application-exit"), "Quit", qApp, SLOT(quit()));
*/
    return m_start->init();
}


void StartApplet::clicked()
{
    int x = localToScreen(QPoint(0, m_size.height())).x();
    int y = localToScreen(QPoint(0, m_size.height())).y();

    /*
    qDebug() << "m_panelWindow->pos(): " << m_panelWindow->pos();
    qDebug() << "m_position: " << m_position;
    qDebug() << "m_size: " << m_size;
//    m_start->move(localToScreen(QPoint(0, m_size.height() - m_start->height())));

    qDebug() << "x: " << x;
    qDebug() << "y: " << y;
    */

    if(y >= QApplication::desktop()->screenGeometry(m_panelWindow->screen()).height() )
    {
        y = y - m_start->height() - m_size.height();
    }
    m_start->move(x,y);

    m_start->show();
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
