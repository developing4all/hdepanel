#include "startapplet.h"

#include "../../lib/textgraphicsitem.h"
#include "../../lib/panelwindow.h"
#include "startwindow.h"

#include <QIcon>
#include <QApplication>
#include <QScreen>
#include <QDebug>

StartApplet::StartApplet(PanelWindow* panelWindow)
    : Applet(panelWindow)
{
    setObjectName("Start");
    m_start = new StartWindow;
    
    // Create text item for "Start" text
    m_textItem = new TextGraphicsItem(this);
    m_textItem->setColor(Qt::white);
    m_textItem->setText("Start");
    
    // Create icon item for start icon
    m_iconItem = new TextGraphicsItem(this);
    m_iconItem->setColor(Qt::white);
#if QT_VERSION >= 0x050000
    m_iconItem->setImage(QImage(QIcon::fromTheme("start-here").pixmap(22,22).toImage()));
#endif

    // Refresh icon when theme changes
    QObject::connect(qApp, SIGNAL(iconThemeChanged(const QString&)), this, SLOT(refreshIcons()));
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

    // Update existing text item
    if (m_textItem) {
        m_textItem->setFont(m_panelWindow->font());
    }
    
    // Icon item doesn't need updating as it's just an image
}

void StartApplet::refreshIcons()
{
#if QT_VERSION >= 0x050000
    if (m_iconItem)
        m_iconItem->setImage(QImage(QIcon::fromTheme("start-here").pixmap(22,22).toImage()));
#endif
}

QSize StartApplet::desiredSize()
{
    if (!m_textItem || !m_panelWindow) return QSize(100, 48);
    
    // Calculate width: left padding + icon + margin + text + right padding
    QFontMetrics metrics(m_panelWindow->font());
    int textWidth = metrics.horizontalAdvance("Start");
    int leftPadding = 12;   // Left padding
    int iconWidth = 22;     // Icon width
    int margin = 12;        // Margin between icon and text
    int rightPadding = 16;  // Right padding
    
    int totalWidth = leftPadding + iconWidth + margin + textWidth + rightPadding;
    return QSize(totalWidth, m_panelWindow->height());
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

    {
        const QList<QScreen*> screens = QGuiApplication::screens();
        const int sidx = m_panelWindow->screen();
        const QScreen* screen = (sidx >= 0 && sidx < screens.size()) ? screens[sidx] : QGuiApplication::primaryScreen();
        const QRect screenGeometry = screen ? screen->geometry() : QRect(0,0,1920,1080);

    if(y >= screenGeometry.height() )
    {
        y = y - m_start->height() - m_size.height();
    }
/*
    qDebug() << "Current desktop width: " << QApplication::desktop()->screenGeometry(m_panelWindow->screen()).width();
    qDebug() << "Start width: " << m_start->width();
    qDebug() << "m_size.width(): " << m_size.width();
    qDebug() << "X: " << x;
*/
    if((x - m_start->width() + m_size.width()) >= screenGeometry.width())
    {
        x = localToScreen(QPoint(0, m_size.height())).x() - m_start->width() + m_size.width();
    }
    m_start->move(x,y);

    m_start->show();
    m_start->setFocused();
    }
}

void StartApplet::layoutChanged()
{
    if (!m_panelWindow) return;
    
    // Define spacing constants (same as in desiredSize)
    int leftPadding = 12;   // Left padding
    int iconWidth = 22;     // Icon width
    int margin = 12;        // Margin between icon and text
    
    // Position icon at the left edge with padding, vertically centered
    if (m_iconItem) {
        m_iconItem->setPos(leftPadding, (m_panelWindow->height() - iconWidth) / 2);
    }
    
    // Position text after the icon with proper margin
    if (m_textItem) {
        int textX = leftPadding + iconWidth + margin;
        int textY = m_panelWindow->textBaseLine();
        m_textItem->setPos(textX, textY);
    }
}


Applet* StartAppletPlugin::createApplet(PanelWindow* panelWindow)
{
    return new StartApplet(panelWindow);
}

Q_PLUGIN_METADATA(IID "hde.panel.startapplet")
