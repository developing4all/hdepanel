/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * Copyright: 2015-2025 Haydar Alkaduhimi
 * Authors:
 *   Haydar Alkaduhimi <haydar@developing4all.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "panelwindow.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QProcessEnvironment>
#include <QProcess>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QShowEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QDir>
#include <QPluginLoader>
#include <QLinearGradient>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QAbstractNativeEventFilter>
#include "layershellqtintegration.h"
#include <typeinfo>

#if QT_VERSION < 0x060000
#include <QDesktopWidget>
#endif
 
#include <QStandardPaths>
 
#include "settings.h"
#include "applet.h"
#include "panelapplication.h"
#include "x11support.h"
#include "panelsettings.h"
#include "hpopupmenu.h"
 
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  include <QGuiApplication>
#else
#  include <QX11Info>
#endif

#if defined(Q_OS_UNIX)
#  include <xcb/xcb.h>
#endif

// IMPORTANT: X11 headers MUST come AFTER Qt headers on Qt5 (X11 defines macro "None")
#if defined(Q_OS_UNIX)
#  include <X11/Xlib.h>
#  include <X11/Xatom.h>
#endif

#include "customtooltip.h"
#include "tooltipeventfilter.h"

class PanelWindow::RootEventFilter final : public QAbstractNativeEventFilter {
public:
    explicit RootEventFilter(PanelWindow* panel)
        : m_panel(panel) {}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray& eventType, void* message, long*) override
#else
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr*) override
#endif
    {
        if (!m_panel) return false;
        if (eventType != "xcb_generic_event_t") return false;
        if (!message) return false;

        auto* ev = static_cast<xcb_generic_event_t*>(message);
        const uint8_t type = ev->response_type & ~0x80;

        const xcb_window_t root = static_cast<xcb_window_t>(X11Support::rootWindow());
        if (!root) return false;

        if (type == XCB_PROPERTY_NOTIFY) {
            auto* pe = reinterpret_cast<xcb_property_notify_event_t*>(ev);
            if (pe->window != root) return false;

            const xcb_atom_t aWorkarea = static_cast<xcb_atom_t>(X11Support::atom("_NET_WORKAREA"));
            const xcb_atom_t aDeskGeom = static_cast<xcb_atom_t>(X11Support::atom("_NET_DESKTOP_GEOMETRY"));
            const xcb_atom_t aCurDesk  = static_cast<xcb_atom_t>(X11Support::atom("_NET_CURRENT_DESKTOP"));
            const xcb_atom_t aViewport = static_cast<xcb_atom_t>(X11Support::atom("_NET_DESKTOP_VIEWPORT"));
            const xcb_atom_t aClient   = static_cast<xcb_atom_t>(X11Support::atom("_NET_CLIENT_LIST"));

            // Ignore workarea updates that are very likely caused by our own strut changes.
            if (pe->atom == aWorkarea) {
                if (m_panel->m_lastStrutApply.isValid() && m_panel->m_lastStrutApply.elapsed() < 250)
                    return false;
                m_panel->scheduleRepositionFromWmChange();
                return false;
            }

            if (pe->atom == aDeskGeom || pe->atom == aCurDesk || pe->atom == aViewport || pe->atom == aClient) {
                m_panel->scheduleRepositionFromWmChange();
                return false;
            }
        } else if (type == XCB_CONFIGURE_NOTIFY) {
            auto* ce = reinterpret_cast<xcb_configure_notify_event_t*>(ev);
            if (ce->window != root) return false;
            m_panel->scheduleRepositionFromWmChange();
            return false;
        }

        return false;
    }

private:
    PanelWindow* m_panel = nullptr; // not owned
};
// ---------------------- PanelWindowGraphicsItem ----------------------
 
PanelWindow::PanelWindowGraphicsItem::PanelWindowGraphicsItem(PanelWindow* panelWindow)
    : m_panelWindow(panelWindow)
{
    setZValue(-10.0); // background
    setAcceptedMouseButtons(Qt::RightButton);
}
 
PanelWindow::PanelWindowGraphicsItem::~PanelWindowGraphicsItem() = default;
 
QRectF PanelWindow::PanelWindowGraphicsItem::boundingRect() const
{
    return QRectF(0.0, 0.0, m_panelWindow->width(), m_panelWindow->height());
}
 
void PanelWindow::PanelWindowGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
	painter->setPen(Qt::NoPen);
	QColor bgColor = m_panelWindow->m_backgroundColor;
	bgColor.setAlpha(m_panelWindow->m_backgroundTransparency);
	painter->setBrush(bgColor);
	painter->drawRect(boundingRect());

	static const int borderThickness = 3;
	QColor borderColor = m_panelWindow->m_borderColor;
	if(m_panelWindow->position() == PanelWindow::Top)
	{
		QLinearGradient gradient(0.0, m_panelWindow->height() - borderThickness, 0.0, m_panelWindow->height());
		gradient.setSpread(QGradient::RepeatSpread);
		QColor borderColorStart = borderColor;
		borderColorStart.setAlpha(0);
		QColor borderColorEnd = borderColor;
		borderColorEnd.setAlpha(m_panelWindow->m_borderTransparency);
		gradient.setColorAt(0.0, borderColorStart);
		gradient.setColorAt(1.0, borderColorEnd);
		painter->setBrush(QBrush(gradient));
		painter->drawRect(0.0, m_panelWindow->height() - borderThickness, m_panelWindow->width(), borderThickness);
	}
	else
	{
		QLinearGradient gradient(0.0, 0.0, 0.0, borderThickness);
		gradient.setSpread(QGradient::RepeatSpread);
		QColor borderColorStart = borderColor;
		borderColorStart.setAlpha(m_panelWindow->m_borderTransparency);
		QColor borderColorEnd = borderColor;
		borderColorEnd.setAlpha(0);
		gradient.setColorAt(0.0, borderColorStart);
		gradient.setColorAt(1.0, borderColorEnd);
		painter->setBrush(QBrush(gradient));
		painter->drawRect(0.0, 0.0, m_panelWindow->width(), borderThickness);
	}
 }
 
void PanelWindow::PanelWindowGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent* e) { Q_UNUSED(e) }
void PanelWindow::PanelWindowGraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e)
{
    if (isUnderMouse())
        m_panelWindow->showPanelContextMenu(QPoint(static_cast<int>(e->pos().x()),
                                                   static_cast<int>(e->pos().y())));
}
 
// ---------------------- PanelWindow ----------------------
 
