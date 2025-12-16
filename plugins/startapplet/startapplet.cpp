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
    
    // Create icon item for start icon (size will be adjusted in refreshIcons)
    m_iconItem = new TextGraphicsItem(this);
    m_iconItem->setColor(Qt::white);
    m_iconSize = 22;

    // Refresh icon when theme changes
    QObject::connect(qApp, SIGNAL(iconThemeChanged(const QString&)), this, SLOT(refreshIcons()));

    // Initialise icon with the right size for the current orientation
    refreshIcons();
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
    if (!m_iconItem || !m_panelWindow)
        return;

    PanelWindow::Orientation orientation = m_panelWindow->orientation();
    PanelWindow::Position position = m_panelWindow->position();
    const bool horizontal =
        (orientation == PanelWindow::Horizontal) &&
        !(position == PanelWindow::Left || position == PanelWindow::Right);

    const int sideMargin = 3;
    const int topMargin  = 5;
    const int padding    = 4;

    int thickness = horizontal ? m_panelWindow->panelHeight()
                               : m_panelWindow->panelWidth();
    if (thickness <= 0) thickness = 24;

    int icon = 22;

    if (!horizontal) {
        // Side panel: show text only on wide panels
        const bool showText = (thickness >= 100);

        if (!showText) {
            // ✅ Narrow side panel: icon is square (width - 6)
            icon = thickness - 2 * sideMargin; // 64 -> 58
        } else {
            // Wide side panel: keep a sane cap
            icon = qMin(thickness - 2 * padding, 32);
        }

        // Clamp by the applet's actual cell height (NOT panelHeight)
        if (m_size.height() > 0) {
            const int innerH = qMax(1, m_size.height() - topMargin);
            icon = qMin(icon, innerH);
        }
    } else {
        // Horizontal panels
        icon = qMin(thickness - 2 * padding, 32);
        icon = qMax(icon, 8);
    }

    if (icon < 8) icon = 8;
    m_iconSize = icon;

    QImage img = QIcon::fromTheme("start-here").pixmap(m_iconSize, m_iconSize).toImage();
    m_iconItem->setImage(img);
#endif
}

QSize StartApplet::desiredSize()
{
    if (!m_panelWindow) return QSize(100, 48);

    PanelWindow::Orientation orientation = m_panelWindow->orientation();
    PanelWindow::Position position = m_panelWindow->position();
    const bool horizontal =
        (orientation == PanelWindow::Horizontal) &&
        !(position == PanelWindow::Left || position == PanelWindow::Right);

    QFontMetrics metrics(m_panelWindow->font());
    const QString label = QStringLiteral("Start");

    const int topMargin   = 5;
    const int leftPadding = 12;
    const int gap         = 12;
    const int rightPadding= 16;

    const int icon = (m_iconSize > 0 ? m_iconSize : 22);

    if (horizontal) {
        int w = leftPadding + icon + gap + metrics.horizontalAdvance(label) + rightPadding;
        return QSize(w, m_panelWindow->panelHeight());
    } else {
        // Side panels: row-style button (not a tall tile)
        int thickness = m_panelWindow->panelWidth();
        if (thickness < 10) thickness = 10;

        int h = m_panelWindow->panelHeight();
        if (h < 24) h = 24;

        // Ensure icon never exceeds the button height (with top margin)
        h = qMax(h, topMargin + icon + 6);

        return QSize(thickness, h);
    }
}

bool StartApplet::init()
{
    setInteractive(true);
    
    // Initialize StartWindow - it will set up signal connections and start loading applications in background
    bool result = m_start->init();
    
    return result;
}


