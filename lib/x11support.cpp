/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * Copyright: 2015-2025 Haydar Alkaduhimi
 * Copyright (C) 2014 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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

#include <QDebug>
#include <unistd.h>

#include "x11support.h"

#include <cstdint>

// TODO: Keep all the X11 stuff with scary defines below normal headers.
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>

#include <X11/extensions/Xrender.h>

#include <QtCore/QTimer>

static XErrorHandler oldX11ErrorHandler = NULL;

// Qt5/Qt6 compatibility helpers for X11 access
static Display* x11DisplayCompat()
{
#if QT_VERSION < 0x060000
    return QX11Info::display();
#else
    static Display* dpy = XOpenDisplay(nullptr);
    return dpy;
#endif
}

static int x11AppScreenCompat()
{
#if QT_VERSION < 0x060000
    return QX11Info::appScreen();
#else
    return DefaultScreen(x11DisplayCompat());
#endif
}

static Window x11RootWindowCompat()
{
#if QT_VERSION < 0x060000
    return QX11Info::appRootWindow();
#else
    return DefaultRootWindow(x11DisplayCompat());
#endif
}

static int x11errorHandler(Display* display, XErrorEvent* error)
{
	if (error->error_code == BadWindow) return 0;

	return (*oldX11ErrorHandler)(display, error);
}

int X11Support::detectTopPanelHeight(Display *dpy)
{
    if (!dpy)
        return 0;

    Window root = DefaultRootWindow(dpy);
    Window root_return, parent_return;
    Window *children;
    unsigned int nchildren;

    if (!XQueryTree(dpy, root, &root_return, &parent_return, &children, &nchildren))
        return 0;

    int topPanelHeight = 0;

    for (unsigned int i = 0; i < nchildren; ++i) {
        XTextProperty wmName;
        if (XGetWMName(dpy, children[i], &wmName) && wmName.value) {
            QString name = QString::fromUtf8(reinterpret_cast<char *>(wmName.value));
            if (name.contains("Top Bar", Qt::CaseInsensitive) ||
                name.contains("ubuntu-panel", Qt::CaseInsensitive) ||
                name.contains("gnome-shell", Qt::CaseInsensitive)) {

                XWindowAttributes attr;
                if (XGetWindowAttributes(dpy, children[i], &attr)) {
                    topPanelHeight = attr.height;
                    qDebug() << "Detected GNOME top panel window:" << name
                             << "height=" << topPanelHeight;
                }
                XFree(wmName.value);
                break;
            }
            XFree(wmName.value);
        }
    }

    if (children)
        XFree(children);

    return topPanelHeight;
}

X11Support* X11Support::m_instance = NULL;

X11Support::X11Support()
{
	m_instance = this;
	oldX11ErrorHandler = XSetErrorHandler(x11errorHandler);
    m_damageEventBase = 0;
    // Guard against missing X11 display (e.g., Wayland without XWayland)
//#if QT_VERSION < 0x060000
    // If no X display is available, disable
    if (!x11DisplayCompat()) {
        qWarning() << "X11 platform not detected; disabling XDamage integration";
        return;
    }
    int damageErrorBase = 0;
    XDamageQueryExtension(x11DisplayCompat(), &m_damageEventBase, &damageErrorBase);

#if QT_VERSION >= 0x060000
    // Qt6 no longer exposes QX11Info, and our X11Support uses its own XOpenDisplay()
    // connection. To reliably receive PropertyNotify (title/icon changes), we poll and
    // dispatch events from that connection ourselves.
    if (qApp && qApp->platformName().toLower().contains("xcb")) {
        m_x11PollTimer = new QTimer(this);
        m_x11PollTimer->setInterval(20);
        connect(m_x11PollTimer, &QTimer::timeout, this, &X11Support::pollX11Events);
        m_x11PollTimer->start();
    }
#endif
}

X11Support::~X11Support()
{
	m_instance = NULL;
}

#if QT_VERSION >= 0x050000
void X11Support::onX11Event(xcb_generic_event_t *event) 
{
    switch (event->response_type & ~0x80) {
    // FIXME: there is no XDamageNotify in xcb?
    // case XCB_DAMAGE_NOTIFY: break;
    case XCB_KEYMAP_NOTIFY: {
        qDebug() << "keymap";
        break;
    }
    case XCB_DESTROY_NOTIFY: {
        xcb_destroy_notify_event_t *destroy = reinterpret_cast<xcb_destroy_notify_event_t *>(event);
        emit windowClosed(destroy->window);
        break;
    }
    case XCB_CONFIGURE_NOTIFY: {
        xcb_configure_notify_event_t *configure = reinterpret_cast<xcb_configure_notify_event_t *>(event);
        emit windowReconfigured(configure->window, configure->x, configure->y, 
            configure->width, configure->height);
        break;
    }
    case XCB_PROPERTY_NOTIFY: {
        xcb_property_notify_event_t *property = reinterpret_cast<xcb_property_notify_event_t*>(event);
        emit windowPropertyChanged(property->window, property->atom);
        break;
    }
    case XCB_CLIENT_MESSAGE: {
        xcb_client_message_event_t *client = reinterpret_cast<xcb_client_message_event_t *>(event);
        emit clientMessageReceived(client->window, client->type, client->data.data32);
        break;
    }
    default: break;
    }
}
#endif