PanelWindow::PanelWindow(QString id)
    : m_id(std::move(id))
{
    // Defaults
    m_dockMode        = true;        // docks want to reserve space by default
    m_screen          = 0;
    m_position        = Bottom;      // bottom by default
    m_orientation     = Horizontal;
    m_layoutPolicy    = Normal;
 
     // Initial size: use screen width, standard height
#if QT_VERSION < 0x060000
    const QRect screen = QApplication::desktop()->screenGeometry();
#else
    const QList<QScreen*> screens = QGuiApplication::screens();
    const QRect screen = screens.isEmpty() ? QRect(0,0,1920,1080) : screens.first()->geometry();
#endif
    resize(screen.width(), 38);
 
    // Scene / view
    m_scene = new QGraphicsScene(this);
    m_scene->setBackgroundBrush(Qt::NoBrush);
 
    auto* item = new PanelWindowGraphicsItem(this);
    m_scene->addItem(item);
 
    m_view = new QGraphicsView(m_scene, this);
    m_view->setStyleSheet("border-style: none; background: rgba(0, 0, 0, 0.4)");
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->move(0, 0);
    m_view->setMouseTracking(true);
    if (m_view->viewport()) {
        m_view->viewport()->setMouseTracking(true);
    }
    m_view->setAttribute(Qt::WA_NoMousePropagation);
    m_view->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_view->setBackgroundBrush(Qt::NoBrush);
    
    // Create custom tooltip widget (no parent - it's a top-level window)
    m_customTooltip = new CustomTooltip();
    
    // Disable default tooltips on the view
    m_view->setAttribute(Qt::WA_AlwaysShowToolTips, false);
    m_view->setToolTip(QString());
    if (m_view->viewport()) {
        m_view->viewport()->setAttribute(Qt::WA_AlwaysShowToolTips, false);
        m_view->viewport()->setToolTip(QString());
    }
    
    // Install event filter on the view to show custom tooltip widget
    auto* tooltipFilter = new TooltipEventFilter(m_view, this, this);
    // QGraphicsView sends mouse/tooltip events to its viewport, so filter there.
    if (m_view->viewport()) {
        m_view->viewport()->installEventFilter(tooltipFilter);
    } else {
        m_view->installEventFilter(tooltipFilter);
    }
     
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
     
    // Window flags / attributes - must be set before showing to prevent decorations
    // Set frameless and always on top for proper panel behavior
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMinimumSize(10, 10);
    
    // Set a specific window title for identification
    setWindowTitle("HDE Panel - Desktop Panel");
    
    // Simplified window setup for Wayland testing
    
    // Initialize Wayland layer-shell if on Wayland
    if (LayerShellQtIntegration::isWayland()) {
        // Try LayerShellQt first (preferred method)
        m_layerShellQt = new LayerShellQtIntegration(this);
        if (m_layerShellQt->isAvailable()) {
            qDebug() << "PanelWindow: LayerShellQt is available - using for positioning";
            m_layerShellQt->enableLayerShell();
        } else {
            qDebug() << "PanelWindow: LayerShellQt not available, falling back to custom implementation";
            m_waylandLayerShell = new WaylandLayerShell(this);
            if (m_waylandLayerShell->initialize()) {
                qDebug() << "PanelWindow: Wayland layer-shell initialized successfully";
            } else {
                qDebug() << "PanelWindow: Failed to initialize Wayland layer-shell";
                // Clean up failed initialization
                delete m_waylandLayerShell;
                m_waylandLayerShell = nullptr;
            }
        }
    }
 
    // Settings & plugins
    readSettings();
    
    // Apply dock mode attributes early (before showing) to prevent window decorations
    // This ensures the window is treated as a dock/panel from the start
#if QT_VERSION < 0x060000
    if (QX11Info::isPlatformX11())
        setAttribute(Qt::WA_X11NetWmWindowTypeDock, m_dockMode);
#else
    if (qApp->platformName().toLower().contains("xcb"))
        setAttribute(Qt::WA_X11NetWmWindowTypeDock, m_dockMode);
#endif
    
    setApplets();
    init();
 
    // Debounce strut applying
    m_strutDebounce.setSingleShot(true);
    m_strutDebounce.setInterval(120);
    connect(&m_strutDebounce, &QTimer::timeout, this, [this]{
        applyX11Struts(geometry());
    });

    // Debounce repositioning after WM/root workarea/geometry changes
    m_repositionDebounce.setSingleShot(true);
    m_repositionDebounce.setInterval(80);
    connect(&m_repositionDebounce, &QTimer::timeout, this, [this]{
        // Re-run the same canonical path: layout -> position -> struts (debounced)
        updateLayout();
        updatePosition();
        scheduleApplyStruts();
    });
    
    // Wayland positioning timer - continuously force position
#if QT_VERSION < 0x060000
    if (!QX11Info::isPlatformX11()) {
        m_waylandPositionTimer.setInterval(100); // Force position every 100ms
        connect(&m_waylandPositionTimer, &QTimer::timeout, this, [this]() {
            const QList<QScreen*> screens = QGuiApplication::screens();
            if (!screens.isEmpty()) {
                const QRect screenGeometry = screens.first()->geometry();
                const int panelY = screenGeometry.height() - height(); // Bottom of screen
                const int panelWidth = screenGeometry.width();
                const int panelHeight = height();
                
                // Force position continuously
                setGeometry(0, panelY, panelWidth, panelHeight);
                move(0, panelY);
                
                if (geometry().y() != panelY) {
                    qDebug() << "PanelWindow: Forcing position - current y:" << geometry().y() << "target y:" << panelY;
                }
            }
        });
    }
#else
    if (!qApp->platformName().toLower().contains("xcb")) {
        m_waylandPositionTimer.setInterval(100); // Force position every 100ms
        connect(&m_waylandPositionTimer, &QTimer::timeout, this, [this]() {
            const QList<QScreen*> screens = QGuiApplication::screens();
            if (!screens.isEmpty()) {
                const QRect screenGeometry = screens.first()->geometry();
                const int panelY = screenGeometry.height() - height(); // Bottom of screen
                const int panelWidth = screenGeometry.width();
                const int panelHeight = height();
                
                // Force position continuously
                setGeometry(0, panelY, panelWidth, panelHeight);
                move(0, panelY);
                
                // Skip wmctrl since it's not working with our panel window
                // Rely on Qt positioning methods only
                
                if (geometry().y() != panelY) {
                    qDebug() << "PanelWindow: Forcing position - current y:" << geometry().y() << "target y:" << panelY;
                }
            }
        });
    }
#endif
 
#if QT_VERSION >= 0x050000
    // Wayland fallback: gently reassert position
#if QT_VERSION < 0x060000
    const bool isX11 = QX11Info::isPlatformX11();
#else
    const bool isX11 = qApp->platformName().toLower().contains("xcb");
#endif
    if (!isX11) {
        // Use timer-based positioning for Wayland
        m_waylandRepositionTimer = new QTimer(this);
        m_waylandRepositionTimer->setSingleShot(false);
        m_waylandRepositionTimer->setInterval(50); // more aggressive positioning
        connect(m_waylandRepositionTimer, &QTimer::timeout, this, &PanelWindow::forceWaylandPosition);
        // Timer will be started/stopped based on layer shell availability
    }
#endif
}
 
PanelWindow::~PanelWindow()
{
    removeApplets();
    teardownX11RootEventListener();
    
    // Stop timers before cleanup
    m_strutDebounce.stop();
    m_waylandPositionTimer.stop();
    if (m_waylandRepositionTimer) {
        m_waylandRepositionTimer->stop();
    }
    
    // Clean up Wayland layer shell
    if (m_waylandLayerShell) {
        delete m_waylandLayerShell;
        m_waylandLayerShell = nullptr;
    }
    
    // Clean up graphics items
    if (m_scene) {
        m_scene->clear();
        delete m_scene;
        m_scene = nullptr;
    }
    
    if (m_view) {
        delete m_view;
        m_view = nullptr;
    }
}
 
void PanelWindow::setupX11RootEventListener()
{
#if QT_VERSION < 0x060000
    if (!QX11Info::isPlatformX11()) return;
    Display* dpy = QX11Info::display();
#else
    if (!qApp->platformName().toLower().contains("xcb")) return;
    auto native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    Display* dpy = native ? native->display() : nullptr;
#endif
    if (!dpy) return;
    if (m_x11RootEventFilter) return; // already installed

    // Ask X11 to send us property/configure events from the root window.
    const Window root = X11Support::rootWindow();
    XSelectInput(dpy, root, PropertyChangeMask | StructureNotifyMask);
    XSync(dpy, False);

    auto* filter = new RootEventFilter(this);
    qApp->installNativeEventFilter(filter);
    m_x11RootEventFilter = filter;
}

void PanelWindow::teardownX11RootEventListener()
{
    if (!m_x11RootEventFilter) return;
    qApp->removeNativeEventFilter(m_x11RootEventFilter);
    delete m_x11RootEventFilter;
    m_x11RootEventFilter = nullptr;
}

void PanelWindow::scheduleRepositionFromWmChange()
{
    // If we’re not visible/mapped yet, just let normal init/showEvent handle it.
    if (!isVisible()) return;
    m_repositionDebounce.start();
}

