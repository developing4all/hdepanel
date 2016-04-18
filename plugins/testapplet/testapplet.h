#ifndef TESTAPPLET_H
#define TESTAPPLET_H

#include "../../lib/applet.h"
#include <QtCore>
#include <QtDebug>

class TextGraphicsItem;
class TestApplet;
class PanelWindow;


class TestAppletPlugin: public QObject, public AppletPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "hde.panel.appletplugin" FILE "TestApplet.json")
    Q_INTERFACES(AppletPlugin)

public:
    TestAppletPlugin(){}
    ~TestAppletPlugin(){}

    Applet* createApplet(PanelWindow* panelWindow) Q_DECL_OVERRIDE;
};


class TestApplet: public Applet
{
    Q_OBJECT
public:
    TestApplet(PanelWindow* panelWindow = 0);
    ~TestApplet(){}
    void close(){}
    virtual void setPanelWindow(PanelWindow* panelWindow);

    bool init(){return true;}
    //void startPlugin();

    QSize desiredSize();

public slots:
    void clicked(){}
    void fontChanged(){}

protected:
    void layoutChanged(){}
    bool isHighlighted(){ return false;}

private:
    TextGraphicsItem* m_textItem;

};

#endif // TESTAPPLET_H
