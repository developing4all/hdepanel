/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * Copyright: 2015-2016 Haydar Alkaduhimi
 * Copyright (C) 2014 Leslie Zhai <xiang.zhai@i-soft.com.cn>
 * Authors:
 *   Haydar Alkaduhimi <haydar@hosting4all.com>
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

#include "x11support.h"

// TODO: Keep all the X11 stuff with scary defines below normal headers.
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>

#include <X11/extensions/Xrender.h>


static XErrorHandler oldX11ErrorHandler = NULL;

static int x11errorHandler(Display* display, XErrorEvent* error)
{
	if (error->error_code == BadWindow) return 0;

	return (*oldX11ErrorHandler)(display, error);
}

X11Support* X11Support::m_instance = NULL;

X11Support::X11Support()
{
	m_instance = this;
	oldX11ErrorHandler = XSetErrorHandler(x11errorHandler);
	int damageErrorBase;
	XDamageQueryExtension(QX11Info::display(), &m_damageEventBase, &damageErrorBase);
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
		XDamageSubtract(QX11Info::display(), damageEvent->damage, None, None);

		emit windowDamaged(event->xany.window);
	}
	if (event->type == DestroyNotify)
		emit windowClosed(event->xdestroywindow.window);
	if (event->type == ConfigureNotify)
		emit windowReconfigured(event->xconfigure.window, event->xconfigure.x, event->xconfigure.y, event->xconfigure.width, event->xconfigure.height);
	if (event->type == PropertyNotify)
		emit windowPropertyChanged(event->xproperty.window, event->xproperty.atom);
    if (event->type == ClientMessage)
        emit clientMessageReceived(event->xclient.window, event->xclient.message_type, event->xclient.data.b);
}

unsigned long X11Support::rootWindow()
{
	return QX11Info::appRootWindow();
}

unsigned long X11Support::atom(const QString& name)
{
	if(!m_instance->m_cachedAtoms.contains(name))
		m_instance->m_cachedAtoms[name] = XInternAtom(QX11Info::display(), name.toLatin1().data(), False);
	return m_instance->m_cachedAtoms[name];
}

void X11Support::removeWindowProperty(unsigned long window, const QString& name)
{
	XDeleteProperty(QX11Info::display(), window, atom(name));
}

void X11Support::setWindowPropertyCardinalArray(unsigned long window, const QString& name, const QVector<unsigned long>& values)
{
	XChangeProperty(QX11Info::display(), window, atom(name), XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(values.data()), values.size());
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

    //so we take our panelsize from the bottom up
    desstrut[0] = left; desstrut[1] = right;
    desstrut[2] = top;  desstrut[3] = bottom;

    desstrut[4] = leftStartY;    desstrut[5] = leftEndY;
    desstrut[6] = rightStartY;   desstrut[7] = rightEndY;
    desstrut[8] = topStartX;     desstrut[9] = topEndX;
    desstrut[10] = bottomStartX; desstrut[11] = bottomEndX;

    //now we can change that property right
    XChangeProperty(QX11Info::display(), _wid , X11Support::atom("_NET_WM_STRUT_PARTIAL"),
                    XA_CARDINAL, 32, PropModeReplace,  (unsigned char *) desstrut, 12  );

    XChangeProperty(QX11Info::display(), _wid, X11Support::atom("_NET_WM_STRUT"),
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char*) desstrut, 4);
}



void X11Support::setWindowPropertyCardinal(unsigned long window, const QString& name, unsigned long value)
{
	XChangeProperty(QX11Info::display(), window, atom(name), XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(&value), 1);
}

void X11Support::setWindowPropertyVisualId(unsigned long window, const QString& name, unsigned long value)
{
	XChangeProperty(QX11Info::display(), window, atom(name), XA_VISUALID, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(&value), 1);
}

