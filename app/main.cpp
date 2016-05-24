#include "panelapplication.h"
#include <QDebug>

#include <signal.h>


static void m_cleanup(int sig)
{
    qDebug() << "Bye :)";
    // your destructor stuff
    if (sig == SIGINT) qApp->quit();
}


int main(int argc, char** argv)
{
	PanelApplication app(argc, argv);
	app.init();

    // Clean shutdown even when we press ctrl-c
    signal(SIGINT, m_cleanup);

    int ret = app.exec();

    return ret;
}