void PanelWindow::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);

    // Wayland: do NOT hardcode sizes, do NOT call show()/activateWindow() inside showEvent()
    if (LayerShellQtIntegration::isWayland()) {
        // ensure correct orientation (in case settings changed before show)
        if (m_position == Left || m_position == Right)
            m_orientation = Vertical;
        else
            m_orientation = Horizontal;

        updateLayout();                         // computes correct QWidget size
        updateWaylandLayerShellConfiguration(); // applies correct anchors/zone/size

        // If we have layer-shell, stop fallback timers
        if ((m_layerShellQt && m_layerShellQt->isAvailable()) ||
            (m_waylandLayerShell && m_waylandLayerShell->isAvailable())) {
            if (m_waylandRepositionTimer && m_waylandRepositionTimer->isActive())
                m_waylandRepositionTimer->stop();
        } else {
            // no layer-shell, keep your fallback
            updatePosition();
            if (m_waylandRepositionTimer && !m_waylandRepositionTimer->isActive())
                m_waylandRepositionTimer->start();
            scheduleApplyStruts();
        }
        return;
    }

    // On X11: set dock type + above, then position, then schedule struts
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    Display *dpy = QX11Info::display();
#else
    auto native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    Display *dpy = native ? native->display() : nullptr;
#endif
    if (!dpy) return;

    const Window win = winId();

    const Atom wmType     = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    const Atom wmTypeDock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
    XChangeProperty(dpy, win, wmType, XA_ATOM, 32, PropModeReplace,
                    (unsigned char*)&wmTypeDock, 1);

    const Atom wmState      = XInternAtom(dpy, "_NET_WM_STATE", False);
    const Atom wmStateAbove = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
    XChangeProperty(dpy, win, wmState, XA_ATOM, 32, PropModeReplace,
                    (unsigned char*)&wmStateAbove, 1);

    // On all desktops
    const Atom wmDesktop = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    const unsigned long allDesktops = 0xFFFFFFFFul;
    XChangeProperty(dpy, win, wmDesktop, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*)&allDesktops, 1);

    XSync(dpy, False);

    setupX11RootEventListener();

    QTimer::singleShot(0, this, [this]{
        updateLayout();
        updatePosition();
        scheduleApplyStruts();
    });
}
 
void PanelWindow::mousePressEvent(QMouseEvent* e)  { e->accept(); QWidget::mousePressEvent(e); }
void PanelWindow::mouseReleaseEvent(QMouseEvent* e){ e->accept(); QWidget::mouseReleaseEvent(e); }
 
QRect PanelWindow::usableScreenRectX11() const
{
    const QRect screen = currentScreenGeometry();

#if QT_VERSION < 0x060000
    const bool isX11 = QX11Info::isPlatformX11();
#else
    const bool isX11 = qApp->platformName().toLower().contains("xcb");
#endif
    if (!isX11) return screen;

    QMargins ext = X11Support::getExternalStrutReservations(screen, winId());
    int extTop = qMax(ext.top(), detectGnomeTopOffsetPx()); // safe after you fix GNOME detection
    return screen.adjusted(ext.left(), extTop, -ext.right(), -ext.bottom());
}
void PanelWindow::resizeEvent(QResizeEvent* ev)
{
    qDebug() << "PanelWindow::resizeEvent - new size:" << ev->size()
             << "isVisible:" << isVisible() << "isHidden:" << isHidden()
             << "m_updatingLayout:" << m_updatingLayout;

    // Prevent the panel from being resized to an incorrect size
    // Horizontal panels: fixed height
    // Vertical panels: fixed width
    if (!m_updatingLayout) {
        if (m_orientation == Horizontal && ev->size().height() != m_panelHeight) {
            QTimer::singleShot(0, [this]() {
                // Check guard flag again in the timer callback
                if (!m_updatingLayout && m_orientation == Horizontal && height() != m_panelHeight) {
                    m_updatingLayout = true;
                    resize(width(), m_panelHeight);
                    m_updatingLayout = false;
                }
            });
        } else if (m_orientation == Vertical && ev->size().width() != m_panelWidth) {
            QTimer::singleShot(0, [this]() {
                // Check guard flag again in the timer callback
                if (!m_updatingLayout && m_orientation == Vertical && width() != m_panelWidth) {
                    m_updatingLayout = true;
                    resize(m_panelWidth, height());
                    m_updatingLayout = false;
                }
            });
        }
    }

    // Keep the view in sync
    m_view->resize(ev->size());
    m_view->setSceneRect(QRectF(QPointF(0,0), QSizeF(ev->size())));

    // Only update layout if we're not already updating (prevent infinite loops)
    if (!m_updatingLayout) {
        updateLayout();
    }
    
    // Only schedule struts if the panel moved, not on every resize
    scheduleApplyStrutsIfMoved();

    qDebug() << "PanelWindow::resizeEvent - done; geom:" << geometry();
}
 
// ---------------------- Settings & Applets ----------------------
 
void PanelWindow::readSettings()
{
    // Force QSettings to reload the file
    Settings::s_settings->sync();

    setFontName(Settings::value(m_id, "fontName", "default").toString());
    setScreen(Settings::value(m_id, "screen", 0).toInt());

    // Position - support both old format (vertical/horizontal) and new format (position)
    // First try new format
    QVariant posVariant = Settings::value(m_id, "position", QVariant());
#if QT_VERSION >= 0x060000
    bool posIsString = posVariant.isValid() && (posVariant.metaType().id() == QMetaType::QString);
#else
    bool posIsString = posVariant.isValid() && (posVariant.type() == QVariant::String);
#endif

    if (posIsString) {
        // New string format: "Top", "Bottom", "Left", "Right"
        const QString pos = posVariant.toString();
        if (pos == "Top") m_position = Top;
        else if (pos == "Bottom") m_position = Bottom;
        else if (pos == "Left") m_position = Left;
        else if (pos == "Right") m_position = Right;
        else m_position = Bottom; // Default
    } else if (posVariant.isValid()) {
        // New integer format: 0=Top, 1=Bottom, 2=Left, 3=Right
        int posIndex = posVariant.toInt();
        if (posIndex == 0) m_position = Top;
        else if (posIndex == 1) m_position = Bottom;
        else if (posIndex == 2) m_position = Left;
        else if (posIndex == 3) m_position = Right;
        else m_position = Bottom; // Default
    } else {
        // Legacy format: migrate from vertical/horizontal positions
        QVariant vposVariant = Settings::value(m_id, "verticalPosition", 1);
        QVariant hposVariant = Settings::value(m_id, "horizontalPosition", 1);

        bool isTop = false, isBottom = false, isLeft = false, isRight = false;

#if QT_VERSION >= 0x060000
        bool vposIsString = (vposVariant.metaType().id() == QMetaType::QString);
#else
        bool vposIsString = (vposVariant.type() == QVariant::String);
#endif
        if (vposIsString) {
            QString vpos = vposVariant.toString();
            isTop = (vpos == "Top");
            isBottom = (vpos == "Bottom");
        } else {
            int vposIndex = vposVariant.toInt();
            isTop = (vposIndex == 0);
            isBottom = (vposIndex == 1);
        }

#if QT_VERSION >= 0x060000
        bool hposIsString = (hposVariant.metaType().id() == QMetaType::QString);
#else
        bool hposIsString = (hposVariant.type() == QVariant::String);
#endif
        if (hposIsString) {
            QString hpos = hposVariant.toString();
            isLeft = (hpos == "Left");
            isRight = (hpos == "Right");
        } else {
            int hposIndex = hposVariant.toInt();
            isLeft = (hposIndex == 0);
            isRight = (hposIndex == 2);
        }

        if (isTop) m_position = Top;
        else if (isBottom) m_position = Bottom;
        else if (isLeft) m_position = Left;
        else if (isRight) m_position = Right;
        else m_position = Bottom;

        QString positionString;
        if (m_position == Top) positionString = "Top";
        else if (m_position == Bottom) positionString = "Bottom";
        else if (m_position == Left) positionString = "Left";
        else positionString = "Right";
        Settings::setValue(m_id, "position", positionString);
    }

    m_appletnames = Settings::value(m_id, "applets", QStringList()).toStringList();

    // Load layout policy (no longer using FillSpace)
    m_layoutPolicy = Normal;

    // Load color settings
    m_backgroundColor = Settings::value(m_id, "backgroundColor", QColor(0, 0, 0)).value<QColor>();
    m_backgroundTransparency = Settings::value(m_id, "backgroundColorTransparency", 128).toInt();
    m_borderColor = Settings::value(m_id, "borderColor", QColor(255, 255, 255)).value<QColor>();
    m_borderTransparency = Settings::value(m_id, "borderColorTransparency", 128).toInt();

    // Load size settings
    m_panelHeight = Settings::value(m_id, "panelHeight", 48).toInt();
    m_panelWidth  = Settings::value(m_id, "panelWidth", 48).toInt();

    // CRITICAL: keep orientation consistent with position (fixes wrong resize enforcement)
    if (m_position == Left || m_position == Right)
        m_orientation = Vertical;
    else
        m_orientation = Horizontal;
}