template<class T>
static bool getWindowPropertyHelper(unsigned long window, unsigned long atom, unsigned long type, int& numItems, T*& data)
{
	Atom retType;
	int retFormat;
	unsigned long numItemsTemp;
	unsigned long bytesLeft;
	if(XGetWindowProperty(QX11Info::display(), window, atom, 0, 0x7FFFFFFF, False, type, &retType, &retFormat, &numItemsTemp, &bytesLeft, reinterpret_cast<unsigned char**>(&data)) != Success)
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

QIcon X11Support::getWindowIcon(unsigned long window)
{
	int numItems;
	unsigned long* rawData;
	QIcon icon;
	if(!getWindowPropertyHelper(window, atom("_NET_WM_ICON"), XA_CARDINAL, numItems, rawData))
		return icon;
	unsigned long* data = rawData;
	while(numItems > 0)
	{
		int width = static_cast<int>(data[0]);
		int height = static_cast<int>(data[1]);
		data += 2;
		numItems -= 2;
		QImage image(width, height, QImage::Format_ARGB32);
		for(int i = 0; i < height; i++)
		{
			for(int k = 0; k < width; k++)
			{
				image.setPixel(k, i, static_cast<unsigned int>(data[i*width + k]));
			}
		}
		data += width*height;
		numItems -= width*height;
		icon.addPixmap(QPixmap::fromImage(image));
	}
	XFree(rawData);
	return icon;
}

bool X11Support::getWindowUrgency(unsigned long window)
{
	XWMHints* hints = XGetWMHints(QX11Info::display(), window);
	if(hints == NULL)
		return false;
	bool isUrgent = (hints->flags & 256) != 0; // UrgencyHint
	XFree(hints);
	return isUrgent;
}

void X11Support::registerForWindowPropertyChanges(unsigned long window)
{
	XSelectInput(QX11Info::display(), window, PropertyChangeMask);
}

void X11Support::registerForTrayIconUpdates(unsigned long window)
{
	XSelectInput(QX11Info::display(), window, StructureNotifyMask);

	// Apparently, there is no need to destroy damage object, as it's gone automatically when window is destroyed.
	XDamageCreate(QX11Info::display(), window, XDamageReportNonEmpty);
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
	XSendEvent(QX11Info::display(), X11Support::rootWindow(), False, SubstructureNotifyMask | SubstructureRedirectMask, reinterpret_cast<XEvent*>(&event));
}

void X11Support::activateWindow(unsigned long window)
{
	XWindowChanges wc;
	wc.stack_mode = Above;
	XConfigureWindow(QX11Info::display(), window, CWStackMode, &wc);

	// Apparently, KWin won't bring window to top with configure request,
	// so we also need to ask it politely by sending a message.
	sendNETWMMessage(window, "_NET_ACTIVE_WINDOW", 2, CurrentTime);
}

void X11Support::minimizeWindow(unsigned long window)
{
    XIconifyWindow(QX11Info::display(), window, QX11Info::appScreen());
}

void X11Support::closeWindow(unsigned long window)
{
	sendNETWMMessage(window, "_NET_CLOSE_WINDOW", CurrentTime, 2);
}

void X11Support::destroyWindow(unsigned long window)
{
	XDestroyWindow(QX11Info::display(), window);
}

void X11Support::killClient(unsigned long window)
{
	XKillClient(QX11Info::display(), window);
}

unsigned long X11Support::systemTrayAtom()
{
	return atom(QString("_NET_SYSTEM_TRAY_S") + QString::number(QX11Info::appScreen()));
}

bool X11Support::makeSystemTray(unsigned long window)
{
    if(XGetSelectionOwner(QX11Info::display(), systemTrayAtom()) != 0)
        return false;

	XSetSelectionOwner(QX11Info::display(), systemTrayAtom(), window, CurrentTime);
	setWindowPropertyVisualId(window, "_NET_SYSTEM_TRAY_VISUAL", getARGBVisualId());
	XSync(QX11Info::display(), False);

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
	XSendEvent(QX11Info::display(), X11Support::rootWindow(), False, StructureNotifyMask, reinterpret_cast<XEvent*>(&event));

	return true;
}

void X11Support::freeSystemTray()
{
    XSetSelectionOwner(QX11Info::display(), systemTrayAtom(), None, CurrentTime);
}

unsigned long X11Support::getARGBVisualId()
{
	XVisualInfo visualInfoTemplate;
	visualInfoTemplate.screen = QX11Info::appScreen();
	visualInfoTemplate.depth = 32;
	visualInfoTemplate.red_mask = 0x00FF0000;
	visualInfoTemplate.green_mask = 0x0000FF00;
	visualInfoTemplate.blue_mask = 0x000000FF;

	int numVisuals;
	XVisualInfo* visualInfoList = XGetVisualInfo(QX11Info::display(), VisualScreenMask | VisualDepthMask | VisualRedMaskMask | VisualGreenMaskMask | VisualBlueMaskMask, &visualInfoTemplate, &numVisuals);
	unsigned long id = visualInfoList[0].visualid;
	XFree(visualInfoList);

	return id;
}

void X11Support::redirectWindow(unsigned long window)
{
	XCompositeRedirectWindow(QX11Info::display(), window, CompositeRedirectManual);
}

void X11Support::unredirectWindow(unsigned long window)
{
	XCompositeUnredirectWindow(QX11Info::display(), window, CompositeRedirectManual);
}

// FIXME: How to convert Pixmap to QPixmap for Qt5?
QPixmap X11Support::getWindowPixmap(unsigned long window)
{
#if QT_VERSION >= 0x050000
    XWindowAttributes attr;
    XGetWindowAttributes(QX11Info::display(), window, &attr);

    QIcon icon = X11Support::getWindowIcon(window);
    //qDebug() << icon.availableSizes();
    QPixmap pixmap(icon.pixmap(attr.width, attr.height));

    if(pixmap.isNull())
    {
        Pixmap pix = XCompositeNameWindowPixmap(QX11Info::display(), window);
        XImage *ximage = XGetImage(QX11Info::display(), pix, 0, 0, attr.width, attr.height, AllPlanes, ZPixmap);
        XFreePixmap(QX11Info::display(), pix);

        // This is safe to do since we only composite ARGB32 windows, and PictStandardARGB32
        // matches QImage::Format_ARGB32_Premultiplied.
        QImage image((const uchar*)ximage->data, ximage->width, ximage->height, ximage->bytes_per_line,
                     QImage::Format_ARGB32_Premultiplied);
        pixmap = QPixmap::fromImage(image);
    }
    return pixmap;
#else
    return QPixmap::fromX11Pixmap(XCompositeNameWindowPixmap(QX11Info::display(), window));
#endif
}

void X11Support::resizeWindow(unsigned long window, int width, int height)
{
	XResizeWindow(QX11Info::display(), window, width, height);
}

void X11Support::moveWindow(unsigned long window, int x, int y)
{
	XMoveWindow(QX11Info::display(), window, x, y);
}

void X11Support::mapWindow(unsigned long window)
{
	XMapWindow(QX11Info::display(), window);
}

void X11Support::reparentWindow(unsigned long window, unsigned long parent)
{
	XReparentWindow(QX11Info::display(), window, parent, 0, 0);
	XSync(QX11Info::display(), False);
}

void X11Support::setWindowBackgroundBlack(unsigned long window)
{
	XSetWindowBackground(QX11Info::display(), window, BlackPixel(QX11Info::display(), QX11Info::appScreen()));
}

