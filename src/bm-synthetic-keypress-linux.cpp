#include "bm-synthetic-keypress.hpp"

#include <QGuiApplication>

#ifdef BETTER_MARKERS_HAVE_X11
#include <X11/Xlib.h>
#include <X11/keysym.h>
#ifdef BETTER_MARKERS_HAVE_XTEST
#include <X11/extensions/XTest.h>
#endif
#endif

#include <optional>
#include <vector>

namespace bm::detail {
namespace {

bool is_x11_platform()
{
	const QString platform = QGuiApplication::platformName().toLower();
	return platform.contains("xcb");
}

#ifdef BETTER_MARKERS_HAVE_X11
std::optional<KeySym> qt_key_to_x11_keysym(Qt::Key key)
{
	if (key >= Qt::Key_A && key <= Qt::Key_Z)
		return static_cast<KeySym>(XK_a + (key - Qt::Key_A));
	if (key >= Qt::Key_0 && key <= Qt::Key_9)
		return static_cast<KeySym>(XK_0 + (key - Qt::Key_0));
	if (key >= Qt::Key_F1 && key <= Qt::Key_F35)
		return static_cast<KeySym>(XK_F1 + (key - Qt::Key_F1));

	switch (key) {
	case Qt::Key_Escape:
		return XK_Escape;
	case Qt::Key_Tab:
		return XK_Tab;
	case Qt::Key_Backspace:
		return XK_BackSpace;
	case Qt::Key_Return:
	case Qt::Key_Enter:
		return XK_Return;
	case Qt::Key_Space:
		return XK_space;
	case Qt::Key_Insert:
		return XK_Insert;
	case Qt::Key_Delete:
		return XK_Delete;
	case Qt::Key_Home:
		return XK_Home;
	case Qt::Key_End:
		return XK_End;
	case Qt::Key_PageUp:
		return XK_Page_Up;
	case Qt::Key_PageDown:
		return XK_Page_Down;
	case Qt::Key_Left:
		return XK_Left;
	case Qt::Key_Right:
		return XK_Right;
	case Qt::Key_Up:
		return XK_Up;
	case Qt::Key_Down:
		return XK_Down;
	case Qt::Key_Control:
		return XK_Control_L;
	case Qt::Key_Shift:
		return XK_Shift_L;
	case Qt::Key_Alt:
		return XK_Alt_L;
	case Qt::Key_Meta:
		return XK_Super_L;
	case Qt::Key_Minus:
		return XK_minus;
	case Qt::Key_Equal:
		return XK_equal;
	case Qt::Key_BracketLeft:
		return XK_bracketleft;
	case Qt::Key_BracketRight:
		return XK_bracketright;
	case Qt::Key_Backslash:
		return XK_backslash;
	case Qt::Key_Semicolon:
		return XK_semicolon;
	case Qt::Key_Apostrophe:
		return XK_apostrophe;
	case Qt::Key_Comma:
		return XK_comma;
	case Qt::Key_Period:
		return XK_period;
	case Qt::Key_Slash:
		return XK_slash;
	case Qt::Key_QuoteLeft:
		return XK_grave;
	default:
		break;
	}

	return std::nullopt;
}

std::optional<KeySym> modifier_to_x11_keysym(Qt::KeyboardModifier modifier)
{
	switch (modifier) {
	case Qt::ShiftModifier:
		return XK_Shift_L;
	case Qt::ControlModifier:
		return XK_Control_L;
	case Qt::AltModifier:
		return XK_Alt_L;
	case Qt::MetaModifier:
		return XK_Super_L;
	default:
		return std::nullopt;
	}
}
#endif

} // namespace

SyntheticKeypressResult send_platform_synthetic_keypress(Qt::Key key, Qt::KeyboardModifiers modifiers)
{
	if (!is_x11_platform())
		return {SyntheticKeypressStatus::UnsupportedPlatform, "platform is not X11 (likely Wayland)"};

#ifndef BETTER_MARKERS_HAVE_X11
	Q_UNUSED(key);
	Q_UNUSED(modifiers);
	return {SyntheticKeypressStatus::UnsupportedPlatform, "plugin built without X11 support"};
#else
#ifndef BETTER_MARKERS_HAVE_XTEST
	Q_UNUSED(key);
	Q_UNUSED(modifiers);
	return {SyntheticKeypressStatus::UnsupportedPlatform, "plugin built without XTest support"};
#else
	Display *display = XOpenDisplay(nullptr);
	if (!display)
		return {SyntheticKeypressStatus::SystemFailure, "failed to open X11 display"};

	const std::optional<KeySym> main_keysym = qt_key_to_x11_keysym(key);
	if (!main_keysym.has_value()) {
		XCloseDisplay(display);
		return {SyntheticKeypressStatus::UnsupportedKey, "key is not mapped to X11 keysym"};
	}

	const KeyCode main_keycode = XKeysymToKeycode(display, main_keysym.value());
	if (main_keycode == 0) {
		XCloseDisplay(display);
		return {SyntheticKeypressStatus::UnsupportedKey, "keysym cannot be translated to X11 keycode"};
	}

	std::vector<KeyCode> modifier_keycodes;
	if (modifiers.testFlag(Qt::ShiftModifier)) {
		const KeyCode code = XKeysymToKeycode(display, modifier_to_x11_keysym(Qt::ShiftModifier).value());
		if (code == 0) {
			XCloseDisplay(display);
			return {SyntheticKeypressStatus::UnsupportedKey, "cannot map Shift modifier"};
		}
		modifier_keycodes.push_back(code);
	}
	if (modifiers.testFlag(Qt::ControlModifier)) {
		const KeyCode code = XKeysymToKeycode(display, modifier_to_x11_keysym(Qt::ControlModifier).value());
		if (code == 0) {
			XCloseDisplay(display);
			return {SyntheticKeypressStatus::UnsupportedKey, "cannot map Control modifier"};
		}
		modifier_keycodes.push_back(code);
	}
	if (modifiers.testFlag(Qt::AltModifier)) {
		const KeyCode code = XKeysymToKeycode(display, modifier_to_x11_keysym(Qt::AltModifier).value());
		if (code == 0) {
			XCloseDisplay(display);
			return {SyntheticKeypressStatus::UnsupportedKey, "cannot map Alt modifier"};
		}
		modifier_keycodes.push_back(code);
	}
	if (modifiers.testFlag(Qt::MetaModifier)) {
		const KeyCode code = XKeysymToKeycode(display, modifier_to_x11_keysym(Qt::MetaModifier).value());
		if (code == 0) {
			XCloseDisplay(display);
			return {SyntheticKeypressStatus::UnsupportedKey, "cannot map Meta modifier"};
		}
		modifier_keycodes.push_back(code);
	}

	for (KeyCode modifier_keycode : modifier_keycodes) {
		if (!XTestFakeKeyEvent(display, modifier_keycode, True, CurrentTime)) {
			XCloseDisplay(display);
			return {SyntheticKeypressStatus::SystemFailure, "failed to inject modifier key down event"};
		}
	}

	if (!XTestFakeKeyEvent(display, main_keycode, True, CurrentTime)) {
		XCloseDisplay(display);
		return {SyntheticKeypressStatus::SystemFailure, "failed to inject main key down event"};
	}
	if (!XTestFakeKeyEvent(display, main_keycode, False, CurrentTime)) {
		XCloseDisplay(display);
		return {SyntheticKeypressStatus::SystemFailure, "failed to inject main key up event"};
	}

	for (auto it = modifier_keycodes.rbegin(); it != modifier_keycodes.rend(); ++it) {
		if (!XTestFakeKeyEvent(display, *it, False, CurrentTime)) {
			XCloseDisplay(display);
			return {SyntheticKeypressStatus::SystemFailure, "failed to inject modifier key up event"};
		}
	}

	XFlush(display);
	XCloseDisplay(display);
	return {SyntheticKeypressStatus::Success, "ok"};
#endif
#endif
}

} // namespace bm::detail