void PanelWindow::updateColors()
{
    // Only reload color settings, not size settings
    // Size settings are managed separately to avoid conflicts
    Settings::setGroup(m_id);
    m_backgroundColor = Settings::value("backgroundColor", QColor(0, 0, 0)).value<QColor>();
    m_backgroundTransparency = Settings::value("backgroundColorTransparency", 128).toInt();
    m_borderColor = Settings::value("borderColor", QColor(255, 255, 255)).value<QColor>();
    m_borderTransparency = Settings::value("borderColorTransparency", 128).toInt();
    
    if (m_scene) {
        m_scene->update();
    }
}
 
bool PanelWindow::init()
{
    for (int i = 0; i < m_applets.size();) {
        m_applets[i]->setPanelWindow(this);
        if (!m_applets[i]->init())
            m_applets.remove(i);
        else
            ++i;
    }
    return true;
}
 
void PanelWindow::setApplets()
{
    QDir plugDir(qApp->applicationDirPath() + "/plugins");
    if (!plugDir.exists() && QDir("/usr/lib/hde/panel/plugins").exists())
        plugDir.cd("/usr/lib/hde/panel/plugins");

    for (const QString& applet_id : m_appletnames) {
        loadApplet(applet_id, plugDir);
    }
}
 
void PanelWindow::loadApplet(QString applet_id, QDir &plugDir)
{
    const int idx = applet_id.lastIndexOf('_');
    const QString name = applet_id.left(idx);
    const QString path = plugDir.absolutePath() + "/lib" + name.toLower() + ".so";

    if (QLibrary::isLibrary(path)) {
        QPluginLoader loader(path, this);
        if (auto* plugin = qobject_cast<AppletPlugin*>(loader.instance())) {
            if (Applet* a = plugin->createApplet(this)) {
                a->setId(applet_id);
                m_applets.append(a);
                m_scene->addItem(a);
            }
        } else {
            qDebug() << loader.errorString();
        }
    }
}
 
void PanelWindow::removeApplets()
{
    qDebug() << "removing apllets";
    
    // Disconnect all applets from the scene first
    for (Applet* applet : m_applets) {
        if (applet && m_scene) {
            m_scene->removeItem(applet);
        }
    }
    
    // Call applet close to ensure internal resources are released
    // and disconnect any signal connections
    for (Applet* applet : m_applets) {
        if (applet) {
            applet->close();
        }
    }

    // Then delete them (don't call close() again - it was already called above)
    while (!m_applets.isEmpty()) {
        if (Applet* a = m_applets.takeLast()) {
            delete a;
        }
    }
}
 
// ---------------------- Public setters -> schedule geometry work ----------------------
 
void PanelWindow::setDockMode(bool dockMode)
{
    if (m_dockMode == dockMode) return;
    m_dockMode = dockMode;

#if QT_VERSION < 0x060000
    if (QX11Info::isPlatformX11())
        setAttribute(Qt::WA_X11NetWmWindowTypeDock, m_dockMode);
#else
    if (qApp->platformName().toLower().contains("xcb"))
        setAttribute(Qt::WA_X11NetWmWindowTypeDock, m_dockMode);
#endif

    if (!m_dockMode) {
        X11Support::removeWindowProperty(winId(), "_NET_WM_STRUT");
        X11Support::removeWindowProperty(winId(), "_NET_WM_STRUT_PARTIAL");
    }

    updateLayout();
    updatePosition();
    scheduleApplyStruts();
}

void PanelWindow::setScreen(int screen)
{
    if (m_screen == screen) return;
    m_screen = screen;
    updateLayout();
    updatePosition();
    scheduleApplyStruts();
}

void PanelWindow::setPosition(Position p)
{
    if (m_position == p) return;
    m_position = p;
    
    // Set orientation based on position
    if (p == Top || p == Bottom) {
        m_orientation = Horizontal;
    } else {
        m_orientation = Vertical;
    }
    
    updateLayout();
    updatePosition();
    updateWaylandLayerShellConfiguration();
    scheduleApplyStruts();
}

void PanelWindow::updateWaylandLayerShellConfiguration()
{
    if (!LayerShellQtIntegration::isWayland())
        return;

    if (!windowHandle())
        return;

    const QRect screen = currentScreenGeometry();
    const QRect avail  = getAvailableScreenGeometry();

    const bool horizontal = (m_position == Top || m_position == Bottom);

    // Desired size based on current settings and screen/workarea
    QSize desiredSize;
    if (horizontal) {
        desiredSize = QSize(screen.width(), m_panelHeight);
    } else {
        // Side panels should span the *available* height so they start below top panels
        // and stop above bottom panels (Wayland compositor handles this via exclusive zones).
        desiredSize = QSize(m_panelWidth, avail.height());
    }

    // Keep QWidget size aligned with what we request from layer-shell
    if (size() != desiredSize) {
        m_updatingLayout = true;
        resize(desiredSize);
        m_updatingLayout = false;
    }

    // Layer + anchors
    int layer = 1;
    int anchorMask = 0;
    uint32_t waylandAnchorMask = 0;

    switch (m_position) {
        case Top:
            layer = 2;
            anchorMask = 1 | 4 | 8; // Top | Left | Right
            waylandAnchorMask =
                (uint32_t)WaylandLayerShell::Anchor::Top |
                (uint32_t)WaylandLayerShell::Anchor::Left |
                (uint32_t)WaylandLayerShell::Anchor::Right;
            break;
        case Bottom:
            layer = 1;
            anchorMask = 2 | 4 | 8; // Bottom | Left | Right
            waylandAnchorMask =
                (uint32_t)WaylandLayerShell::Anchor::Bottom |
                (uint32_t)WaylandLayerShell::Anchor::Left |
                (uint32_t)WaylandLayerShell::Anchor::Right;
            break;
        case Left:
            layer = 1;
            anchorMask = 1 | 2 | 4; // Top | Bottom | Left
            waylandAnchorMask =
                (uint32_t)WaylandLayerShell::Anchor::Top |
                (uint32_t)WaylandLayerShell::Anchor::Bottom |
                (uint32_t)WaylandLayerShell::Anchor::Left;
            break;
        case Right:
            layer = 1;
            anchorMask = 1 | 2 | 8; // Top | Bottom | Right
            waylandAnchorMask =
                (uint32_t)WaylandLayerShell::Anchor::Top |
                (uint32_t)WaylandLayerShell::Anchor::Bottom |
                (uint32_t)WaylandLayerShell::Anchor::Right;
            break;
    }

    // Exclusive zone must match thickness (no hardcoding!)
    const int exclusiveZone = horizontal ? height() : width();

    // LayerShellQtIntegration (preferred)
    if (m_layerShellQt && m_layerShellQt->isAvailable()) {
        m_layerShellQt->configureWindow(windowHandle(),
                                        layer,
                                        anchorMask,
                                        exclusiveZone,
                                        QMargins(0, 0, 0, 0),
                                        0);
        m_layerShellQt->setSize(windowHandle(), desiredSize);
        return;
    }

    // Custom layer-shell
    if (m_waylandLayerShell && m_waylandLayerShell->isAvailable()) {
        m_waylandLayerShell->setAnchorMask(waylandAnchorMask);
        m_waylandLayerShell->setExclusiveZone(exclusiveZone);
        m_waylandLayerShell->setSize(desiredSize);
        m_waylandLayerShell->commit();
        return;
    }
}
 
