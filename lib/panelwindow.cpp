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

// X11 headers for GNOME top bar detection (needed for XOpenDisplay/XCloseDisplay)
#include <X11/Xlib.h>
 
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
 
#if defined(Q_OS_UNIX)
#  include <X11/Xlib.h>
#  include <X11/Xatom.h>
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  include <QGuiApplication>
#else
#  include <QX11Info>
#endif

#if defined(Q_OS_UNIX)
#  include <xcb/xcb.h>
#endif

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
	painter->setBrush(QColor(0, 0, 0, 128));
	painter->drawRect(boundingRect());

	static const int borderThickness = 3;
	if(m_panelWindow->verticalAnchor() == PanelWindow::Min)
	{
		QLinearGradient gradient(0.0, m_panelWindow->height() - borderThickness, 0.0, m_panelWindow->height());
		gradient.setSpread(QGradient::RepeatSpread);
		gradient.setColorAt(0.0, QColor(255, 255, 255, 0));
		gradient.setColorAt(1.0, QColor(255, 255, 255, 128));
		painter->setBrush(QBrush(gradient));
		painter->drawRect(0.0, m_panelWindow->height() - borderThickness, m_panelWindow->width(), borderThickness);
	}
	else
	{
		QLinearGradient gradient(0.0, 0.0, 0.0, borderThickness);
		gradient.setSpread(QGradient::RepeatSpread);
		gradient.setColorAt(0.0, QColor(255, 255, 255, 128));
		gradient.setColorAt(1.0, QColor(255, 255, 255, 0));
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
    m_horizontalAnchor= Center;
    m_verticalAnchor  = Max;         // bottom by default
    m_orientation     = Horizontal;
    m_layoutPolicy    = FillSpace;   // most panels stretch full width
 
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
    m_view->setAttribute(Qt::WA_NoMousePropagation);
    m_view->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_view->setBackgroundBrush(Qt::NoBrush);
     
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
     
    // Window flags / attributes - must be set before showing to prevent decorations
    // Set frameless and always on top for proper panel behavior
    setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMinimumSize(100, 48);
    
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
    
    // Handle Wayland positioning with layer-shell
    if (LayerShellQtIntegration::isWayland()) {
        // Use Wayland layer-shell for proper panel behavior
        // Try LayerShellQt first (preferred method)
        qDebug() << "PanelWindow: Checking LayerShellQt - m_layerShellQt:" << (m_layerShellQt ? "exists" : "null") 
                 << "isAvailable:" << (m_layerShellQt ? m_layerShellQt->isAvailable() : false);
        if (m_layerShellQt && m_layerShellQt->isAvailable()) {
            qDebug() << "PanelWindow: Using LayerShellQt for positioning";
            
            // Set appropriate window flags first
            setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
            setAttribute(Qt::WA_ShowWithoutActivating);
            
            // Show the window first, then configure
            show();
            raise();
            activateWindow();
            setVisible(true);
            setWindowState(Qt::WindowActive);
            setFocus();
            
            // Now configure the window for layer shell after it's shown
            // Set layer and anchor based on current position setting
            int layer = (m_verticalAnchor == Min) ? 2 : 1; // LayerTop : LayerBottom
            int anchorMask = (m_verticalAnchor == Min) ? 
                (1 | 4 | 8) : // AnchorTop | AnchorLeft | AnchorRight
                (2 | 4 | 8);  // AnchorBottom | AnchorLeft | AnchorRight
            
            m_layerShellQt->configureWindow(windowHandle(), 
                                           layer,
                                           anchorMask,
                                           48, // exclusive zone
                                           QMargins(0, 0, 0, 0), // margins
                                           0); // keyboard interactivity
            
            // Set size
            m_layerShellQt->setSize(windowHandle(), QSize(1920, 48));
            
            // Stop timer when using layer shell
            if (m_waylandRepositionTimer && m_waylandRepositionTimer->isActive()) {
                m_waylandRepositionTimer->stop();
            }
        } else if (m_waylandLayerShell && m_waylandLayerShell->isAvailable()) {
            // Set up layer-shell for the Qt window with improved positioning
            m_waylandLayerShell->setWindow(windowHandle());
            
            // Set layer and anchor based on current position setting
            if (m_verticalAnchor == Min) {
                // Top position
                m_waylandLayerShell->setLayer(WaylandLayerShell::Layer::Top);
                uint32_t anchorMask = static_cast<uint32_t>(WaylandLayerShell::Anchor::Top) | 
                                     static_cast<uint32_t>(WaylandLayerShell::Anchor::Left) | 
                                     static_cast<uint32_t>(WaylandLayerShell::Anchor::Right);
                m_waylandLayerShell->setAnchorMask(anchorMask);
            } else {
                // Bottom position (default)
                m_waylandLayerShell->setLayer(WaylandLayerShell::Layer::Bottom);
                uint32_t anchorMask = static_cast<uint32_t>(WaylandLayerShell::Anchor::Bottom) | 
                                     static_cast<uint32_t>(WaylandLayerShell::Anchor::Left) | 
                                     static_cast<uint32_t>(WaylandLayerShell::Anchor::Right);
                m_waylandLayerShell->setAnchorMask(anchorMask);
            }
            
            m_waylandLayerShell->setNamespace("hdepanel");
            m_waylandLayerShell->setKeyboardFocus(WaylandLayerShell::KeyboardFocus::OnDemand);
            m_waylandLayerShell->setExclusiveZone(48);
            m_waylandLayerShell->setSize(QSize(1920, 48));
            
            // Set appropriate window flags for Wayland layer shell
            setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
            setAttribute(Qt::WA_ShowWithoutActivating);
            
            show();
            raise();
            activateWindow();
            setVisible(true);
            setWindowState(Qt::WindowActive);
            setFocus();
            
            qDebug() << "PanelWindow: Custom layer-shell configured for" << (m_verticalAnchor == Min ? "top" : "bottom") << "positioning";
            qDebug() << "PanelWindow: Qt window visibility - visible:" << isVisible() 
                     << "geometry:" << geometry() << "windowState:" << windowState();
            
            // Stop timer when using layer shell
            if (m_waylandRepositionTimer && m_waylandRepositionTimer->isActive()) {
                m_waylandRepositionTimer->stop();
                qDebug() << "PanelWindow: Stopped Wayland reposition timer (using custom layer shell)";
            }
        } else {
            qDebug() << "PanelWindow: Layer-shell not available, using XCB-like positioning";
            
            // Set Wayland-specific window properties for dock-like behavior
            setWindowFlags(Qt::Window | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
            setAttribute(Qt::WA_ShowWithoutActivating);
            setAttribute(Qt::WA_X11NetWmWindowTypeDock, true);
            
            // Additional Wayland properties for better positioning
            setProperty("_kde_net_wm_window_type", "_NET_WM_WINDOW_TYPE_DOCK");
            setProperty("_kde_net_wm_desktop", 0xFFFFFFFF); // All desktops
            
            // Use the same positioning logic as XCB for consistent behavior
            updatePosition();
            
            // Ensure the panel is visible
            show();
            raise();
            activateWindow();
            setVisible(true);
            setWindowState(Qt::WindowActive);
            setFocus();
            
            qDebug() << "PanelWindow: XCB-like positioning applied, panel visible:" << isVisible() << "geometry:" << geometry();
            
            // Start timer when not using layer shell
            if (m_waylandRepositionTimer && !m_waylandRepositionTimer->isActive()) {
                m_waylandRepositionTimer->start();
                qDebug() << "PanelWindow: Started Wayland reposition timer (no layer shell)";
            }
            
            // On Qt5 Wayland, also try to reserve space using struts
            // Note: Struts don't work on Wayland, but we try anyway in case of XWayland
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

    const Atom wmState     = XInternAtom(dpy, "_NET_WM_STATE", False);
    const Atom wmStateAbove= XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
    XChangeProperty(dpy, win, wmState, XA_ATOM, 32, PropModeReplace,
                    (unsigned char*)&wmStateAbove, 1);

    // On all desktops
    const Atom wmDesktop = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    const unsigned long allDesktops = 0xFFFFFFFFul;
    XChangeProperty(dpy, win, wmDesktop, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char*)&allDesktops, 1);

    XSync(dpy, False);

    // Now that we're mapped, do a final assert of position + struts,
    // and start listening for root changes (workarea/desktop geometry).
    setupX11RootEventListener();

    // Pass 1: right after we're mapped
    QTimer::singleShot(0, this, [this]{
        updateLayout();
        updatePosition();
        scheduleApplyStruts();
    });
}
 
void PanelWindow::mousePressEvent(QMouseEvent* e)  { e->accept(); QWidget::mousePressEvent(e); }
void PanelWindow::mouseReleaseEvent(QMouseEvent* e){ e->accept(); QWidget::mouseReleaseEvent(e); }
 
void PanelWindow::resizeEvent(QResizeEvent* ev)
{
    qDebug() << "PanelWindow::resizeEvent - new size:" << ev->size()
             << "isVisible:" << isVisible() << "isHidden:" << isHidden();

    // Prevent the panel from being resized to an incorrect height
    if (ev->size().height() != 48) {
        QTimer::singleShot(0, [this]() {
            if (height() != 48) {
                resize(width(), 48);
            }
        });
    }

    // Keep the view in sync
    m_view->resize(ev->size());
    m_view->setSceneRect(QRectF(QPointF(0,0), QSizeF(ev->size())));

    // Only update layout, not position - position should be stable
    updateLayout();
    
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

    // Vertical - support both old string format and new index format
    QVariant vposVariant = Settings::value(m_id, "verticalPosition", 1);
#if QT_VERSION >= 0x060000
    bool vposIsString = (vposVariant.metaType().id() == QMetaType::QString);
#else
    bool vposIsString = (vposVariant.type() == QVariant::String);
#endif
    if (vposIsString) {
        // String format: "Top", "Bottom", or legacy numeric strings
        const QString vpos = vposVariant.toString();
        if (vpos == "Top") {
            m_verticalAnchor = Min;
        } else if (vpos == "Bottom") {
            m_verticalAnchor = Max;
        } else {
            // Handle legacy string representation of numbers (e.g., "0", "1")
            bool ok;
            int vposIndex = vpos.toInt(&ok);
            if (ok) {
                m_verticalAnchor = (vposIndex == 0) ? Min : Max;
                // Migrate legacy numeric strings to descriptive strings
                QString positionString = (m_verticalAnchor == Min) ? "Top" : "Bottom";
                Settings::setValue(m_id, "verticalPosition", positionString);
            } else {
                m_verticalAnchor = Max; // Default to bottom
            }
        }
    } else {
        // Legacy integer format: 0=Top, 1=Bottom
        int vposIndex = vposVariant.toInt();
        m_verticalAnchor = (vposIndex == 0) ? Min : Max;
        // Migrate legacy integer format to descriptive strings
        QString positionString = (m_verticalAnchor == Min) ? "Top" : "Bottom";
        Settings::setValue(m_id, "verticalPosition", positionString);
    }

    // Horizontal - support both old string format and new index format
    QVariant hposVariant = Settings::value(m_id, "horizontalPosition", 1);
#if QT_VERSION >= 0x060000
    bool hposIsString = (hposVariant.metaType().id() == QMetaType::QString);
#else
    bool hposIsString = (hposVariant.type() == QVariant::String);
#endif
    if (hposIsString) {
        // Old string format (for backward compatibility)
        const QString hpos = hposVariant.toString();
        if      (hpos == "Left")  m_horizontalAnchor = Min;
        else if (hpos == "Right") m_horizontalAnchor = Max;
        else                      m_horizontalAnchor = Center;
        // Migrate to new format
        int hposIndex = (hpos == "Left") ? 0 : (hpos == "Center") ? 1 : 2;
        Settings::setValue(m_id, "horizontalPosition", hposIndex);
    } else {
        // New index format: 0=Left, 1=Center, 2=Right
        int hposIndex = hposVariant.toInt();
        if      (hposIndex == 0) m_horizontalAnchor = Min;
        else if (hposIndex == 2) m_horizontalAnchor = Max;
        else                     m_horizontalAnchor = Center;
    }

    m_appletnames = Settings::value(m_id, "applets", QStringList()).toStringList();
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
    for (Applet* applet : m_applets) {
        if (applet) {
            applet->close();
        }
    }

    // Then delete them
    while (!m_applets.isEmpty()) {
        if (Applet* a = m_applets.takeLast()) {
            a->close();
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

void PanelWindow::setHorizontalAnchor(Anchor a)
{
    if (m_horizontalAnchor == a) return;
    m_horizontalAnchor = a;
    updatePosition();
    scheduleApplyStruts();
}

void PanelWindow::setVerticalAnchor(Anchor a)
{
    if (m_verticalAnchor == a) return;
    m_verticalAnchor = a;
    updatePosition();
    scheduleApplyStruts();
    
    // Update Wayland layer shell configuration if using layer shell
    updateWaylandLayerShellConfiguration();
}

void PanelWindow::updateWaylandLayerShellConfiguration()
{
    qDebug() << "PanelWindow::updateWaylandLayerShellConfiguration() - Updating layer shell for vertical anchor:" << m_verticalAnchor;
    
    // Determine layer and anchor based on vertical position
    int layer;
    int anchorMask;
    
    if (m_verticalAnchor == Min) {
        // Top position
        layer = 2; // LayerTop
        anchorMask = 1 | 4 | 8; // AnchorTop | AnchorLeft | AnchorRight
    } else {
        // Bottom position (default)
        layer = 1; // LayerBottom
        anchorMask = 2 | 4 | 8; // AnchorBottom | AnchorLeft | AnchorRight
    }
    
    // Update LayerShellQt configuration if available
    if (m_layerShellQt && m_layerShellQt->isAvailable()) {
        qDebug() << "PanelWindow::updateWaylandLayerShellConfiguration() - Updating LayerShellQt";
        m_layerShellQt->configureWindow(windowHandle(), layer, anchorMask, 48, QMargins(0, 0, 0, 0), 0);
    }
    
    // Update custom Wayland layer shell configuration if available
    if (m_waylandLayerShell && m_waylandLayerShell->isAvailable()) {
        qDebug() << "PanelWindow::updateWaylandLayerShellConfiguration() - Updating custom Wayland layer shell";
        
        // Note: Layer cannot be changed after surface creation in Wayland layer shell protocol
        // We can only update the anchor mask, which should be sufficient for positioning
        uint32_t waylandAnchorMask = (m_verticalAnchor == Min) ?
            (static_cast<uint32_t>(WaylandLayerShell::Anchor::Top) | 
             static_cast<uint32_t>(WaylandLayerShell::Anchor::Left) | 
             static_cast<uint32_t>(WaylandLayerShell::Anchor::Right)) :
            (static_cast<uint32_t>(WaylandLayerShell::Anchor::Bottom) | 
             static_cast<uint32_t>(WaylandLayerShell::Anchor::Left) | 
             static_cast<uint32_t>(WaylandLayerShell::Anchor::Right));
        
        m_waylandLayerShell->setAnchorMask(waylandAnchorMask);
        m_waylandLayerShell->commit();
        
        qDebug() << "PanelWindow::updateWaylandLayerShellConfiguration() - Updated anchor mask:" << waylandAnchorMask;
    }
    
    qDebug() << "PanelWindow::updateWaylandLayerShellConfiguration() - Updated layer:" << layer 
             << "anchorMask:" << anchorMask;
}
 
void PanelWindow::setOrientation(Orientation o) { m_orientation = o; }
void PanelWindow::setLayoutPolicy(LayoutPolicy p)
{
    if (m_layoutPolicy == p) return;
    m_layoutPolicy = p;
    updateLayout();
    updatePosition();
    scheduleApplyStruts();
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
    qDebug() << "PanelWindow::updateLayout - size:" << size() << "policy:" << m_layoutPolicy;

    if (m_layoutPolicy == FillSpace && m_orientation == Horizontal) {
        const int w = currentScreenGeometry().width();
        if (width() != w) resize(w, height());
    } else if (m_layoutPolicy == AutoSize) {
        // Sum fixed applet widths + spacers (very similar to your original code)
        const int spacing = 4;
        int desired = 0;
        int spacers = 0;
        for (Applet* a : m_applets) {
            const int w = a->desiredSize().width();
            if (w >= 0) desired += w; else ++spacers;
        }
        if (!m_applets.isEmpty()) desired += spacing * (m_applets.size()-1);
        desired = qMax(desired, minimumWidth());
        if (width() != desired) resize(desired, height());
    }

    // Layout applets horizontally
    const int spacing = 4;
    int freeSpace = width() - spacing * (m_applets.size() - 1);
    int spacers = 0;
    for (Applet* a : m_applets) {
        const int w = a->desiredSize().width();
        if (w >= 0) freeSpace -= w; else ++spacers;
    }
    const int perSpacer = (spacers > 0) ? (freeSpace / spacers) : 0;

    int x = 0;
    int remainingSpacers = spacers;
    for (Applet* a : m_applets) {
        QSize sz = a->desiredSize();
        if (sz.width() < 0) {
            if (remainingSpacers > 1) { sz.setWidth(perSpacer); freeSpace -= perSpacer; --remainingSpacers; }
            else { sz.setWidth(freeSpace); freeSpace = 0; --remainingSpacers; }
        }
        sz.setHeight(height());
        a->setPosition(QPoint(x,0));
        a->setSize(sz);
        x += sz.width() + spacing;
    }
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
        qDebug() << "PanelWindow::detectGnomeTopOffsetPx: no X11 display available";
        return 0;
    }

    // This function already scans for GNOME Shell / Top Bar windows.
    // If it finds nothing, we return 0 (NO guessed fallback).
    const int h = X11Support::detectTopPanelHeight(dpy);
    qDebug() << "PanelWindow::detectGnomeTopOffsetPx (X11): detected height=" << h
             << "isX11=" << isX11 << "openedDisplay=" << openedDisplay;
    
    // Close display if we opened it ourselves (don't close Qt's display)
    if (openedDisplay && dpy) {
        XCloseDisplay(dpy);
    }
    
    if (h <= 0 || h > 128) {
        qDebug() << "PanelWindow::detectGnomeTopOffsetPx: returning 0 (invalid height)";
        return 0;
    }

    return h;
}

 void PanelWindow::updatePosition()
 {
    qDebug() << "PanelWindow::updatePosition ENTER"
    << "winId=" << winId()
    << "visible=" << isVisible()
    << "anchorV=" << m_verticalAnchor
    << "anchorH=" << m_horizontalAnchor
    << "geom(before)=" << geometry();
    // Layer-shell handles positioning itself
     if ((m_layerShellQt && m_layerShellQt->isAvailable()) ||
         (m_waylandLayerShell && m_waylandLayerShell->isAvailable())) {
         return;
     }
 
     if (winId() == 0 || width() <= 0 || height() <= 0)
         return;
 
     const QRect screen = currentScreenGeometry();
 
 #if QT_VERSION < 0x060000
     const bool isX11 = QX11Info::isPlatformX11();
 #else
     const bool isX11 = qApp->platformName().toLower().contains("xcb");
 #endif
 
     // ------------------------------------------------------------------
     // 1) External struts ONLY (never mix with our own panels)
     // ------------------------------------------------------------------
     int extLeft = 0, extRight = 0, extTop = 0, extBottom = 0;
     if (isX11) {
         const QMargins ext = X11Support::getExternalStrutReservations(screen, winId());
         extLeft   = ext.left();
         extRight  = ext.right();
         extTop    = ext.top();
         extBottom = ext.bottom();
     }
 
    // GNOME fallback (only if not already reserved)
    int gnomeTop = detectGnomeTopOffsetPx();
    extTop = qMax(extTop, gnomeTop);
 
    // ------------------------------------------------------------------
    // 2) Stack OUR panels relative to the usable area (screen minus external)
    // ------------------------------------------------------------------
    int ownTopStack = 0;
    int ownBottomStack = 0;

    if (isX11) {
        const QRect usable = screen.adjusted(extLeft, extTop, -extRight, -extBottom);

        if (m_verticalAnchor == Min) {
            ownTopStack = X11Support::getHdepanelPanelsHeight(
                usable, winId(), true, false
            );
        } else if (m_verticalAnchor == Max) {
            ownBottomStack = X11Support::getHdepanelPanelsHeight(
                usable, winId(), false, true
            );
        }
    }

 
     // ------------------------------------------------------------------
     // 3) Horizontal placement
     // ------------------------------------------------------------------
     int x = screen.left();
     switch (m_horizontalAnchor) {
         case Min:    x = screen.left() + extLeft; break;
         case Center: x = screen.left() + (screen.width() - width()) / 2; break;
         case Max:    x = screen.right() - extRight - width() + 1; break;
     }
 
     // ------------------------------------------------------------------
     // 4) Vertical placement (CORRECT stacking)
     // ------------------------------------------------------------------
     int y = screen.top();
     if (m_verticalAnchor == Min) {
         y = screen.top() + extTop + ownTopStack;
     } else if (m_verticalAnchor == Max) {
         y = screen.bottom() - extBottom - ownBottomStack - height() + 1;
     } else {
         y = screen.top() + (screen.height() - height()) / 2;
     }
 
     // Full-width panels
     if (m_layoutPolicy == FillSpace && m_orientation == Horizontal) {
         if (width() != screen.width())
             resize(screen.width(), height());
     }
 
    const QRect newGeom(x, y, width(), height());
    if (geometry() != newGeom) {
       setGeometry(newGeom);
       scheduleApplyStruts();
    } else {
       qDebug() << "PanelWindow::updatePosition SKIPPED (geometry unchanged)";
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

    if (m_verticalAnchor == Min) {
        // Absolute distance from screen top to *this panel’s bottom*
        top = panelGeom.bottom() - screen.top() + 1;
        if (top < 0) top = 0;
    }
    else if (m_verticalAnchor == Max) {
        // Absolute distance from *this panel’s top* to screen bottom
        bottom = screen.bottom() - panelGeom.top() + 1;
        if (bottom < 0) bottom = 0;
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

    // For Wayland, always use full width
    int x = screen.left();
    int y = 0;
    int panelHeight = 48; // Standard panel height
    if (m_verticalAnchor == Min)      y = anchor.top();
    else if (m_verticalAnchor == Max) y = anchor.bottom() - panelHeight + 1;
    else                              y = anchor.top() + (anchor.height() - panelHeight)/2;
    
    // Set panel size to full screen width with proper panel height
    int panelWidth = screen.width();

    qDebug() << "PanelWindow::forceWaylandPosition - screen:" << screen << "available:" << available 
             << "anchor:" << anchor << "verticalAnchor:" << m_verticalAnchor 
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
    return (height() - m.height())/2 + m.ascent();
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