void X11Support::onX11Event(XEvent* event)
{
    if (event->type == m_damageEventBase + XDamageNotify)
	{
		// Repair damaged area.
		XDamageNotifyEvent* damageEvent = reinterpret_cast<XDamageNotifyEvent*>(event);
    XDamageSubtract(x11DisplayCompat(), damageEvent->damage, None, None);

		emit windowDamaged(event->xany.window);
	}
	if (event->type == DestroyNotify)
		emit windowClosed(event->xdestroywindow.window);
	if (event->type == ConfigureNotify)
		emit windowReconfigured(event->xconfigure.window, event->xconfigure.x, event->xconfigure.y, event->xconfigure.width, event->xconfigure.height);
	if (event->type == PropertyNotify)
		emit windowPropertyChanged(event->xproperty.window, event->xproperty.atom);
    if (event->type == ClientMessage) {
        // Normalize payload to 5x 32-bit words to match xcb_client_message_event_t::data32.
        // TrayApplet interprets this pointer as u_int32_t*, so passing long[5] (64-bit) breaks it.
        thread_local std::uint32_t data32[5];
        for (int i = 0; i < 5; ++i) {
            data32[i] = static_cast<std::uint32_t>(event->xclient.data.l[i] & 0xFFFFFFFFu);
        }
        emit clientMessageReceived(event->xclient.window, event->xclient.message_type, data32);
    }
}

unsigned long X11Support::rootWindow()
{
    return x11RootWindowCompat();
}

unsigned long X11Support::atom(const QString& name)
{
    if(!m_instance->m_cachedAtoms.contains(name))
        m_instance->m_cachedAtoms[name] = XInternAtom(x11DisplayCompat(), name.toLatin1().data(), False);
	return m_instance->m_cachedAtoms[name];
}

void X11Support::selectInput(unsigned long window, long eventMask)
{
    if (!x11DisplayCompat() || window == 0)
        return;

    // IMPORTANT: must be on the same X11 connection as Qt (QX11Info::display() on Qt5),
    // otherwise PropertyNotify events will be delivered to a different client connection.
    long combinedMask = eventMask;
    XWindowAttributes attrs;
    if (XGetWindowAttributes(x11DisplayCompat(), static_cast<Window>(window), &attrs)) {
        combinedMask |= attrs.your_event_mask;
    }
    XSelectInput(x11DisplayCompat(), static_cast<Window>(window), combinedMask);
    XFlush(x11DisplayCompat());
}

void X11Support::pollX11Events()
{
#if QT_VERSION >= 0x060000
    if (!x11DisplayCompat())
        return;

    while (XPending(x11DisplayCompat()) > 0) {
        XEvent ev;
        XNextEvent(x11DisplayCompat(), &ev);
        onX11Event(&ev);
    }
#endif
}

void X11Support::removeWindowProperty(unsigned long window, const QString& name)
{
    XDeleteProperty(x11DisplayCompat(), window, atom(name));
}

void X11Support::setWindowPropertyCardinalArray(unsigned long window, const QString& name, const QVector<unsigned long>& values)
{
    XChangeProperty(x11DisplayCompat(), window, atom(name), XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(values.data()), values.size());
}

// ---- ADD: helper struct & functions (in x11support.cpp, top-level) ----
struct X11Strut {
    int left = 0, right = 0, top = 0, bottom = 0;
    int leftStartY = 0, leftEndY = 0, rightStartY = 0, rightEndY = 0;
    int topStartX = 0, topEndX = 0, bottomStartX = 0, bottomEndX = 0;
    bool valid = false;
};

static bool x11IsViewable(Window w) {
    XWindowAttributes a;
    if (!x11DisplayCompat()) return false;
    if (!XGetWindowAttributes(x11DisplayCompat(), w, &a)) return false;
    return a.map_state == IsViewable;
}

static bool x11IsDock(Window w) {
    if (!x11DisplayCompat()) return false;
    Atom typeAtom = X11Support::atom("_NET_WM_WINDOW_TYPE");
    Atom dockAtom = X11Support::atom("_NET_WM_WINDOW_TYPE_DOCK");
    Atom panelAtom = X11Support::atom("_NET_WM_WINDOW_TYPE_PANEL");

    Atom actualType; int actualFormat; unsigned long nitems, bytesAfter;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(x11DisplayCompat(), w, typeAtom, 0, 1024, False, XA_ATOM,
                           &actualType, &actualFormat, &nitems, &bytesAfter, &data) != Success) {
        return false;
    }
    bool isDock = false;
    if (data && actualType == XA_ATOM && actualFormat == 32) {
        Atom* atoms = reinterpret_cast<Atom*>(data);
        for (unsigned long i = 0; i < nitems; ++i) {
            if (atoms[i] == dockAtom || atoms[i] == panelAtom) { isDock = true; break; }
        }
    }
    if (data) XFree(data);
    return isDock;
}

static bool x11GetRootRect(Window w, QRect& out)
{
    if (!x11DisplayCompat()) return false;

    XWindowAttributes a;
    if (!XGetWindowAttributes(x11DisplayCompat(), w, &a)) return false;

    Window child = None;
    int rx = 0, ry = 0;

    if (!XTranslateCoordinates(x11DisplayCompat(),
                               w,
                               x11RootWindowCompat(),
                               0, 0,
                               &rx, &ry,
                               &child)) {
        return false;
    }

    out = QRect(rx, ry, a.width, a.height);
    return true;
}