void PanelWindow::setOrientation(Orientation o) { m_orientation = o; }
void PanelWindow::setLayoutPolicy(LayoutPolicy p)
{
    if (m_layoutPolicy == p) return;
    m_layoutPolicy = p;
    updateLayout();
    updatePosition();
    updateWaylandLayerShellConfiguration();
    scheduleApplyStruts();
}

void PanelWindow::setPanelHeight(int height)
{
    // Validate the height
    if (height < 24) height = 24;
    if (height > 200) height = 200;
    if (m_panelHeight == height) {
        qDebug() << "PanelWindow::setPanelHeight - height unchanged:" << height;
        return;
    }
    
    qDebug() << "PanelWindow::setPanelHeight - changing from" << m_panelHeight << "to" << height;
    m_panelHeight = height;
    // Don't save here - the valueChanged handler in PanelSettings will save it
    // This prevents double-saving and ensures the UI is the source of truth
    
    if (m_position == Top || m_position == Bottom) {
        // Set guard flag to prevent resizeEvent from interfering
        m_updatingLayout = true;
        updateLayout();
        updatePosition();
        m_updatingLayout = false;
        scheduleApplyStruts();
    }
}

void PanelWindow::setPanelWidth(int width)
{
    // Validate the width
    if (width < 10) width = 10;
    if (width > 200) width = 200;
    if (m_panelWidth == width) return;
    
    m_panelWidth = width;
    // Don't save here - the valueChanged handler in PanelSettings will save it
    // This prevents double-saving and ensures the UI is the source of truth
    
    if (m_position == Left || m_position == Right) {
        // Set guard flag to prevent resizeEvent from interfering
        m_updatingLayout = true;
        updateLayout();
        updatePosition();
        m_updatingLayout = false;
        scheduleApplyStruts();
    }
}

// ---------------------- Geometry helpers ----------------------

QRect PanelWindow::currentScreenGeometry() const
{
#if QT_VERSION >= 0x050000
    const auto screens = QGuiApplication::screens();
    if (m_screen >= 0 && m_screen < screens.size())
        return screens[m_screen]->geometry();
    return screens.isEmpty() ? QRect(0,0,1920,1080) : screens.first()->geometry();
#else
    return QApplication::desktop()->screenGeometry(m_screen);
#endif
}

QRect PanelWindow::getAvailableScreenGeometry() const
{
    const QRect screen = currentScreenGeometry();
    QRect available = screen;

#if QT_VERSION >= 0x050000
    const auto screens = QGuiApplication::screens();
    QRect scrAvail = (!screens.isEmpty() ?
        (m_screen >= 0 && m_screen < screens.size() ? screens[m_screen]->availableGeometry()
                                                    : screens.first()->availableGeometry())
        : QRect());
    if (scrAvail.isValid()) available = scrAvail;
#else
    QRect scrAvail = QApplication::desktop()->availableGeometry(m_screen);
    if (scrAvail.isValid()) available = scrAvail;
#endif

    // Try EWMH _NET_WORKAREA (X11)
#if QT_VERSION < 0x060000
    const bool isX11 = QX11Info::isPlatformX11();
#else
    const bool isX11 = qApp->platformName().toLower().contains("xcb");
#endif
    if (isX11) {
        QVector<unsigned long> wa = X11Support::getWindowPropertyCardinalArray(
            X11Support::rootWindow(), "_NET_WORKAREA");
        if (!wa.isEmpty() && (wa.size()%4)==0) {
            unsigned long deskCount = static_cast<unsigned long>(wa.size()/4);
            unsigned long curDesk = X11Support::getWindowPropertyCardinal(
                X11Support::rootWindow(), "_NET_CURRENT_DESKTOP");
            if (deskCount == 0) deskCount = 1;
            if (curDesk >= deskCount) curDesk = 0;
            const int off = static_cast<int>(curDesk*4);
            if (off+3 < wa.size()) {
                QRect r( (int)wa[off], (int)wa[off+1], (int)wa[off+2], (int)wa[off+3] );
                if (r.isValid() && !r.isEmpty()) {
                    QRect cand = r.intersected(screen);
                    available = cand.isValid() ? cand : r;
                }
            }
        }
    }

    return available;
}

QRect PanelWindow::getAnchorGeometry(const QRect& screen, const QRect& available) const
{
    QRect anchor = available.intersected(screen);
    if (!anchor.isValid() || anchor.isEmpty())
        anchor = available.isValid() ? available : screen;
    return anchor;
}

