#include "bm-synthetic-keypress.hpp"

#define NOMINMAX
#include <windows.h>

#include <optional>
#include <vector>

namespace bm::detail {
namespace {

std::optional<WORD> qt_key_to_virtual_key(Qt::Key key)
{
	if (key >= Qt::Key_A && key <= Qt::Key_Z)
		return static_cast<WORD>('A' + (key - Qt::Key_A));
	if (key >= Qt::Key_0 && key <= Qt::Key_9)
		return static_cast<WORD>('0' + (key - Qt::Key_0));
	if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
		return static_cast<WORD>(VK_F1 + (key - Qt::Key_F1));

	switch (key) {
	case Qt::Key_Escape:
		return VK_ESCAPE;
	case Qt::Key_Tab:
		return VK_TAB;
	case Qt::Key_Backspace:
		return VK_BACK;
	case Qt::Key_Return:
	case Qt::Key_Enter:
		return VK_RETURN;
	case Qt::Key_Space:
		return VK_SPACE;
	case Qt::Key_Insert:
		return VK_INSERT;
	case Qt::Key_Delete:
		return VK_DELETE;
	case Qt::Key_Home:
		return VK_HOME;
	case Qt::Key_End:
		return VK_END;
	case Qt::Key_PageUp:
		return VK_PRIOR;
	case Qt::Key_PageDown:
		return VK_NEXT;
	case Qt::Key_Left:
		return VK_LEFT;
	case Qt::Key_Right:
		return VK_RIGHT;
	case Qt::Key_Up:
		return VK_UP;
	case Qt::Key_Down:
		return VK_DOWN;
	case Qt::Key_Control:
		return VK_CONTROL;
	case Qt::Key_Shift:
		return VK_SHIFT;
	case Qt::Key_Alt:
		return VK_MENU;
	case Qt::Key_Meta:
		return VK_LWIN;
	case Qt::Key_Minus:
		return VK_OEM_MINUS;
	case Qt::Key_Equal:
		return VK_OEM_PLUS;
	case Qt::Key_BracketLeft:
		return VK_OEM_4;
	case Qt::Key_BracketRight:
		return VK_OEM_6;
	case Qt::Key_Backslash:
		return VK_OEM_5;
	case Qt::Key_Semicolon:
		return VK_OEM_1;
	case Qt::Key_Apostrophe:
		return VK_OEM_7;
	case Qt::Key_Comma:
		return VK_OEM_COMMA;
	case Qt::Key_Period:
		return VK_OEM_PERIOD;
	case Qt::Key_Slash:
		return VK_OEM_2;
	case Qt::Key_QuoteLeft:
		return VK_OEM_3;
	default:
		break;
	}

	return std::nullopt;
}

void append_key_event(std::vector<INPUT> *events, WORD virtual_key, bool key_down)
{
	if (!events)
		return;

	INPUT input{};
	input.type = INPUT_KEYBOARD;
	input.ki.wVk = virtual_key;
	input.ki.dwFlags = key_down ? 0 : KEYEVENTF_KEYUP;
	events->push_back(input);
}

} // namespace

SyntheticKeypressResult send_platform_synthetic_keypress(Qt::Key key, Qt::KeyboardModifiers modifiers)
{
	const std::optional<WORD> key_vk = qt_key_to_virtual_key(key);
	if (!key_vk.has_value())
		return {SyntheticKeypressStatus::UnsupportedKey, "key is not mapped to Win32 virtual key"};

	std::vector<WORD> modifier_keys;
	if (modifiers.testFlag(Qt::ShiftModifier))
		modifier_keys.push_back(VK_SHIFT);
	if (modifiers.testFlag(Qt::ControlModifier))
		modifier_keys.push_back(VK_CONTROL);
	if (modifiers.testFlag(Qt::AltModifier))
		modifier_keys.push_back(VK_MENU);
	if (modifiers.testFlag(Qt::MetaModifier))
		modifier_keys.push_back(VK_LWIN);

	std::vector<INPUT> events;
	events.reserve(modifier_keys.size() * 2 + 2);

	for (WORD modifier_vk : modifier_keys)
		append_key_event(&events, modifier_vk, true);
	append_key_event(&events, key_vk.value(), true);
	append_key_event(&events, key_vk.value(), false);
	for (auto it = modifier_keys.rbegin(); it != modifier_keys.rend(); ++it)
		append_key_event(&events, *it, false);

	if (events.empty())
		return {SyntheticKeypressStatus::SystemFailure, "failed to build input event list"};

	const UINT sent = SendInput(static_cast<UINT>(events.size()), events.data(), sizeof(INPUT));
	if (sent != events.size())
		return {SyntheticKeypressStatus::SystemFailure, QString("SendInput sent %1/%2 events").arg(sent).arg(events.size())};

	return {SyntheticKeypressStatus::Success, "ok"};
}

} // namespace bm::detail