static X11Strut x11GetStrut(Window w) {
    X11Strut s; s.valid = false;
    if (!x11DisplayCompat()) return s;

    auto readCardinals = [&](Atom prop, QVector<unsigned long>& out)->bool {
        Atom actualType; int actualFormat; unsigned long nitems, bytesAfter;
        unsigned char* data = nullptr;
        if (XGetWindowProperty(x11DisplayCompat(), w, prop, 0, 12, False, XA_CARDINAL,
                               &actualType, &actualFormat, &nitems, &bytesAfter, &data) != Success) {
            return false;
        }
        bool ok = (data && actualType == XA_CARDINAL && actualFormat == 32 && nitems >= 4);
        if (ok) {
            out.resize((int)nitems);
            memcpy(out.data(), data, nitems * sizeof(unsigned long));
        }
        if (data) XFree(data);
        return ok;
    };

    QVector<unsigned long> arr;
    // Prefer _NET_WM_STRUT_PARTIAL (12 values); fall back to _NET_WM_STRUT (4 values)
    if (readCardinals(X11Support::atom("_NET_WM_STRUT_PARTIAL"), arr) && arr.size() >= 12) {
        s.left   = (int)arr[0];  s.right  = (int)arr[1];  s.top    = (int)arr[2];  s.bottom = (int)arr[3];
        s.leftStartY   = (int)arr[4];  s.leftEndY   = (int)arr[5];
        s.rightStartY  = (int)arr[6];  s.rightEndY  = (int)arr[7];
        s.topStartX    = (int)arr[8];  s.topEndX    = (int)arr[9];
        s.bottomStartX = (int)arr[10]; s.bottomEndX = (int)arr[11];
        s.valid = (s.left || s.right || s.top || s.bottom);
    } else if (readCardinals(X11Support::atom("_NET_WM_STRUT"), arr) && arr.size() >= 4) {
        s.left   = (int)arr[0];  s.right  = (int)arr[1];  s.top    = (int)arr[2];  s.bottom = (int)arr[3];
        s.valid = (s.left || s.right || s.top || s.bottom);
    }
    return s;
}

static QVector<Window> x11ClientList() {
    QVector<Window> out;
    if (!x11DisplayCompat()) return out;
    Atom prop = X11Support::atom("_NET_CLIENT_LIST");
    Atom actualType; int actualFormat; unsigned long nitems, bytesAfter;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(x11DisplayCompat(), X11Support::rootWindow(), prop, 0, 0x7FFFFFFF, False, XA_WINDOW,
                           &actualType, &actualFormat, &nitems, &bytesAfter, &data) == Success) {
        if (data && actualType == XA_WINDOW && actualFormat == 32) {
            Window* wins = reinterpret_cast<Window*>(data);
            for (unsigned long i = 0; i < nitems; ++i) out.append(wins[i]);
        }
        if (data) XFree(data);
    }
    return out;
}

QMargins X11Support::getExternalStrutReservations(const QRect& screen, unsigned long excludeWindow)
{
    QMargins m(0, 0, 0, 0);
    if (!x11DisplayCompat() || !screen.isValid())
        return m;

    const int sLeft = screen.left();
    const int sRight = screen.right();
    const int sTop = screen.top();
    const int sBottom = screen.bottom();
    Q_UNUSED(sTop)
    Q_UNUSED(sBottom)

    auto overlaps1D = [](int a1, int a2, int b1, int b2) -> bool {
        if (a1 > a2) std::swap(a1, a2);
        if (b1 > b2) std::swap(b1, b2);
        return !(a2 < b1 || b2 < a1);
    };

    const QVector<Window> wins = x11ClientList();
    for (Window w : wins) {
        if (!w) continue;
        if (excludeWindow && w == static_cast<Window>(excludeWindow)) continue;
        if (!x11IsViewable(w)) continue;

        // Only consider dock/panel windows to avoid random windows with bogus properties.
        if (!x11IsDock(w)) continue;

        // Exclude our own hdepanel windows - they set their own struts independently
        // Check window title to identify hdepanel windows
        QString windowName = getWindowName(w);
        if (windowName.contains("HDE Panel", Qt::CaseInsensitive) || 
            windowName.contains("hdepanel", Qt::CaseInsensitive)) {
            continue; // Skip our own panels
        }

        const X11Strut s = x11GetStrut(w);
        if (!s.valid) continue;

        // Top / bottom: respect strut partial range if present; otherwise assume it applies globally.
        if (s.top > 0) {
            const bool hasRange = (s.topStartX != 0 || s.topEndX != 0);
            const bool overlaps = hasRange ? overlaps1D(s.topStartX, s.topEndX, sLeft, sRight) : true;
            if (overlaps) m.setTop(qMax(m.top(), s.top));
        }
        if (s.bottom > 0) {
            const bool hasRange = (s.bottomStartX != 0 || s.bottomEndX != 0);
            const bool overlaps = hasRange ? overlaps1D(s.bottomStartX, s.bottomEndX, sLeft, sRight) : true;
            if (overlaps) m.setBottom(qMax(m.bottom(), s.bottom));
        }

        // Left / right: optional; use partial Y range if present.
        if (s.left > 0) {
            const bool hasRange = (s.leftStartY != 0 || s.leftEndY != 0);
            const bool overlaps = hasRange ? overlaps1D(s.leftStartY, s.leftEndY, screen.top(), screen.bottom()) : true;
            if (overlaps) m.setLeft(qMax(m.left(), s.left));
        }
        if (s.right > 0) {
            const bool hasRange = (s.rightStartY != 0 || s.rightEndY != 0);
            const bool overlaps = hasRange ? overlaps1D(s.rightStartY, s.rightEndY, screen.top(), screen.bottom()) : true;
            if (overlaps) m.setRight(qMax(m.right(), s.right));
        }
    }

    return m;
}