// ---------------------- Layout & Position ----------------------
void PanelWindow::updateLayout()
{
    if (m_updatingLayout) return;
    m_updatingLayout = true;

    const QRect screen = currentScreenGeometry();

#if QT_VERSION < 0x060000
    const bool isX11 = QX11Info::isPlatformX11();
    Display* dpy = isX11 ? QX11Info::display() : nullptr;
#else
    const bool isX11 = qApp->platformName().toLower().contains("xcb");
    Display* dpy = nullptr;
    if (isX11) {
        auto native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
        dpy = native ? native->display() : nullptr;
    }
#endif

    // --- external struts (exclude our own panels) ---
    QMargins ext(0,0,0,0);
    if (isX11 && winId() != 0) {
        ext = X11Support::getExternalStrutReservations(screen, winId());
        const int gnomeTop = detectGnomeTopOffsetPx();
        if (gnomeTop > 0) ext.setTop(qMax(ext.top(), gnomeTop));
    }

    auto readCardinalIfExists = [&](Window w, Atom prop, unsigned long& out) -> bool {
        if (!dpy) return false;
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long nitems = 0, bytesAfter = 0;
        unsigned char* data = nullptr;
        const int rc = XGetWindowProperty(dpy, w, prop, 0, 1, False, XA_CARDINAL,
                                         &actualType, &actualFormat, &nitems, &bytesAfter, &data);
        if (rc != Success) return false;
        bool ok = (data && actualType == XA_CARDINAL && actualFormat == 32 && nitems >= 1);
        if (ok) out = reinterpret_cast<unsigned long*>(data)[0];
        if (data) XFree(data);
        return ok;
    };

    // --- own total thickness at Top/Bottom (for side usable area) ---
    int ownTopTotal = 0;
    int ownBottomTotal = 0;

    if (isX11 && dpy) {
        const Atom aPos = XInternAtom(dpy, "_HDE_PANEL_POS", False);
        const QVector<unsigned long> clients =
            X11Support::getWindowPropertyWindowsArray(X11Support::rootWindow(), "_NET_CLIENT_LIST");

        for (unsigned long w : clients) {
            if (!w) continue;

            const QString name = X11Support::getWindowName(w);
            if (!name.contains("hdepanel", Qt::CaseInsensitive) &&
                !name.contains("HDE Panel", Qt::CaseInsensitive))
                continue;

            unsigned long pval = 0;
            if (!readCardinalIfExists((Window)w, aPos, pval))
                continue;

            const QRect g = X11Support::getWindowWindowsGeometry(w);
            if (!g.isValid() || g.isEmpty()) continue;
            if (!screen.contains(g.center())) continue;

            if ((int)pval == (int)Top)    ownTopTotal    += g.height();
            if ((int)pval == (int)Bottom) ownBottomTotal += g.height();
        }

        if (m_position == Top)    ownTopTotal    += height();
        if (m_position == Bottom) ownBottomTotal += height();
    }

    const int spacing = 4;

    // ---- Resize panel itself to canonical size ----
    if (m_position == Top || m_position == Bottom) {
        const int desiredW = qMax(1, screen.width());
        const int desiredH = qMax(10, m_panelHeight);
        if (width() != desiredW || height() != desiredH)
            resize(desiredW, desiredH);
    } else {
        const int sideTop    = screen.top() + ext.top() + ownTopTotal;
        const int sideBottom = screen.bottom() - ext.bottom() - ownBottomTotal;
        const int usableH    = qMax(1, sideBottom - sideTop + 1);

        const int desiredW = qMax(10, m_panelWidth);
        const int desiredH = usableH;

        if (width() != desiredW || height() != desiredH)
            resize(desiredW, desiredH);
    }

    // ---- Layout applets: PRIMARY expander = first applet with expandable()==true ----
    const bool horizontal = (m_position == Top || m_position == Bottom);

    const int gaps = qMax(0, m_applets.size() - 1);
    const int axisLen  = horizontal ? width()  : height();
    const int crossLen = horizontal ? height() : width();

    int available = axisLen - spacing * gaps;
    if (available < 0) available = 0;

    Applet* primaryExpander = nullptr;
    QVector<Applet*> flex;        // desiredSize() == -1 on main axis
    QVector<Applet*> otherFlex;

    int fixedSum = 0;

    for (Applet* a : m_applets) {
        if (!a) continue;
        const QSize ds = a->desiredSize();
        const int want = horizontal ? ds.width() : ds.height();

        if (want >= 0) {
            fixedSum += want;
        } else {
            flex.push_back(a);
            if (!primaryExpander && a->expandable())
                primaryExpander = a;
        }
    }

    int remaining = available - fixedSum;
    if (remaining < 0) remaining = 0;

    QHash<Applet*, int> mainSize;

    // fixed sizes
    for (Applet* a : m_applets) {
        if (!a) continue;
        const QSize ds = a->desiredSize();
        const int want = horizontal ? ds.width() : ds.height();
        if (want >= 0)
            mainSize[a] = want;
    }

    // flexible sizes
    if (!flex.isEmpty()) {
        if (primaryExpander) {
            for (Applet* a : flex)
                if (a && a != primaryExpander)
                    otherFlex.push_back(a);

            const int oc = otherFlex.size();
            int share = 0;
            if (oc > 0) {
                // keep primaryExpander larger than other flex items
                share = remaining / (oc + 2);
                if (share < 0) share = 0;
            }

            for (Applet* a : otherFlex)
                mainSize[a] = share;

            const int used = share * oc;
            int big = remaining - used;
            if (big < 0) big = 0;
            mainSize[primaryExpander] = big;
        } else {
            const int fc = flex.size();
            int per = (fc > 0) ? (remaining / fc) : 0;
            int rem = remaining;

            for (int i = 0; i < flex.size(); ++i) {
                Applet* a = flex[i];
                if (!a) continue;

                int v = (i == flex.size() - 1) ? rem : per;
                if (v < 0) v = 0;
                mainSize[a] = v;
                rem -= v;
            }
        }
    }

    // place applets
    int pos = 0;
    for (Applet* a : m_applets) {
        if (!a) continue;

        int main = mainSize.value(a, 0);
        if (main < 0) main = 0;

        if (horizontal) {
            a->setPosition(QPoint(pos, 0));
            a->setSize(QSize(main, crossLen));
        } else {
            a->setPosition(QPoint(0, pos));
            a->setSize(QSize(crossLen, main));
        }

        pos += main + spacing;
    }

    m_updatingLayout = false;
}

int PanelWindow::detectGnomeTopOffsetPx() const
{
#if QT_VERSION < 0x060000
    const bool isX11 = QX11Info::isPlatformX11();
#else
    const bool isX11 = qApp->platformName().toLower().contains("xcb");
#endif
    
    // Check if we're on Wayland (even if using XWayland for the panel)
    const QByteArray sessionType = qgetenv("XDG_SESSION_TYPE").toLower();
    const bool isWayland = (sessionType == "wayland");
    
    // On Wayland, GNOME top bar is a native Wayland window, not visible via X11
    // Use a reasonable default or try to detect via available screen geometry
    if (isWayland) {
        // Try to get available screen geometry (workarea) vs full screen
        // The difference at the top should be the GNOME top bar height
        const QRect screen = currentScreenGeometry();
        const QRect available = getAvailableScreenGeometry();
        
        int topBarHeight = screen.top() - available.top();
        if (topBarHeight > 0 && topBarHeight <= 128) {
            return topBarHeight;
        }
        
        // Fallback: common GNOME top bar heights
        // GNOME typically uses 27-30px, but can be customized
        return 30; // Common GNOME top bar height
    }
    
    // On native X11, try to detect via X11 window queries
    Display* dpy = nullptr;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (isX11) {
        dpy = QX11Info::display();
    }
#else
    if (isX11) {
        if (auto native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()) {
            dpy = native->display();
        }
    }
#endif
    
    // Fallback: try opening display directly (useful for XWayland on Wayland)
    bool openedDisplay = false;
    if (!dpy) {
        const char* displayName = qgetenv("DISPLAY").constData();
        if (displayName && *displayName) {
            dpy = XOpenDisplay(displayName);
            openedDisplay = (dpy != nullptr);
        }
    }
    
    if (!dpy) {
        return 0;
    }

    // This function already scans for GNOME Shell / Top Bar windows.
    // If it finds nothing, we return 0 (NO guessed fallback).
    const int h = X11Support::detectTopPanelHeight(dpy);
    
    // Close display if we opened it ourselves (don't close Qt's display)
    if (openedDisplay && dpy) {
        XCloseDisplay(dpy);
    }
    
    if (h <= 0 || h > 128) {
        return 0;
    }

    return h;
}

