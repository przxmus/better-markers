#include "bm-window-focus.hpp"

#include <obs-module.h>

#include <QWidget>

#define NOMINMAX
#include <windows.h>

namespace bm::detail {

namespace {

HWND hwnd_from_widget(QWidget *widget)
{
	if (!widget)
		return nullptr;
	return reinterpret_cast<HWND>(widget->winId());
}

bool activate_hwnd(HWND hwnd)
{
	if (!hwnd || !IsWindow(hwnd))
		return false;

	if (IsIconic(hwnd))
		ShowWindow(hwnd, SW_RESTORE);
	else
		ShowWindow(hwnd, SW_SHOW);

	BringWindowToTop(hwnd);
	if (SetForegroundWindow(hwnd) != FALSE)
		return true;

	const HWND foreground = GetForegroundWindow();
	const DWORD self_tid = GetCurrentThreadId();
	const DWORD target_tid = GetWindowThreadProcessId(hwnd, nullptr);
	const DWORD foreground_tid = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;

	bool attached_target = false;
	bool attached_foreground = false;
	if (target_tid != 0 && target_tid != self_tid)
		attached_target = (AttachThreadInput(self_tid, target_tid, TRUE) != FALSE);
	if (foreground_tid != 0 && foreground_tid != self_tid && foreground_tid != target_tid)
		attached_foreground = (AttachThreadInput(self_tid, foreground_tid, TRUE) != FALSE);

	SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	BringWindowToTop(hwnd);
	SetActiveWindow(hwnd);
	SetFocus(hwnd);
	const bool foreground_set = (SetForegroundWindow(hwnd) != FALSE);

	if (attached_foreground)
		AttachThreadInput(self_tid, foreground_tid, FALSE);
	if (attached_target)
		AttachThreadInput(self_tid, target_tid, FALSE);

	return foreground_set || GetForegroundWindow() == hwnd;
}

} // namespace

WindowFocusSnapshot capture_platform_window_focus_snapshot()
{
	const HWND hwnd = GetForegroundWindow();
	if (!hwnd) {
		blog(LOG_DEBUG, "[better-markers][focus][win] foreground capture failed: no active window");
		return {};
	}

	blog(LOG_DEBUG, "[better-markers][focus][win] captured foreground hwnd=%p", hwnd);
	return {WindowFocusSnapshotKind::Win32Handle, static_cast<quint64>(reinterpret_cast<quintptr>(hwnd))};
}

bool activate_platform_marker_dialog_window(QWidget *dialog)
{
	if (!dialog)
		return false;

	HWND parent_hwnd = nullptr;
	if (QWidget *parent_window = dialog->parentWidget() ? dialog->parentWidget()->window() : nullptr)
		parent_hwnd = hwnd_from_widget(parent_window);

	if (parent_hwnd && IsWindow(parent_hwnd) && IsIconic(parent_hwnd)) {
		blog(LOG_DEBUG, "[better-markers][focus][win] restoring OBS parent window hwnd=%p", parent_hwnd);
		ShowWindow(parent_hwnd, SW_RESTORE);
		activate_hwnd(parent_hwnd);
	}

	const HWND dialog_hwnd = hwnd_from_widget(dialog->window());
	if (!dialog_hwnd) {
		blog(LOG_DEBUG, "[better-markers][focus][win] dialog activation skipped: missing HWND");
		return false;
	}

	const bool activated = activate_hwnd(dialog_hwnd);
	blog(LOG_DEBUG, "[better-markers][focus][win] dialog activation hwnd=%p result=%s", dialog_hwnd,
	     activated ? "success" : "failure");
	return activated;
}

bool restore_platform_window_focus(const WindowFocusSnapshot &snapshot)
{
	if (snapshot.kind != WindowFocusSnapshotKind::Win32Handle || snapshot.value == 0)
		return false;

	HWND hwnd = reinterpret_cast<HWND>(static_cast<quintptr>(snapshot.value));
	if (!hwnd || !IsWindow(hwnd)) {
		blog(LOG_DEBUG, "[better-markers][focus][win] restore skipped: window no longer exists");
		return false;
	}

	const bool restored = activate_hwnd(hwnd);
	blog(LOG_DEBUG, "[better-markers][focus][win] restore hwnd=%p result=%s", hwnd,
	     restored ? "success" : "failure");
	return restored;
}

} // namespace bm::detail
