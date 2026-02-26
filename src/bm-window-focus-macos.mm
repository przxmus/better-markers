#include "bm-window-focus.hpp"

#include <obs-module.h>

#include <QWidget>

#import <AppKit/AppKit.h>

namespace bm::detail {

namespace {

NSWindow *nswindow_from_widget(QWidget *widget)
{
	if (!widget)
		return nil;

	NSView *view = (__bridge NSView *)reinterpret_cast<void *>(widget->winId());
	if (!view)
		return nil;

	return view.window;
}

} // namespace

WindowFocusSnapshot capture_platform_window_focus_snapshot()
{
	NSRunningApplication *frontmost = [[NSWorkspace sharedWorkspace] frontmostApplication];
	if (!frontmost) {
		blog(LOG_DEBUG, "[better-markers][focus][mac] foreground capture failed: no frontmost app");
		return {};
	}

	const pid_t pid = frontmost.processIdentifier;
	if (pid <= 0) {
		blog(LOG_DEBUG, "[better-markers][focus][mac] foreground capture failed: invalid PID");
		return {};
	}

	blog(LOG_DEBUG, "[better-markers][focus][mac] captured frontmost pid=%d", static_cast<int>(pid));
	return {WindowFocusSnapshotKind::MacProcessId, static_cast<quint64>(pid)};
}

bool activate_platform_marker_dialog_window(QWidget *dialog)
{
	if (!dialog)
		return false;

	[NSApp activateIgnoringOtherApps:YES];

	NSWindow *dialog_window = nswindow_from_widget(dialog->window());
	if (!dialog_window) {
		blog(LOG_DEBUG, "[better-markers][focus][mac] dialog activation skipped: missing NSWindow");
		return false;
	}

	if (dialog_window.miniaturized)
		[dialog_window deminiaturize:nil];
	[dialog_window makeKeyAndOrderFront:nil];

	const bool active = dialog_window.keyWindow || dialog_window.mainWindow;
	blog(LOG_DEBUG, "[better-markers][focus][mac] dialog activation result=%s", active ? "success" : "failure");
	return active;
}

bool restore_platform_window_focus(const WindowFocusSnapshot &snapshot)
{
	if (snapshot.kind != WindowFocusSnapshotKind::MacProcessId || snapshot.value == 0)
		return false;

	const pid_t pid = static_cast<pid_t>(snapshot.value);
	NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
	if (!app) {
		blog(LOG_DEBUG, "[better-markers][focus][mac] restore skipped: PID %d no longer running",
		     static_cast<int>(pid));
		return false;
	}

	const bool restored =
		[app activateWithOptions:(NSApplicationActivateAllWindows | NSApplicationActivateIgnoringOtherApps)];
	blog(LOG_DEBUG, "[better-markers][focus][mac] restore pid=%d result=%s", static_cast<int>(pid),
	     restored ? "success" : "failure");
	return restored;
}

} // namespace bm::detail