void PanelWindow::updatePosition()
{
    // If layer-shell is active, compositor places us.
    if ((m_layerShellQt && m_layerShellQt->isAvailable()) ||
        (m_waylandLayerShell && m_waylandLayerShell->isAvailable())) {
        return;
    }

    if (winId() == 0 || width() <= 0 || height() <= 0)
        return;

    // Defensive orientation sync
    if (m_position == Left || m_position == Right)
        m_orientation = Vertical;
    else
        m_orientation = Horizontal;

    const QRect screen = currentScreenGeometry();

#if QT_VERSION < 0x060000
    const bool isX11 = QX11Info::isPlatformX11();
    Display* dpy = isX11 ? QX11Info::display() : nullptr;
#else
    const bool isX11 = qApp->platformName().toLower().contains("xcb");
    Display* dpy = nullptr;
    if (isX11) {
        auto native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
        dpy = native ? native->display() : nullptr;
    }
#endif

    // External struts
    QMargins ext(0,0,0,0);
    if (isX11) {
        ext = X11Support::getExternalStrutReservations(screen, winId());
        const int gnomeTop = detectGnomeTopOffsetPx();
        if (gnomeTop > 0) ext.setTop(qMax(ext.top(), gnomeTop));
    }

    // Publish ourselves early
    if (isX11) {
        X11Support::setWindowPropertyCardinal(winId(), "_HDE_PANEL", 1);
        X11Support::setWindowPropertyCardinal(winId(), "_HDE_PANEL_POS", (unsigned long)m_position);
    }

    auto readCardinalIfExists = [&](Window w, Atom prop, unsigned long& out) -> bool {
        if (!dpy) return false;
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long nitems = 0, bytesAfter = 0;
        unsigned char* data = nullptr;
        const int rc = XGetWindowProperty(dpy, w, prop, 0, 1, False, XA_CARDINAL,
                                         &actualType, &actualFormat, &nitems, &bytesAfter, &data);
        if (rc != Success) return false;
        bool ok = (data && actualType == XA_CARDINAL && actualFormat == 32 && nitems >= 1);
        if (ok) out = reinterpret_cast<unsigned long*>(data)[0];
        if (data) XFree(data);
        return ok;
    };

    // Total top/bottom stacks
    int ownTopTotal = 0;
    int ownBottomTotal = 0;

    if (isX11 && dpy) {
        const Atom aPos = XInternAtom(dpy, "_HDE_PANEL_POS", False);
        const QVector<unsigned long> clients =
            X11Support::getWindowPropertyWindowsArray(X11Support::rootWindow(), "_NET_CLIENT_LIST");

        for (unsigned long w : clients) {
            if (!w) continue;

            const QString name = X11Support::getWindowName(w);
            if (!name.contains("hdepanel", Qt::CaseInsensitive) &&
                !name.contains("HDE Panel", Qt::CaseInsensitive))
                continue;

            unsigned long pval = 0;
            if (!readCardinalIfExists((Window)w, aPos, pval))
                continue;

            const QRect g = X11Support::getWindowWindowsGeometry(w);
            if (!g.isValid() || g.isEmpty()) continue;
            if (!screen.contains(g.center())) continue;

            if ((int)pval == (int)Top)    ownTopTotal    += g.height();
            if ((int)pval == (int)Bottom) ownBottomTotal += g.height();
        }
    }

    // Deterministic stacking offset for THIS side (multiple panels on same edge)
    int offset = 0;
    if (isX11 && dpy) {
        const Atom aPos = XInternAtom(dpy, "_HDE_PANEL_POS", False);
        const QVector<unsigned long> clients =
            X11Support::getWindowPropertyWindowsArray(X11Support::rootWindow(), "_NET_CLIENT_LIST");

        struct Item { unsigned long wid; int thick; };
        QVector<Item> sameSide;

        for (unsigned long w : clients) {
            if (!w) continue;

            const QString name = X11Support::getWindowName(w);
            if (!name.contains("hdepanel", Qt::CaseInsensitive) &&
                !name.contains("HDE Panel", Qt::CaseInsensitive))
                continue;

            unsigned long pval = 0;
            if (!readCardinalIfExists((Window)w, aPos, pval))
                continue;
            if ((int)pval != (int)m_position)
                continue;

            const QRect g = X11Support::getWindowWindowsGeometry(w);
            if (!g.isValid() || g.isEmpty()) continue;
            if (!screen.contains(g.center())) continue;

            const int thick = (m_position == Top || m_position == Bottom) ? g.height() : g.width();
            if (thick > 0) sameSide.push_back({w, thick});
        }

        const unsigned long self = (unsigned long)winId();
        bool hasSelf = false;
        for (const auto& it : sameSide) if (it.wid == self) { hasSelf = true; break; }
        if (!hasSelf) {
            const int thickSelf = (m_position == Top || m_position == Bottom) ? height() : width();
            sameSide.push_back({self, thickSelf});
        }

        std::sort(sameSide.begin(), sameSide.end(),
                  [](const Item& a, const Item& b){ return a.wid < b.wid; });

        for (const auto& it : sameSide) {
            if (it.wid == self) break;
            offset += it.thick;
        }
    }

    // Side usable vertical span
    const int sideTop    = screen.top()    + ext.top()    + ownTopTotal;
    const int sideBottom = screen.bottom() - ext.bottom() - ownBottomTotal;

    // Enforce full width for Top/Bottom
    if (m_position == Top || m_position == Bottom) {
        if (width() != screen.width())
            resize(screen.width(), height());
    }

    int x = screen.left();
    int y = screen.top();

    switch (m_position) {
        case Top:
            x = screen.left();
            y = screen.top() + ext.top() + offset;
            break;
        case Bottom:
            x = screen.left();
            y = screen.bottom() - ext.bottom() - offset - height() + 1;
            break;
        case Left:
            x = screen.left() + ext.left() + offset;
            y = sideTop;
            break;
        case Right:
            x = screen.right() - ext.right() - offset - width() + 1;
            y = sideTop;
            break;
    }

    // Clamp so panels never disappear offscreen
    x = qBound(screen.left(), x, screen.right() - width() + 1);

    if (m_position == Left || m_position == Right) {
        // Clamp within the side span, not the whole screen
        int maxY = sideBottom - height() + 1;
        if (maxY < sideTop) maxY = sideTop;
        y = qBound(sideTop, y, maxY);
    } else {
        y = qBound(screen.top(), y, screen.bottom() - height() + 1);
    }

    const QRect newGeom(x, y, width(), height());
    if (geometry() != newGeom) {
        setGeometry(newGeom);
        raise();
        scheduleApplyStruts();
    }
}
 
// ---------------------- Strut application (debounced) ----------------------

void PanelWindow::setupStrutProperties()
{
    // called during init paths if needed
    scheduleApplyStruts();
}

void PanelWindow::scheduleApplyStruts()
{
    // coalesce multiple callers
    m_strutDebounce.start();
}

void PanelWindow::scheduleApplyStrutsIfMoved()
{
    if (geometry() != m_lastStrutGeom)
        scheduleApplyStruts();
}

void PanelWindow::applyX11Struts(const QRect& panelGeom)
{
#if QT_VERSION < 0x060000
    if (!QX11Info::isPlatformX11()) return;
#else
    if (!qApp->platformName().toLower().contains("xcb")) return;
#endif
    if (!m_dockMode) return;
    if (!panelGeom.isValid() || panelGeom.isEmpty()) return;

    // Prevent feedback loops
    if (panelGeom == m_lastStrutGeom &&
        m_lastStrutApply.isValid() &&
        m_lastStrutApply.elapsed() < 800) {
        return;
    }

    const QRect screen = currentScreenGeometry();

    int left = 0, right = 0, top = 0, bottom = 0;
    int leftStartY = 0, leftEndY = 0;
    int rightStartY = 0, rightEndY = 0;
    int topStartX = screen.left();
    int topEndX   = screen.right();
    int bottomStartX = screen.left();
    int bottomEndX   = screen.right();

    switch (m_position) {
        case Top:
            // Absolute distance from screen top to *this panel's bottom*
            top = panelGeom.bottom() - screen.top() + 1;
            if (top < 0) top = 0;
            break;
        case Bottom:
            // Absolute distance from *this panel's top* to screen bottom
            bottom = screen.bottom() - panelGeom.top() + 1;
            if (bottom < 0) bottom = 0;
            break;
        case Left:
            // Absolute distance from screen left to *this panel's right*
            left = panelGeom.right() - screen.left() + 1;
            if (left < 0) left = 0;
            leftStartY = panelGeom.top();
            leftEndY = panelGeom.bottom();
            break;
        case Right:
            // Absolute distance from *this panel's left* to screen right
            right = screen.right() - panelGeom.left() + 1;
            if (right < 0) right = 0;
            rightStartY = panelGeom.top();
            rightEndY = panelGeom.bottom();
            break;
    }

    X11Support::setStrut(
        winId(),
        left, right, top, bottom,
        leftStartY, leftEndY,
        rightStartY, rightEndY,
        topStartX, topEndX,
        bottomStartX, bottomEndX
    );

    m_lastStrutGeom = panelGeom;
    if (!m_lastStrutApply.isValid())
        m_lastStrutApply.start();
    else
        m_lastStrutApply.restart();
}

// ---------------------- Wayland fallback ----------------------
 
