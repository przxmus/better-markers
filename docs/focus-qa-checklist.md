# Focus QA Checklist

Use this checklist to validate marker dialog focus behavior after installation.

## Windows 10/11 (priority)

1. Start recording in OBS.
2. Open another app (for example a text editor) and keep it focused.
3. Trigger a hotkey that opens marker dialog (`Quick Custom Marker` or editable template hotkey).
4. Confirm marker dialog is immediately focused and ready for typing.
5. Confirm `OK` returns focus to the previously focused app.
6. Repeat and cancel with:
   - `Esc`
   - `Cancel` button
   - system window close button (`X`)
7. Confirm every cancel path returns focus to the previous app.
8. Minimize OBS, keep another app focused, and trigger the same hotkey.
9. Confirm OBS is restored and marker dialog becomes focused.

## macOS

1. Start recording and focus another app.
2. Trigger a hotkey that opens marker dialog.
3. Confirm marker dialog becomes key window.
4. Confirm `OK` and all cancel paths return focus to previous app.

## Linux X11

1. Start recording and focus another app.
2. Trigger a hotkey that opens marker dialog.
3. Confirm marker dialog becomes focused.
4. Confirm `OK` and all cancel paths return focus to previous app.

## Linux Wayland

1. Start recording and focus another app.
2. Trigger a hotkey that opens marker dialog.
3. Confirm there are no crashes or hangs.
4. Confirm best-effort focus behavior (compositor may block forced activation).

## Non-regression checks

1. Trigger `Quick Marker` (no dialog) and confirm behavior is unchanged.
2. Use `Add Marker` button in Better Markers dock and confirm behavior is unchanged.
3. Disable `Auto-focus marker dialog` in settings and verify no auto focus/unfocus occurs.
