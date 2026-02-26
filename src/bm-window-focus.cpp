#include "bm-window-focus.hpp"

#include <QWidget>

namespace bm {

void activate_widget_qt_best_effort(QWidget *widget)
{
	if (!widget)
		return;

	if (QWidget *top_level = widget->window()) {
		if (top_level->isMinimized())
			top_level->showNormal();
		top_level->show();
		top_level->raise();
		top_level->activateWindow();
	}

	if (widget->isMinimized())
		widget->showNormal();
	widget->show();
	widget->raise();
	widget->activateWindow();
}

WindowFocusSnapshot capture_window_focus_snapshot()
{
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
	return detail::capture_platform_window_focus_snapshot();
#else
	return {};
#endif
}

bool activate_marker_dialog_window(QWidget *dialog)
{
	if (!dialog)
		return false;

	activate_widget_qt_best_effort(dialog);

#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
	return detail::activate_platform_marker_dialog_window(dialog);
#else
	return false;
#endif
}

bool restore_window_focus(const WindowFocusSnapshot &snapshot)
{
	if (!snapshot.is_valid())
		return false;

#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
	return detail::restore_platform_window_focus(snapshot);
#else
	return false;
#endif
}

} // namespace bm
