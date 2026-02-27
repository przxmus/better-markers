#include "bm-synthetic-keypress.hpp"

#import <CoreGraphics/CGEvent.h>
#import <Carbon/Carbon.h>

#include <optional>
#include <vector>

namespace bm::detail {
    namespace {

        std::optional<CGKeyCode> qt_key_to_mac_key_code(Qt::Key key)
        {
            if (key >= Qt::Key_A && key <= Qt::Key_Z) {
                static const CGKeyCode letter_map[] = {kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C, kVK_ANSI_D, kVK_ANSI_E,
                                                       kVK_ANSI_F, kVK_ANSI_G, kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J,
                                                       kVK_ANSI_K, kVK_ANSI_L, kVK_ANSI_M, kVK_ANSI_N, kVK_ANSI_O,
                                                       kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S, kVK_ANSI_T,
                                                       kVK_ANSI_U, kVK_ANSI_V, kVK_ANSI_W, kVK_ANSI_X, kVK_ANSI_Y,
                                                       kVK_ANSI_Z};
                return letter_map[key - Qt::Key_A];
            }

            if (key >= Qt::Key_0 && key <= Qt::Key_9) {
                static const CGKeyCode digit_map[] = {kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3, kVK_ANSI_4,
                                                      kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8, kVK_ANSI_9};
                return digit_map[key - Qt::Key_0];
            }

            if (key >= Qt::Key_F1 && key <= Qt::Key_F20)
                return static_cast<CGKeyCode>(kVK_F1 + (key - Qt::Key_F1));

            switch (key) {
                case Qt::Key_Escape:
                    return kVK_Escape;
                case Qt::Key_Tab:
                    return kVK_Tab;
                case Qt::Key_Backspace:
                    return kVK_Delete;
                case Qt::Key_Return:
                case Qt::Key_Enter:
                    return kVK_Return;
                case Qt::Key_Space:
                    return kVK_Space;
                case Qt::Key_Insert:
                    return kVK_Help;
                case Qt::Key_Delete:
                    return kVK_ForwardDelete;
                case Qt::Key_Home:
                    return kVK_Home;
                case Qt::Key_End:
                    return kVK_End;
                case Qt::Key_PageUp:
                    return kVK_PageUp;
                case Qt::Key_PageDown:
                    return kVK_PageDown;
                case Qt::Key_Left:
                    return kVK_LeftArrow;
                case Qt::Key_Right:
                    return kVK_RightArrow;
                case Qt::Key_Up:
                    return kVK_UpArrow;
                case Qt::Key_Down:
                    return kVK_DownArrow;
                case Qt::Key_Control:
                    return kVK_Control;
                case Qt::Key_Shift:
                    return kVK_Shift;
                case Qt::Key_Alt:
                    return kVK_Option;
                case Qt::Key_Meta:
                    return kVK_Command;
                case Qt::Key_Minus:
                    return kVK_ANSI_Minus;
                case Qt::Key_Equal:
                    return kVK_ANSI_Equal;
                case Qt::Key_BracketLeft:
                    return kVK_ANSI_LeftBracket;
                case Qt::Key_BracketRight:
                    return kVK_ANSI_RightBracket;
                case Qt::Key_Backslash:
                    return kVK_ANSI_Backslash;
                case Qt::Key_Semicolon:
                    return kVK_ANSI_Semicolon;
                case Qt::Key_Apostrophe:
                    return kVK_ANSI_Quote;
                case Qt::Key_Comma:
                    return kVK_ANSI_Comma;
                case Qt::Key_Period:
                    return kVK_ANSI_Period;
                case Qt::Key_Slash:
                    return kVK_ANSI_Slash;
                case Qt::Key_QuoteLeft:
                    return kVK_ANSI_Grave;
                default:
                    break;
            }

            return std::nullopt;
        }

        std::optional<CGKeyCode> modifier_to_mac_key_code(Qt::KeyboardModifier modifier)
        {
            switch (modifier) {
                case Qt::ShiftModifier:
                    return kVK_Shift;
                case Qt::ControlModifier:
                    return kVK_Control;
                case Qt::AltModifier:
                    return kVK_Option;
                case Qt::MetaModifier:
                    return kVK_Command;
                default:
                    return std::nullopt;
            }
        }

        bool post_keyboard_event(CGKeyCode key_code, bool key_down)
        {
            CGEventRef event = CGEventCreateKeyboardEvent(nullptr, key_code, key_down);
            if (!event)
                return false;
            CGEventPost(kCGHIDEventTap, event);
            CFRelease(event);
            return true;
        }

    }  // namespace

    SyntheticKeypressResult send_platform_synthetic_keypress(Qt::Key key, Qt::KeyboardModifiers modifiers)
    {
        if (!CGPreflightPostEventAccess())
            return {SyntheticKeypressStatus::PermissionDenied, "event synthesizing access denied"};

        const std::optional<CGKeyCode> key_code = qt_key_to_mac_key_code(key);
        if (!key_code.has_value())
            return {SyntheticKeypressStatus::UnsupportedKey, "key is not mapped to macOS key code"};

        std::vector<CGKeyCode> modifier_codes;
        if (modifiers.testFlag(Qt::ShiftModifier))
            modifier_codes.push_back(modifier_to_mac_key_code(Qt::ShiftModifier).value());
        if (modifiers.testFlag(Qt::ControlModifier))
            modifier_codes.push_back(modifier_to_mac_key_code(Qt::ControlModifier).value());
        if (modifiers.testFlag(Qt::AltModifier))
            modifier_codes.push_back(modifier_to_mac_key_code(Qt::AltModifier).value());
        if (modifiers.testFlag(Qt::MetaModifier))
            modifier_codes.push_back(modifier_to_mac_key_code(Qt::MetaModifier).value());

        for (CGKeyCode modifier_code : modifier_codes) {
            if (!post_keyboard_event(modifier_code, true))
                return {SyntheticKeypressStatus::SystemFailure, "failed to post modifier key down event"};
        }

        if (!post_keyboard_event(key_code.value(), true))
            return {SyntheticKeypressStatus::SystemFailure, "failed to post main key down event"};
        if (!post_keyboard_event(key_code.value(), false))
            return {SyntheticKeypressStatus::SystemFailure, "failed to post main key up event"};

        for (auto it = modifier_codes.rbegin(); it != modifier_codes.rend(); ++it) {
            if (!post_keyboard_event(*it, false))
                return {SyntheticKeypressStatus::SystemFailure, "failed to post modifier key up event"};
        }

        return {SyntheticKeypressStatus::Success, "ok"};
    }

}  // namespace bm::detail
