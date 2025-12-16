#ifndef STARTAPPLET_H
#define STARTAPPLET_H


#include "../../lib/applet.h"
#include <QtCore>
#include <QtDebug>

class TextGraphicsItem;
class TestApplet;
class PanelWindow;
class StartWindow;

class StartAppletPlugin: public QObject, public AppletPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "hde.panel.appletplugin")
    Q_INTERFACES(AppletPlugin)

public:
    StartAppletPlugin(){}
    ~StartAppletPlugin(){}

    Applet* createApplet(PanelWindow* panelWindow) Q_DECL_OVERRIDE;
};


class StartApplet: public Applet
{
    Q_OBJECT
public:
    StartApplet(PanelWindow* panelWindow = 0);
    ~StartApplet();
    bool init();
    void close(){}
    QMargins buttonMargins() const override { return QMargins(0, 12, 0, 0); }
    virtual void setPanelWindow(PanelWindow* panelWindow);

    //void startPlugin();

    QSize desiredSize();
public slots:
    void clicked();
    void fontChanged(){}
    void refreshIcons();

protected:
    void layoutChanged();
    bool isHighlighted(){ return false;}

private:
    TextGraphicsItem* m_textItem;
    TextGraphicsItem* m_iconItem;
    StartWindow *m_start;
    int m_iconSize = 22;

};

#endif // STARTAPPLET_H