int X11Support::getMaxHdepanelStrut(const QRect& screen, unsigned long excludeWindow, bool isTop, bool isBottom)
{
    int maxStrut = 0;
    if (!x11DisplayCompat() || !screen.isValid())
        return maxStrut;

    const QVector<Window> wins = x11ClientList();
    for (Window w : wins) {
        if (!w) continue;
        if (excludeWindow && w == static_cast<Window>(excludeWindow)) continue;
        if (!x11IsViewable(w)) continue;
        if (!x11IsDock(w)) continue;

        // Only consider hdepanel windows
        QString windowName = getWindowName(w);
        if (!windowName.contains("HDE Panel", Qt::CaseInsensitive) && 
            !windowName.contains("hdepanel", Qt::CaseInsensitive)) {
            continue; // Skip non-hdepanel windows
        }

        // Get window geometry to determine position and calculate strut
        XWindowAttributes attrs;
        if (!XGetWindowAttributes(x11DisplayCompat(), w, &attrs)) continue;
        
        // Check if this window is at the same position (top or bottom)
        // For top: consider panels in the upper half of the screen
        // For bottom: consider panels in the lower half of the screen
        int screenCenterY = screen.top() + screen.height() / 2;
        bool windowIsTop = (attrs.y + attrs.height / 2) < screenCenterY;
        bool windowIsBottom = (attrs.y + attrs.height / 2) >= screenCenterY;

        if (isTop && windowIsTop) {
            // Calculate what the strut should be based on window geometry
            // Strut is distance from screen top to bottom of panel
            int calculatedStrut = (attrs.y + attrs.height - screen.top()) + 1;
            if (calculatedStrut > 0) {
                maxStrut = qMax(maxStrut, calculatedStrut);
            }
        } else if (isBottom && windowIsBottom) {
            // Calculate what the strut should be based on window geometry
            // Strut is distance from top of panel to screen bottom
            int calculatedStrut = (screen.bottom() - attrs.y) + 1;
            if (calculatedStrut > 0) {
                maxStrut = qMax(maxStrut, calculatedStrut);
            }
        }
    }

    return maxStrut;
}

int X11Support::getHdepanelPanelsHeight(
    const QRect& usable,
    unsigned long selfWindow,
    bool isTop,
    bool isBottom)
{
    if (!x11DisplayCompat() || !usable.isValid() || selfWindow == 0)
        return 0;

    const int kEdgeSlackPx = 8; // small tolerance

    struct Entry {
        Window w;
        QRect  r;
    };
    QVector<Entry> panels;

    const QVector<Window> wins = x11ClientList();
    for (Window w : wins) {
        if (!w) continue;
        if (!x11IsViewable(w)) continue;
        if (!x11IsDock(w)) continue;

        const QString name = getWindowName(w);
        if (!name.contains("HDE Panel", Qt::CaseInsensitive) &&
            !name.contains("hdepanel", Qt::CaseInsensitive))
            continue;

        QRect wr;
        if (!x11GetRootRect(w, wr))
            continue;

        // Must be on the same screen area (avoid other monitors)
        if (wr.right() < usable.left() || wr.left() > usable.right())
            continue;

        // Decide if this panel belongs to the requested edge.
        // TOP: accept windows at/above usable.top()+slack (includes startup y=0).
        // BOTTOM: accept windows at/under usable.bottom()-slack.
        bool okEdge = false;
        if (isTop) {
            if (wr.top() <= usable.top() + kEdgeSlackPx)
                okEdge = true;
        } else if (isBottom) {
            if (wr.bottom() >= usable.bottom() - kEdgeSlackPx)
                okEdge = true;
        } else {
            // center or unknown: don't stack
            return 0;
        }

        if (!okEdge) continue;

        panels.push_back({w, wr});
    }

    // Stable order: sort by window id. (If you want user-defined order later,
    // we can sort by a property or by your panel id.)
    std::sort(panels.begin(), panels.end(),
              [](const Entry& a, const Entry& b) { return a.w < b.w; });

    // Offset = sum heights of panels BEFORE selfWindow
    int offset = 0;
    const Window self = static_cast<Window>(selfWindow);

    for (const auto& e : panels) {
        if (e.w == self)
            break;
        offset += e.r.height();
    }

    return offset;
}

void X11Support::setStrut(Window _wid,
                       int left, int right,
                       int top,  int bottom,

                       int leftStartY,   int leftEndY,
                       int rightStartY,  int rightEndY,
                       int topStartX,    int topEndX,
                       int bottomStartX, int bottomEndX
                       )
{
    unsigned long desstrut[12];
    memset(desstrut,0,sizeof(desstrut));

    // Set up EWMH strut properties according to specification
    // Order: left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x, bottom_end_x
    desstrut[0] = left; desstrut[1] = right;
    desstrut[2] = top;  desstrut[3] = bottom;

    desstrut[4] = leftStartY;    desstrut[5] = leftEndY;
    desstrut[6] = rightStartY;   desstrut[7] = rightEndY;
    desstrut[8] = topStartX;     desstrut[9] = topEndX;
    desstrut[10] = bottomStartX; desstrut[11] = bottomEndX;

    //now we can change that property right
    Display* display = x11DisplayCompat();
    if (!display) {
        qWarning() << "X11Support::setStrut - No X11 display available";
        return;
    }
    
    // Check if window is mapped
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, _wid, &attrs) == 0) {
        qWarning() << "X11Support::setStrut - Cannot get window attributes for window" << _wid;
        return;
    }
    
    if (attrs.map_state != IsViewable) {
        qWarning() << "X11Support::setStrut - Window" << _wid << "is not mapped (map_state:" << attrs.map_state << ")";
        qWarning() << "X11Support::setStrut - Attempting to map window first...";
        
        // Try to map the window first
        XMapWindow(display, _wid);
        XFlush(display);
        
        // Wait a bit and check again
        usleep(10000); // 10ms
        if (XGetWindowAttributes(display, _wid, &attrs) != 0) {
            if (attrs.map_state == IsViewable) {
            } else {
                qWarning() << "X11Support::setStrut - Window still not mapped after attempt";
            }
        }
    }
    
    Atom strutPartialAtom = X11Support::atom("_NET_WM_STRUT_PARTIAL");
    Atom strutAtom = X11Support::atom("_NET_WM_STRUT");
    
    int result1 = XChangeProperty(display, _wid, strutPartialAtom,
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char *) desstrut, 12);
    int result2 = XChangeProperty(display, _wid, strutAtom,
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char*) desstrut, 4);

    // Flush to ensure the properties are sent to the X server
    XFlush(display);
}

