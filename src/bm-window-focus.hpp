#pragma once

#include <QtGlobal>

#include <cstdint>

class QWidget;

namespace bm {

enum class WindowFocusSnapshotKind : uint8_t {
	None = 0,
	Win32Handle,
	MacProcessId,
	X11Window,
};

struct WindowFocusSnapshot {
	WindowFocusSnapshotKind kind = WindowFocusSnapshotKind::None;
	quint64 value = 0;

	bool is_valid() const
	{
		return kind != WindowFocusSnapshotKind::None && value != 0;
	}
};

WindowFocusSnapshot capture_window_focus_snapshot();
bool activate_marker_dialog_window(QWidget *dialog);
bool restore_window_focus(const WindowFocusSnapshot &snapshot);
void activate_widget_qt_best_effort(QWidget *widget);

namespace detail {

WindowFocusSnapshot capture_platform_window_focus_snapshot();
bool activate_platform_marker_dialog_window(QWidget *dialog);
bool restore_platform_window_focus(const WindowFocusSnapshot &snapshot);

} // namespace detail

} // namespace bm
