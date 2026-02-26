#include "bm-window-focus.hpp"

#include <obs-module.h>

#include <QGuiApplication>
#include <QWidget>

#include <cstring>

#ifdef BETTER_MARKERS_HAVE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

namespace bm::detail {

namespace {

bool is_x11_platform()
{
	const QString platform = QGuiApplication::platformName().toLower();
	return platform.contains("xcb");
}

#ifdef BETTER_MARKERS_HAVE_X11
bool query_active_x11_window(Window *out_window)
{
	if (!out_window)
		return false;

	Display *display = XOpenDisplay(nullptr);
	if (!display)
		return false;

	const Window root = DefaultRootWindow(display);
	const Atom net_active_window = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
	if (net_active_window == None) {
		XCloseDisplay(display);
		return false;
	}

	Atom actual_type = None;
	int actual_format = 0;
	unsigned long item_count = 0;
	unsigned long bytes_after = 0;
	unsigned char *property = nullptr;
	const int result = XGetWindowProperty(display, root, net_active_window, 0, 1, False, XA_WINDOW, &actual_type,
					      &actual_format, &item_count, &bytes_after, &property);
	if (result != Success || actual_type != XA_WINDOW || actual_format != 32 || item_count != 1 || !property) {
		if (property)
			XFree(property);
		XCloseDisplay(display);
		return false;
	}

	*out_window = *(reinterpret_cast<Window *>(property));
	XFree(property);
	XCloseDisplay(display);
	return *out_window != 0;
}

bool activate_x11_window(Window window)
{
	if (window == 0)
		return false;

	Display *display = XOpenDisplay(nullptr);
	if (!display)
		return false;

	const Window root = DefaultRootWindow(display);
	const Atom net_active_window = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
	if (net_active_window == None) {
		XCloseDisplay(display);
		return false;
	}

	XEvent event;
	std::memset(&event, 0, sizeof(event));
	event.xclient.type = ClientMessage;
	event.xclient.message_type = net_active_window;
	event.xclient.window = window;
	event.xclient.format = 32;
	event.xclient.data.l[0] = 1; // Application source indication.
	event.xclient.data.l[1] = CurrentTime;
	event.xclient.data.l[2] = 0;
	event.xclient.data.l[3] = 0;
	event.xclient.data.l[4] = 0;

	const Status send_status =
		XSendEvent(display, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
	XFlush(display);
	XCloseDisplay(display);
	return send_status != 0;
}
#endif

} // namespace

WindowFocusSnapshot capture_platform_window_focus_snapshot()
{
#ifdef BETTER_MARKERS_HAVE_X11
	if (!is_x11_platform()) {
		blog(LOG_DEBUG, "[better-markers][focus][linux] skipping capture on non-X11 platform");
		return {};
	}

	Window active_window = 0;
	if (!query_active_x11_window(&active_window)) {
		blog(LOG_DEBUG, "[better-markers][focus][linux] X11 foreground capture failed");
		return {};
	}

	blog(LOG_DEBUG, "[better-markers][focus][linux] captured X11 active window=0x%lx",
	     static_cast<unsigned long>(active_window));
	return {WindowFocusSnapshotKind::X11Window, static_cast<quint64>(active_window)};
#else
	blog(LOG_DEBUG, "[better-markers][focus][linux] built without X11 support");
	return {};
#endif
}

bool activate_platform_marker_dialog_window(QWidget *dialog)
{
	if (!dialog)
		return false;

#ifdef BETTER_MARKERS_HAVE_X11
	if (!is_x11_platform()) {
		blog(LOG_DEBUG, "[better-markers][focus][linux] non-X11 platform: Qt best effort only");
		return false;
	}

	const Window dialog_window = static_cast<Window>(dialog->window()->winId());
	if (dialog_window == 0) {
		blog(LOG_DEBUG, "[better-markers][focus][linux] dialog activation skipped: missing X11 window id");
		return false;
	}

	const bool activated = activate_x11_window(dialog_window);
	blog(LOG_DEBUG, "[better-markers][focus][linux] dialog activation window=0x%lx result=%s",
	     static_cast<unsigned long>(dialog_window), activated ? "success" : "failure");
	return activated;
#else
	blog(LOG_DEBUG, "[better-markers][focus][linux] activation unavailable without X11 support");
	return false;
#endif
}

bool restore_platform_window_focus(const WindowFocusSnapshot &snapshot)
{
	if (snapshot.kind != WindowFocusSnapshotKind::X11Window || snapshot.value == 0)
		return false;

#ifdef BETTER_MARKERS_HAVE_X11
	if (!is_x11_platform()) {
		blog(LOG_DEBUG, "[better-markers][focus][linux] restore skipped on non-X11 platform");
		return false;
	}

	const Window window = static_cast<Window>(snapshot.value);
	const bool restored = activate_x11_window(window);
	blog(LOG_DEBUG, "[better-markers][focus][linux] restore window=0x%lx result=%s",
	     static_cast<unsigned long>(window), restored ? "success" : "failure");
	return restored;
#else
	blog(LOG_DEBUG, "[better-markers][focus][linux] restore unavailable without X11 support");
	return false;
#endif
}

} // namespace bm::detail