int X11Support::getHdepanelStackOffsetPx(const QRect& usable, unsigned long selfWindow, HdeEdge edge)
{
    if (!x11DisplayCompat() || !usable.isValid() || selfWindow == 0)
        return 0;

    constexpr int kSlack = 8;

    struct Entry { Window w; QRect r; };
    QVector<Entry> panels;

    const QVector<Window> wins = x11ClientList();
    for (Window w : wins) {
        if (!w) continue;
        if (!x11IsViewable(w)) continue;
        if (!x11IsDock(w)) continue;

        const QString name = getWindowName(w);
        if (!name.contains("HDE Panel", Qt::CaseInsensitive) &&
            !name.contains("hdepanel", Qt::CaseInsensitive))
            continue;

        QRect wr;
        if (!x11GetRootRect(w, wr)) continue;

        // Must intersect usable area on the perpendicular axis
        if (edge == HdeEdge::Top || edge == HdeEdge::Bottom) {
            if (wr.right() < usable.left() || wr.left() > usable.right()) continue;
        } else {
            if (wr.bottom() < usable.top() || wr.top() > usable.bottom()) continue;
        }

        bool ok = false;
        switch (edge) {
            case HdeEdge::Top:    ok = (wr.top() <= usable.top() + kSlack); break;
            case HdeEdge::Bottom: ok = (wr.bottom() >= usable.bottom() - kSlack); break;
            case HdeEdge::Left:   ok = (wr.left() <= usable.left() + kSlack); break;
            case HdeEdge::Right:  ok = (wr.right() >= usable.right() - kSlack); break;
        }
        if (!ok) continue;

        panels.push_back({w, wr});
    }

    auto byId = [](const Entry& a, const Entry& b){ return a.w < b.w; };

    // Stable + sensible ordering based on current geometry, with id tiebreak
    switch (edge) {
        case HdeEdge::Top:
            std::sort(panels.begin(), panels.end(), [&](const Entry& a, const Entry& b){
                if (a.r.top() != b.r.top()) return a.r.top() < b.r.top();
                return byId(a,b);
            });
            break;
        case HdeEdge::Bottom:
            std::sort(panels.begin(), panels.end(), [&](const Entry& a, const Entry& b){
                if (a.r.bottom() != b.r.bottom()) return a.r.bottom() > b.r.bottom();
                return byId(a,b);
            });
            break;
        case HdeEdge::Left:
            std::sort(panels.begin(), panels.end(), [&](const Entry& a, const Entry& b){
                if (a.r.left() != b.r.left()) return a.r.left() < b.r.left();
                return byId(a,b);
            });
            break;
        case HdeEdge::Right:
            std::sort(panels.begin(), panels.end(), [&](const Entry& a, const Entry& b){
                if (a.r.right() != b.r.right()) return a.r.right() > b.r.right();
                return byId(a,b);
            });
            break;
    }

    const Window self = static_cast<Window>(selfWindow);

    int offset = 0;
    for (const auto& e : panels) {
        if (e.w == self) break;
        offset += (edge == HdeEdge::Left || edge == HdeEdge::Right) ? e.r.width()
                                                                    : e.r.height();
    }
    return offset;
}


void X11Support::setWindowPropertyCardinal(unsigned long window, const QString& name, unsigned long value)
{
    XChangeProperty(x11DisplayCompat(), window, atom(name), XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(&value), 1);
}

void X11Support::setWindowPropertyVisualId(unsigned long window, const QString& name, unsigned long value)
{
    XChangeProperty(x11DisplayCompat(), window, atom(name), XA_VISUALID, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(&value), 1);
}

template<class T>
static bool getWindowPropertyHelper(unsigned long window, unsigned long atom, unsigned long type, int& numItems, T*& data)
{
	Atom retType;
	int retFormat;
	unsigned long numItemsTemp;
	unsigned long bytesLeft;
    if(XGetWindowProperty(x11DisplayCompat(), window, atom, 0, 0x7FFFFFFF, False, type, &retType, &retFormat, &numItemsTemp, &bytesLeft, reinterpret_cast<unsigned char**>(&data)) != Success)
		return false;
	numItems = numItemsTemp;
	if(numItems == 0)
		return false;
	return true;
}

unsigned long X11Support::getWindowPropertyCardinal(unsigned long window, const QString& name)
{
	int numItems;
	unsigned long* data;
	unsigned long value = 0;
	if(!getWindowPropertyHelper(window, atom(name), XA_CARDINAL, numItems, data))
		return value;
	value = data[0];
	XFree(data);
	return value;
}
bool X11Support::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type()) {

    case QEvent::KeyboardLayoutChange :
        qDebug() << "KeyboardLayoutChange";
        //QTimer::singleShot(10,this,SLOT(keyChanged()));
        break;


    default:
        break;
    }
    return QObject::eventFilter(obj, event);
}

// FIXME: it failed to get activateWindow for Qt5
unsigned long X11Support::getWindowPropertyWindow(unsigned long window, const QString& name)
{
	int numItems;
	unsigned long *data;
	unsigned long value = 0;
	if (!getWindowPropertyHelper(window, atom(name), XA_WINDOW, numItems, data))
		return value;
	value = data[0];
	XFree(data);
	return value;
}

