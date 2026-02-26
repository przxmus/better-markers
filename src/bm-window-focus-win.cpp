#include "bm-window-focus.hpp"

class QWidget;

namespace bm::detail {

WindowFocusSnapshot capture_platform_window_focus_snapshot()
{
	return {};
}

bool activate_platform_marker_dialog_window(QWidget *dialog)
{
	Q_UNUSED(dialog);
	return false;
}

bool restore_platform_window_focus(const WindowFocusSnapshot &snapshot)
{
	Q_UNUSED(snapshot);
	return false;
}

} // namespace bm::detail
