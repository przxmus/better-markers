#pragma once

namespace bm {

constexpr bool should_use_hotkey_focus_session(bool auto_focus_enabled, bool triggered_by_hotkey, bool dialog_required)
{
	return auto_focus_enabled && triggered_by_hotkey && dialog_required;
}

constexpr bool should_restore_focus_after_dialog(bool focus_session_active, bool dialog_executed)
{
	return focus_session_active && dialog_executed;
}

} // namespace bm
