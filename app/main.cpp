#include "panelapplication.h"
#include <QDebug>

#include <signal.h>


static void m_cleanup(int sig)
{
    qDebug() << "Bye :)";
    // your destructor stuff
    //if (robot) delete robot; robot = NULL;
    if (sig == SIGINT) qApp->quit();
}


int main(int argc, char** argv)
{
	PanelApplication app(argc, argv);
	app.init();

    // Clean shutdown even when we press ctrl-c
    signal(SIGINT, m_cleanup);
    //QObject::connect(&app, SIGNAL(aboutToQuit()), &app, SLOT(deletePanels()));

    int ret = app.exec();

    return ret;
}
