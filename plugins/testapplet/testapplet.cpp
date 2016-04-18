#include "testapplet.h"

#include "../../lib/textgraphicsitem.h"
#include "../../lib/panelwindow.h"

#include <QDebug>

TestApplet::TestApplet(PanelWindow* panelWindow)
    : Applet(panelWindow)
{
    m_textItem = new TextGraphicsItem(this);
    m_textItem->setColor(Qt::white);
    m_textItem->setFont(m_panelWindow->font());
    m_textItem->setText("Applications");
#if QT_VERSION >= 0x050000
    // TODO: add oslogo to act like M$_WIN
    m_textItem->setImage(QImage("/usr/share/icons/hicolor/22x22/apps/webkit"));
#endif
}


QSize TestApplet::desiredSize()
{
#if QT_VERSION >= 0x050000
    return QSize(m_textItem->boundingRect().size().width() + 16,
                 m_textItem->boundingRect().size().height());
#else
    return QSize(m_textItem->boundingRect().size().width(),
                 m_textItem->boundingRect().size().height());
#endif
}

Applet* TestAppletPlugin::createApplet(PanelWindow* panelWindow)
{
    return new TestApplet(panelWindow);
}

Q_PLUGIN_METADATA(IID "hde.panel.testapplet")