QVector<unsigned long> X11Support::getWindowPropertyCardinalArray(unsigned long window, const QString& name)
{
    int numItems;
    unsigned long* data;
    QVector<unsigned long> values;
    if (!getWindowPropertyHelper(window, atom(name), XA_CARDINAL, numItems, data))
        return values;
    values.reserve(numItems);
    for (int i = 0; i < numItems; ++i)
        values.append(data[i]);
    XFree(data);
    return values;
}

QVector<unsigned long> X11Support::getWindowPropertyWindowsArray(unsigned long window, const QString& name)
{
	int numItems;
	unsigned long* data;
	QVector<unsigned long> values;
	if(!getWindowPropertyHelper(window, atom(name), XA_WINDOW, numItems, data))
		return values;
	for(int i = 0; i < numItems; i++)
		values.append(data[i]);
	XFree(data);
	return values;
}

QVector<unsigned long> X11Support::getWindowPropertyAtomsArray(unsigned long window, const QString& name)
{
	int numItems;
	unsigned long* data;
	QVector<unsigned long> values;
	if(!getWindowPropertyHelper(window, atom(name), XA_ATOM, numItems, data))
		return values;
	for(int i = 0; i < numItems; i++)
		values.append(data[i]);
	XFree(data);
	return values;
}

QVector<unsigned long> X11Support::getAllWindows()
{
	QVector<unsigned long> windows;
	Display* dpy = x11DisplayCompat();
	if (!dpy) return windows;
	
	Window root = DefaultRootWindow(dpy);
	
	// Recursively search for windows with names
	getAllWindowsRecursive(dpy, root, windows);
	
	return windows;
}

void X11Support::getAllWindowsRecursive(void* dpy, unsigned long window, QVector<unsigned long>& windows)
{
	Display* display = static_cast<Display*>(dpy);
	Window win = static_cast<Window>(window);
	Window parent, *children;
	unsigned int nchildren;
	
	// Get children of this window
	if (XQueryTree(display, win, &win, &parent, &children, &nchildren)) {
		for (unsigned int i = 0; i < nchildren; i++) {
			// Check if this is a real window (not just a container)
			XWindowAttributes attrs;
			if (XGetWindowAttributes(display, children[i], &attrs)) {
				// Only include windows that are mapped and have a name
				if (attrs.map_state == IsViewable) {
					QString name = getWindowName(children[i]);
					qDebug() << "X11Support::getAllWindowsRecursive() - Found window" << QString::number(children[i], 16) << "name:" << name;
					if (!name.isEmpty() && name != "<Unknown>") {
						windows.append(children[i]);
						qDebug() << "X11Support::getAllWindowsRecursive() - Added window" << QString::number(children[i], 16) << "to list";
					}
				}
			}
			
			// Recursively search children
			getAllWindowsRecursive(display, children[i], windows);
		}
		XFree(children);
	}
}

QString X11Support::getWindowPropertyUTF8String(unsigned long window, const QString& name)
{
	int numItems;
	char* data;
	QString value;
	if(!getWindowPropertyHelper(window, atom(name), atom("UTF8_STRING"), numItems, data))
		return value;
	value = QString::fromUtf8(data);
	XFree(data);
	return value;
}

QString X11Support::getWindowPropertyLatin1String(unsigned long window, const QString& name)
{
	int numItems;
	char* data;
	QString value;
	if(!getWindowPropertyHelper(window, atom(name), XA_STRING, numItems, data))
		return value;
	value = QString::fromLatin1(data);
	XFree(data);
	return value;
}

QString X11Support::getWindowName(unsigned long window)
{
	QString result = getWindowPropertyUTF8String(window, "_NET_WM_VISIBLE_NAME");
	if(result.isEmpty())
		result = getWindowPropertyUTF8String(window, "_NET_WM_NAME");
	if(result.isEmpty())
		result = getWindowPropertyLatin1String(window, "WM_NAME");
	if(result.isEmpty())
		result = "<Unknown>";
	return result;
}

bool X11Support::getWindowMinimizedState(unsigned long window)
{
    Atom wmState = X11Support::atom("_NET_WM_STATE");
    Atom wmHidden = X11Support::atom("_NET_WM_STATE_HIDDEN");
    Atom *atoms;
    atoms=NULL;
    Atom actual_type;
    int actual_format;
    unsigned long i, num_items, bytes_after;

    XGetWindowProperty(x11DisplayCompat(), window, wmState, 0, 1024, False, XA_ATOM, &actual_type, &actual_format, &num_items, &bytes_after, (unsigned char**)&atoms);
    //usleep(1000000);
    //qDebug() << "itemmmmmm=" <<nItem;
    for(i=0; i<num_items; ++i)
    {
        if(atoms[i] == wmHidden)
        {
            XFree(atoms);
            atoms=NULL;
            return true;
        }
    }
    XFree(atoms);
    atoms=NULL;
    return false;
}