void StartApplet::clicked()
{
    if (!m_panelWindow || !m_start)
        return;

    const QList<QScreen*> screens = QGuiApplication::screens();
    const int sidx = m_panelWindow->screen();
    const QScreen* screen = (sidx >= 0 && sidx < screens.size())
        ? screens[sidx]
        : QGuiApplication::primaryScreen();
    const QRect screenGeometry = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);

    const QRect panelGeom = m_panelWindow->geometry();

    // IMPORTANT: Do NOT call adjustSize() here.
    // Use the current designed size; if not available yet, use sizeHint only for math.
    QSize menuSz = m_start->size();
    if (menuSz.width() <= 0 || menuSz.height() <= 0) {
        menuSz = m_start->sizeHint(); // doesn't resize the widget, only a hint
        if (menuSz.width() <= 0 || menuSz.height() <= 0)
            menuSz = QSize(400, 500); // last-resort fallback for positioning math
    }

    const int menuW = menuSz.width();
    const int menuH = menuSz.height();

    int x = panelGeom.left();
    int y = panelGeom.top();

    const PanelWindow::Position pos = m_panelWindow->position();

    switch (pos) {
        case PanelWindow::Top:
            // Below panel, aligned with start button x in the panel
            x = panelGeom.left() + m_position.x();
            y = panelGeom.bottom() + 1;
            break;

        case PanelWindow::Bottom:
            // Above panel
            x = panelGeom.left() + m_position.x();
            y = panelGeom.top() - menuH;
            break;

        case PanelWindow::Left:
            // Right of panel, aligned to panel top
            x = panelGeom.right() + 1;
            y = panelGeom.top();
            break;

        case PanelWindow::Right:
            // Left of panel, aligned to panel top (your case)
            x = panelGeom.left() - menuW;
            y = panelGeom.top();
            break;
    }

    // Clamp to screen (prevents "above the panel" / offscreen)
    if (x < screenGeometry.left()) x = screenGeometry.left();
    if (x + menuW > screenGeometry.right() + 1) x = screenGeometry.right() - menuW + 1;

    if (y < screenGeometry.top()) y = screenGeometry.top();
    if (y + menuH > screenGeometry.bottom() + 1) y = screenGeometry.bottom() - menuH + 1;

    m_start->move(x, y);
    m_start->show();
    m_start->raise();
    m_start->activateWindow();
    m_start->setFocused();
}

void StartApplet::layoutChanged()
{
    if (!m_panelWindow) return;

    refreshIcons();

    PanelWindow::Orientation orientation = m_panelWindow->orientation();
    PanelWindow::Position position = m_panelWindow->position();
    const bool horizontal =
        (orientation == PanelWindow::Horizontal) &&
        !(position == PanelWindow::Left || position == PanelWindow::Right);

    const int topMargin   = 5;
    const int sideMargin  = 5;
    const int leftPadding = 12;
    const int gap         = 12;

    const int iconSize = (m_iconSize > 0 ? m_iconSize : 22);

    const int cellW = m_size.width();
    const int cellH = m_size.height();
    if (cellW <= 0 || cellH <= 0) return;

    QFontMetrics metrics(m_panelWindow->font());

    if (horizontal) {
        if (m_iconItem) {
            int iconY = (cellH - iconSize) / 2;
            if (iconY < 0) iconY = 0;
            m_iconItem->setPos(leftPadding, iconY);
        }

        if (m_textItem) {
            m_textItem->setText(QStringLiteral("Start"));
            m_textItem->setVisible(true);

            int textX = leftPadding + iconSize + gap;
            int textY = (cellH - metrics.height())/2 + metrics.ascent();
            m_textItem->setPos(textX, textY);
        }
    } else {
        const bool showText = (cellW >= 100);

        const int innerY = topMargin;
        const int innerH = qMax(1, cellH - topMargin);

        const int iconX = showText ? leftPadding : sideMargin;

        if (m_iconItem) {
            int iconY = innerY + (innerH - iconSize) / 2;
            if (iconY < innerY) iconY = innerY;
            m_iconItem->setPos(iconX, iconY);
        }

        if (m_textItem) {
            if (showText) {
                m_textItem->setText(QStringLiteral("Start"));
                m_textItem->setVisible(true);

                int textX = leftPadding + iconSize + gap;
                int textY = innerY + (innerH - metrics.height())/2 + metrics.ascent();
                m_textItem->setPos(textX, textY);
            } else {
                m_textItem->setText(QString());
                m_textItem->setVisible(false);
            }
        }
    }
}


Applet* StartAppletPlugin::createApplet(PanelWindow* panelWindow)
{
    return new StartApplet(panelWindow);
}

Q_PLUGIN_METADATA(IID "hde.panel.startapplet")