void PanelWindow::forceWaylandPosition()
{
    static int callCount = 0;
    callCount++;
    
#if QT_VERSION < 0x060000
    const bool isX11 = QX11Info::isPlatformX11();
#else
    const bool isX11 = qApp->platformName().toLower().contains("xcb");
#endif
    if (isX11) {
        if (callCount == 1) qDebug() << "PanelWindow::forceWaylandPosition - X11 detected, stopping timer";
        return;
    }
    if (!isVisible()) {
        if (callCount % 100 == 1) qDebug() << "PanelWindow::forceWaylandPosition - call #" << callCount << " - panel not visible";
        return;
    }

    // Reassert position based on our rules (best-effort under Wayland)
    const QRect screen    = currentScreenGeometry();
    const QRect available = getAvailableScreenGeometry();
    QRect anchor          = getAnchorGeometry(screen, available);

    int x = screen.left();
    int y = screen.top();
    int panelWidth = width();
    int panelHeight = height();
    
    switch (m_position) {
        case Top:
            x = anchor.left();
            y = anchor.top();
            panelWidth = qMin(width(), screen.width());
            break;
        case Bottom:
            x = anchor.left();
            y = anchor.bottom() - panelHeight + 1;
            panelWidth = qMin(width(), screen.width());
            break;
        case Left:
            x = anchor.left();
            y = anchor.top();
            panelHeight = qMin(height(), screen.height());
            break;
        case Right:
            x = anchor.right() - panelWidth + 1;
            y = anchor.top();
            panelHeight = qMin(height(), screen.height());
            break;
    }

    qDebug() << "PanelWindow::forceWaylandPosition - screen:" << screen << "available:" << available 
             << "anchor:" << anchor << "position:" << m_position 
             << "current pos:" << geometry().topLeft() << "target pos:" << QPoint(x,y)
             << "current size:" << size() << "target size:" << QSize(panelWidth, panelHeight);

    // Resize and move the panel
    QRect targetGeometry(x, y, panelWidth, panelHeight);
    if (geometry() != targetGeometry) {
        qDebug() << "PanelWindow::forceWaylandPosition - resizing and moving panel to:" << targetGeometry;
        setGeometry(targetGeometry);
        
        // Force the panel to maintain its size and position
        setFixedSize(panelWidth, panelHeight);
        move(x, y);
        
        update();
        repaint();
    }
    
    // Ensure the panel is visible and on top
    if (!isVisible()) {
        show();
        raise();
        activateWindow();
    }
}

// ---------------------- Misc UI ----------------------

int PanelWindow::textBaseLine()
{
    QFontMetrics m(font());

    // For horizontal panels, base the text position on the full panel height
    // (classic behaviour). For vertical panels, use the configured panel
    // "thickness" instead so that applet text is centred within each applet
    // cell, not the full screen‑height side panel.
    int refHeight = height();
    if (m_orientation == Vertical) {
        // Use the same logical thickness as horizontal panels so vertical
        // applets look similar in size and don't end up vertically centred
        // in the middle of a tall side panel.
        refHeight = m_panelHeight;
    }

    return (refHeight - m.height())/2 + m.ascent();
}

void PanelWindow::showPanelContextMenu(const QPoint& point)
{
    //QMenu menu;
    HPopupMenu menu;
    /*
     * -= Panel =-
     * Configure panel
     * Add Panel
     * Remove Panel
     */
    menu.addTitle(tr("Panel"));
    menu.addAction(QIcon::fromTheme("preferences-desktop"), tr("Configure Panel"), this, SLOT(showConfigurationDialog()));

    menu.addAction(QIcon::fromTheme("list-add"), tr("Add Panel"), QApplication::instance(), SLOT(addPanel()));
    menu.addAction(QIcon::fromTheme("list-remove"), tr("Remove Panel"), this, SLOT(removePanel()));

    menu.exec(pos() + point);
}


void PanelWindow::showConfigurationDialog()
{
    PanelSettings *panelSettings = new PanelSettings(m_id);
    panelSettings->setPanelWindow(this);
    if(panelSettings->exec())
    {

    }
    delete panelSettings;
}


void PanelWindow::removePanel()
{
    static_cast<PanelApplication*>(QApplication::instance())->removePanel(m_id);
}

void PanelWindow::setFontName(const QString& fontName)
{
    QFont f = (fontName != "default") ? QFont() : QApplication::font();
    if (fontName != "default") {
        const int lastSpace = fontName.lastIndexOf(' ');
        if (lastSpace != -1) {
            bool ok = false;
            const int size = fontName.mid(lastSpace+1).toInt(&ok);
            const QString fam = fontName.left(lastSpace);
            f = QFont(fam, ok ? size : f.pointSize());
        }
    }
    setFont(f);
    for (Applet* a : m_applets) a->fontChanged();
}

Applet* PanelWindow::getAppletById(const QString& appletId) const
{
    for (Applet* applet : m_applets) {
        if (applet && applet->id() == appletId) {
            return applet;
        }
    }
    return nullptr;
}

void PanelWindow::showCustomTooltip(const QString& text, const QPoint& itemPos)
{
    if (!m_customTooltip || text.isEmpty()) {
        hideCustomTooltip();
        return;
    }
    
    // Convert item position (in view coordinates) to global coordinates
    QPoint globalPos = m_view->mapToGlobal(itemPos);
    
    // Calculate tooltip position based on panel position
    m_customTooltip->setText(text);
    m_customTooltip->adjustSize();
    QSize tooltipSize = m_customTooltip->sizeHint();
    if (tooltipSize.width() < 50) tooltipSize.setWidth(50); // Minimum width
    if (tooltipSize.height() < 20) tooltipSize.setHeight(20); // Minimum height
    QPoint tooltipPos = calculateTooltipPosition(globalPos, tooltipSize);
    
    m_customTooltip->showAtPosition(tooltipPos);
}

void PanelWindow::hideCustomTooltip()
{
    if (m_customTooltip) {
        m_customTooltip->hide();
    }
}

QPoint PanelWindow::calculateTooltipPosition(const QPoint& itemGlobalPos, const QSize& tooltipSize) const
{
    QPoint pos = itemGlobalPos;
    const int spacing = 8; // Space between panel and tooltip
    
    switch (m_position) {
        case Left:
            // Panel on left, tooltip on right
            pos.setX(itemGlobalPos.x() + width() + spacing);
            pos.setY(itemGlobalPos.y() - tooltipSize.height() / 2);
            break;
        case Right:
            // Panel on right, tooltip on left
            pos.setX(itemGlobalPos.x() - tooltipSize.width() - spacing);
            pos.setY(itemGlobalPos.y() - tooltipSize.height() / 2);
            break;
        case Bottom:
            // Panel at bottom, tooltip on top
            pos.setX(itemGlobalPos.x() - tooltipSize.width() / 2);
            pos.setY(itemGlobalPos.y() - tooltipSize.height() - spacing);
            break;
        case Top:
            // Panel at top, tooltip on bottom
            pos.setX(itemGlobalPos.x() - tooltipSize.width() / 2);
            pos.setY(itemGlobalPos.y() + height() + spacing);
            break;
    }
    
    // Ensure tooltip stays on screen
    QRect screenRect = currentScreenGeometry();
    if (pos.x() < screenRect.left()) {
        pos.setX(screenRect.left() + 5);
    } else if (pos.x() + tooltipSize.width() > screenRect.right()) {
        pos.setX(screenRect.right() - tooltipSize.width() - 5);
    }
    
    if (pos.y() < screenRect.top()) {
        pos.setY(screenRect.top() + 5);
    } else if (pos.y() + tooltipSize.height() > screenRect.bottom()) {
        pos.setY(screenRect.bottom() - tooltipSize.height() - 5);
    }
    
    return pos;
}

void PanelWindow::resetApplets()
{
    removeApplets();

    // Reload applet list from settings
    m_appletnames = Settings::value(m_id, "applets", QStringList()).toStringList();

    setApplets();
    init();  // re-initialize all applets
    updateLayout();
    updatePosition();
}