QIcon X11Support::getWindowIcon(unsigned long window)
{
    QIcon icon;
    if (!x11DisplayCompat()) {
        return icon;
    }

	int numItems = 0;
	unsigned long* rawData = nullptr;
	if(!getWindowPropertyHelper(window, atom("_NET_WM_ICON"), XA_CARDINAL, numItems, rawData) || !rawData || numItems < 2) {
		return icon;
    }

    // NOTE: _NET_WM_ICON is an array of CARDINALs:
    // [width, height, argb-pixels...][width, height, argb-pixels...]...
    // During early startup some apps briefly expose malformed/partial data.
	unsigned long* data = rawData;
    int remaining = numItems;

    // Safety caps (avoid huge allocations / bogus metadata).
    // Increased from 512 to 2048 to accommodate larger icons while still preventing DoS
    constexpr int kMaxIconDim = 2048;
    // Track if we've successfully parsed at least one icon
    bool hasValidIcon = false;

	while(remaining >= 2)
	{
        // Read header (don't consume yet - validate first)
        const unsigned long wU = data[0];
        const unsigned long hU = data[1];
		const int width  = static_cast<int>(wU);
		const int height = static_cast<int>(hU);

        // Validate header dimensions
        if (width <= 0 || height <= 0 || width > kMaxIconDim || height > kMaxIconDim) {
            // Invalid header - try to recover by skipping this entry
            // Skip forward by 1 item and try again (heuristic: next icon might start here)
            if (remaining > 2) {
                data += 1;
                remaining -= 1;
                continue;
            }
            // No more data to try
            break;
        }

        const qint64 pixels64 = static_cast<qint64>(width) * static_cast<qint64>(height);
        // Check if we have enough data (accounting for the 2 header items we haven't consumed yet)
        if (pixels64 <= 0 || pixels64 > (remaining - 2)) {
            // Not enough data for this icon - if we already have valid icons, stop here
            // Otherwise try to skip forward (might be mid-update during startup)
            if (hasValidIcon) {
                break;
            }
            // Try skipping forward by 1 to find next valid icon
            if (remaining > 2) {
                data += 1;
                remaining -= 1;
                continue;
            }
            break;
        }
        const int pixels = static_cast<int>(pixels64);

        // Header is valid - now consume it
        data += 2;
        remaining -= 2;

        QImage image(width, height, QImage::Format_ARGB32);
        if (image.isNull()) {
            // Allocation failed; skip this icon's pixel data and continue
            if (remaining >= pixels) {
                data += pixels;
                remaining -= pixels;
            } else {
                // Not enough data - stop here
                break;
            }
            continue;
        }

        // Fast pixel copy with bounds safety - we know pixels <= remaining at this point
        for (int y = 0; y < height; ++y) {
            QRgb* scan = reinterpret_cast<QRgb*>(image.scanLine(y));
            const int rowOff = y * width;
            for (int x = 0; x < width; ++x) {
                scan[x] = static_cast<QRgb>(data[rowOff + x] & 0xFFFFFFFFu);
            }
        }

        data += pixels;
        remaining -= pixels;

        icon.addPixmap(QPixmap::fromImage(image));
        hasValidIcon = true;
	}

	XFree(rawData);
	return icon;
}

bool X11Support::getWindowUrgency(unsigned long window)
{
    XWMHints* hints = XGetWMHints(x11DisplayCompat(), window);
	if(hints == NULL)
		return false;
	bool isUrgent = (hints->flags & 256) != 0; // UrgencyHint
	XFree(hints);
	return isUrgent;
}

void X11Support::registerForWindowPropertyChanges(unsigned long window)
{
    selectInput(window, PropertyChangeMask);
}

void X11Support::registerForWindowStructureNotify(unsigned long window)
{
    selectInput(window, StructureNotifyMask);
}

void X11Support::registerForTrayIconUpdates(unsigned long window)
{
    selectInput(window, StructureNotifyMask);

	// Apparently, there is no need to destroy damage object, as it's gone automatically when window is destroyed.
    XDamageCreate(x11DisplayCompat(), window, XDamageReportNonEmpty);
}

static void sendNETWMMessage(unsigned long window, const QString& atomName, unsigned long l0 = 0, unsigned long l1 = 0, unsigned long l2 = 0, unsigned long l3 = 0, unsigned long l4 = 0)
{
	XClientMessageEvent event;
	event.type = ClientMessage;
	event.window = window;
	event.message_type = X11Support::atom(atomName);
	event.format = 32;
	event.data.l[0] = l0;
	event.data.l[1] = l1;
	event.data.l[2] = l2;
	event.data.l[3] = l3;
	event.data.l[4] = l4;
    XSendEvent(x11DisplayCompat(), X11Support::rootWindow(), False, SubstructureNotifyMask | SubstructureRedirectMask, reinterpret_cast<XEvent*>(&event));
}

void X11Support::activateWindow(unsigned long window)
{
    XWindowChanges wc;
	wc.stack_mode = Above;
    XConfigureWindow(x11DisplayCompat(), window, CWStackMode, &wc);

	// Apparently, KWin won't bring window to top with configure request,
	// so we also need to ask it politely by sending a message.
	sendNETWMMessage(window, "_NET_ACTIVE_WINDOW", 2, CurrentTime);
}

void X11Support::minimizeWindow(unsigned long window)
{
    XIconifyWindow(x11DisplayCompat(), window, x11AppScreenCompat());
}

void X11Support::closeWindow(unsigned long window)
{
	sendNETWMMessage(window, "_NET_CLOSE_WINDOW", CurrentTime, 2);
}

void X11Support::destroyWindow(unsigned long window)
{
    XDestroyWindow(x11DisplayCompat(), window);
}

void X11Support::killClient(unsigned long window)
{
    XKillClient(x11DisplayCompat(), window);
}

unsigned long X11Support::systemTrayAtom()
{
    return atom(QString("_NET_SYSTEM_TRAY_S") + QString::number(x11AppScreenCompat()));
}

bool X11Support::makeSystemTray(unsigned long window)
{
    if(XGetSelectionOwner(x11DisplayCompat(), systemTrayAtom()) != 0)
        return false;

        XSetSelectionOwner(x11DisplayCompat(), systemTrayAtom(), window, CurrentTime);
	setWindowPropertyVisualId(window, "_NET_SYSTEM_TRAY_VISUAL", getARGBVisualId());
    XSync(x11DisplayCompat(), False);

	// Inform other clients.
    XClientMessageEvent event;
	event.type = ClientMessage;
	event.window = rootWindow();
	event.message_type = atom("MANAGER");
	event.format = 32;
	event.data.l[0] = CurrentTime;
	event.data.l[1] = systemTrayAtom();
	event.data.l[2] = window;
	event.data.l[3] = 0;
	event.data.l[4] = 0;
    XSendEvent(x11DisplayCompat(), X11Support::rootWindow(), False, StructureNotifyMask, reinterpret_cast<XEvent*>(&event));

	return true;
}

void X11Support::freeSystemTray()
{
    XSetSelectionOwner(x11DisplayCompat(), systemTrayAtom(), None, CurrentTime);
}

unsigned long X11Support::getARGBVisualId()
{
    XVisualInfo visualInfoTemplate;
    visualInfoTemplate.screen = x11AppScreenCompat();
	visualInfoTemplate.depth = 32;
	visualInfoTemplate.red_mask = 0x00FF0000;
	visualInfoTemplate.green_mask = 0x0000FF00;
	visualInfoTemplate.blue_mask = 0x000000FF;

	int numVisuals;
    XVisualInfo* visualInfoList = XGetVisualInfo(x11DisplayCompat(), VisualScreenMask | VisualDepthMask | VisualRedMaskMask | VisualGreenMaskMask | VisualBlueMaskMask, &visualInfoTemplate, &numVisuals);
    if (!visualInfoList || numVisuals <= 0) {
        // Fallback: use default visual id if ARGB visual isn't available.
        // This can happen on some X servers or during early startup when extensions aren't fully ready.
        Visual* v = DefaultVisual(x11DisplayCompat(), x11AppScreenCompat());
        return XVisualIDFromVisual(v);
    }

    unsigned long id = visualInfoList[0].visualid;
    XFree(visualInfoList);
    return id;
}

void X11Support::redirectWindow(unsigned long window)
{
    XCompositeRedirectWindow(x11DisplayCompat(), window, CompositeRedirectManual);
}

void X11Support::unredirectWindow(unsigned long window)
{
    XCompositeUnredirectWindow(x11DisplayCompat(), window, CompositeRedirectManual);
}

// FIXME: How to convert Pixmap to QPixmap for Qt5?
QPixmap X11Support::getWindowPixmap(unsigned long window)
{
#if QT_VERSION >= 0x050000
    XWindowAttributes attr;
    if (!x11DisplayCompat()) {
        return QPixmap();
    }
    if (XGetWindowAttributes(x11DisplayCompat(), window, &attr) == 0) {
        return QPixmap();
    }
    // If the icon window isn't mapped / viewable yet, it may not have a drawable pixmap.
    if (attr.map_state != IsViewable) {
        return QPixmap();
    }
    if (attr.width <= 0 || attr.height <= 0) {
        return QPixmap();
    }

    QIcon icon = X11Support::getWindowIcon(window);
    //qDebug() << icon.availableSizes();
    QPixmap pixmap(icon.pixmap(attr.width, attr.height));

    if(pixmap.isNull())
    {
        // Composite path: some tray icons only render correctly via XComposite.
        Pixmap pix = XCompositeNameWindowPixmap(x11DisplayCompat(), window);
        if (pix == None) {
            return QPixmap();
        }
        XImage *ximage = XGetImage(x11DisplayCompat(), pix, 0, 0,
                                   static_cast<unsigned int>(attr.width),
                                   static_cast<unsigned int>(attr.height),
                                   AllPlanes, ZPixmap);
        XFreePixmap(x11DisplayCompat(), pix);
        if (!ximage || !ximage->data) {
            if (ximage) {
                XDestroyImage(ximage);
            }
            return QPixmap();
        }

        // This is safe to do since we only composite ARGB32 windows, and PictStandardARGB32
        // matches QImage::Format_ARGB32_Premultiplied.
        QImage image(reinterpret_cast<const uchar*>(ximage->data),
                     ximage->width,
                     ximage->height,
                     ximage->bytes_per_line,
                     QImage::Format_ARGB32_Premultiplied);
        // Detach from XImage-owned memory before freeing it.
        pixmap = QPixmap::fromImage(image.copy());
        XDestroyImage(ximage);
    }
    return pixmap;
#else
    return QPixmap::fromX11Pixmap(XCompositeNameWindowPixmap(QX11Info::display(), window));
#endif
}


QRect X11Support::getWindowWindowsGeometry(unsigned long window)
{
    XWindowAttributes attr;
    QRect windowGeometry(0,0,0,0);

    if(XGetWindowAttributes(x11DisplayCompat(), window, &attr) != 0)
    {
        windowGeometry.setX(attr.x);
        windowGeometry.setY(attr.y);
        windowGeometry.setWidth(attr.width);
        windowGeometry.setHeight(attr.height);
    }
    //XFree(&attr);
    return windowGeometry;
}

void X11Support::resizeWindow(unsigned long window, int width, int height)
{
    XResizeWindow(x11DisplayCompat(), window, width, height);
}

void X11Support::moveWindow(unsigned long window, int x, int y)
{
    XMoveWindow(x11DisplayCompat(), window, x, y);
}

void X11Support::mapWindow(unsigned long window)
{
    XMapWindow(x11DisplayCompat(), window);
}

void X11Support::reparentWindow(unsigned long window, unsigned long parent)
{
    XReparentWindow(x11DisplayCompat(), window, parent, 0, 0);
    XSync(x11DisplayCompat(), False);
}

void X11Support::setWindowBackgroundBlack(unsigned long window)
{
    XSetWindowBackground(x11DisplayCompat(), window, BlackPixel(x11DisplayCompat(), x11AppScreenCompat()));
}
