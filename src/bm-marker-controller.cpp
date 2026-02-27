#include "bm-marker-controller.hpp"

#include "bm-focus-policy.hpp"
#include "bm-localization.hpp"
#include "bm-marker-dialog.hpp"
#include "bm-synthetic-keypress.hpp"
#include "bm-window-focus.hpp"

#include <obs-frontend-api.h>
#include <obs.h>
#include <util/base.h>
#include <util/platform.h>

#include <QFile>
#include <QCoreApplication>
#include <QMetaObject>
#include <QMessageBox>
#include <QPointer>
#include <QUuid>
#include <QWidget>

namespace bm {
namespace {

const char *synthetic_keypress_status_name(SyntheticKeypressStatus status)
{
	switch (status) {
	case SyntheticKeypressStatus::Success:
		return "success";
	case SyntheticKeypressStatus::Empty:
		return "empty";
	case SyntheticKeypressStatus::InvalidSequence:
		return "invalid";
	case SyntheticKeypressStatus::UnsupportedPlatform:
		return "unsupported_platform";
	case SyntheticKeypressStatus::PermissionDenied:
		return "permission_denied";
	case SyntheticKeypressStatus::UnsupportedKey:
		return "unsupported_key";
	case SyntheticKeypressStatus::SystemFailure:
		return "system_failure";
	default:
		return "unknown";
	}
}

class HotkeyDialogFocusSession {
public:
	explicit HotkeyDialogFocusSession(const ScopeStore *store)
	{
		const bool auto_focus_enabled = store && store->auto_focus_marker_dialog();
		m_enabled = should_use_hotkey_focus_session(auto_focus_enabled, true, true);
		if (m_enabled)
			m_snapshot = capture_window_focus_snapshot();
	}

	void prepare_dialog(MarkerDialog *dialog) const
	{
		if (!m_enabled || !dialog)
			return;

		dialog->prepare_for_immediate_input(true);
		dialog->set_platform_activation_callback([dialog]() { return activate_marker_dialog_window(dialog); });
	}

	void restore() const
	{
		if (!should_restore_focus_after_dialog(m_enabled, true) || !m_snapshot.is_valid())
			return;

		restore_window_focus(m_snapshot);
	}

private:
	bool m_enabled = false;
	WindowFocusSnapshot m_snapshot;
};

class DialogReentryGuard {
public:
	explicit DialogReentryGuard(std::atomic_bool *flag) : m_flag(flag)
	{
		if (m_flag)
			m_acquired = !m_flag->exchange(true);
	}

	~DialogReentryGuard()
	{
		if (m_flag && m_acquired)
			m_flag->store(false);
	}

	bool acquired() const { return m_acquired; }

private:
	std::atomic_bool *m_flag = nullptr;
	bool m_acquired = false;
};

class DialogRecordingPauseSession {
public:
	explicit DialogRecordingPauseSession(const ScopeStore *store)
	{
		m_enabled = store && store->pause_recording_during_marker_dialog();
	}

	~DialogRecordingPauseSession() { resume_if_needed(); }

	void pause_if_needed()
	{
		if (!m_enabled || m_pause_attempted)
			return;
		m_pause_attempted = true;

		if (!obs_frontend_recording_active() || obs_frontend_recording_paused())
			return;

		obs_output_t *output = obs_frontend_get_recording_output();
		if (!output || !obs_output_can_pause(output)) {
			blog(LOG_DEBUG, "[better-markers] auto-pause skipped: recording output cannot be paused");
			return;
		}

		obs_frontend_recording_pause(true);
		m_paused_by_plugin = obs_frontend_recording_paused();
		if (!m_paused_by_plugin)
			blog(LOG_DEBUG, "[better-markers] auto-pause request resulted in no-op");
	}

	void resume_if_needed()
	{
		if (m_resume_attempted)
			return;
		m_resume_attempted = true;

		if (!m_paused_by_plugin)
			return;
		if (!obs_frontend_recording_active() || !obs_frontend_recording_paused())
			return;

		obs_frontend_recording_pause(false);
		if (obs_frontend_recording_paused())
			blog(LOG_DEBUG, "[better-markers] auto-unpause request resulted in no-op");
	}

private:
	bool m_enabled = false;
	bool m_pause_attempted = false;
	bool m_resume_attempted = false;
	bool m_paused_by_plugin = false;
};

} // namespace

MarkerController::MarkerController(ScopeStore *store, RecordingSessionTracker *tracker, QWidget *parent_window,
				   const QString &base_store_dir)
	: m_store(store),
	  m_tracker(tracker),
	  m_parent_window(parent_window),
	  m_premiere_xmp_sink(base_store_dir + "/pending-embed.json")
{
	set_export_profile(ExportProfile{});
}

void MarkerController::set_active_templates(const QVector<MarkerTemplate> &templates)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_active_templates = templates;
}

void MarkerController::set_export_sinks(const QVector<MarkerExportSink *> &sinks)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_export_sinks = sinks;
}

void MarkerController::set_export_profile(const ExportProfile &profile)
{
	QVector<MarkerExportSink *> sinks;
	if (profile.enable_premiere_xmp)
		sinks.push_back(&m_premiere_xmp_sink);
	if (profile.enable_resolve_fcpxml)
		sinks.push_back(&m_resolve_fcpxml_sink);
#ifdef __APPLE__
	if (profile.enable_final_cut_fcpxml)
		sinks.push_back(&m_final_cut_fcpxml_sink);
#endif
	set_export_sinks(sinks);
}

void MarkerController::add_marker_from_main_button()
{
	PendingMarkerContext ctx;
	if (!capture_pending_context(&ctx, true))
		return;

	QVector<MarkerTemplate> templates;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		templates = m_active_templates;
	}

	MarkerDialog dialog(templates, MarkerDialog::Mode::ChooseTemplate, QString(), m_parent_window);
	prepare_marker_dialog(&dialog);
	DialogRecordingPauseSession pause_session(m_store);
	pause_session.pause_if_needed();
	if (dialog.exec() != QDialog::Accepted) {
		pause_session.resume_if_needed();
		return;
	}
	pause_session.resume_if_needed();

	const MarkerRecord marker =
		marker_from_inputs(ctx, dialog.marker_title(), dialog.marker_description(), dialog.marker_color_id());
	append_marker(ctx.media_path, marker);
}

void MarkerController::add_marker_from_template_hotkey(const MarkerTemplate &templ)
{
	PendingMarkerContext ctx;
	if (!capture_pending_context(&ctx, false))
		return;

	QString title = templ.title;
	QString description = templ.description;
	int color_id = templ.color_id;

	if (template_has_editables(templ)) {
		DialogReentryGuard dialog_guard(&m_hotkey_dialog_open);
		if (!dialog_guard.acquired()) {
			blog(LOG_WARNING, "[better-markers] marker dialog already open; ignoring hotkey trigger");
			return;
		}

		HotkeyDialogFocusSession focus_session(m_store);
		DialogRecordingPauseSession pause_session(m_store);
		QVector<MarkerTemplate> templates{templ};
			MarkerDialog dialog(templates, MarkerDialog::Mode::FixedTemplate, templ.id, m_parent_window);
			focus_session.prepare_dialog(&dialog);
			pause_session.pause_if_needed();
			maybe_send_synthetic_keypress(true);
			if (dialog.exec() != QDialog::Accepted) {
				focus_session.restore();
				maybe_send_synthetic_keypress(false);
				pause_session.resume_if_needed();
				return;
			}

		title = dialog.marker_title();
			description = dialog.marker_description();
			color_id = dialog.marker_color_id();
			focus_session.restore();
			maybe_send_synthetic_keypress(false);
			pause_session.resume_if_needed();
		}

	const MarkerRecord marker = marker_from_inputs(ctx, title, description, color_id);
	append_marker(ctx.media_path, marker);
}

void MarkerController::quick_marker()
{
	PendingMarkerContext ctx;
	if (!capture_pending_context(&ctx, false))
		return;

	const MarkerRecord marker = marker_from_inputs(ctx, "", "", 0);
	append_marker(ctx.media_path, marker);
}

void MarkerController::quick_custom_marker()
{
	PendingMarkerContext ctx;
	if (!capture_pending_context(&ctx, false))
		return;

	DialogReentryGuard dialog_guard(&m_hotkey_dialog_open);
	if (!dialog_guard.acquired()) {
		blog(LOG_WARNING, "[better-markers] marker dialog already open; ignoring quick custom hotkey");
		return;
	}

	HotkeyDialogFocusSession focus_session(m_store);
	DialogRecordingPauseSession pause_session(m_store);
	QVector<MarkerTemplate> none;
	MarkerDialog dialog(none, MarkerDialog::Mode::NoTemplate, QString(), m_parent_window);
	focus_session.prepare_dialog(&dialog);
	pause_session.pause_if_needed();
	maybe_send_synthetic_keypress(true);
	if (dialog.exec() != QDialog::Accepted) {
		focus_session.restore();
		maybe_send_synthetic_keypress(false);
		pause_session.resume_if_needed();
		return;
	}

	const MarkerRecord marker =
		marker_from_inputs(ctx, dialog.marker_title(), dialog.marker_description(), dialog.marker_color_id());
	focus_session.restore();
	maybe_send_synthetic_keypress(false);
	pause_session.resume_if_needed();
	append_marker(ctx.media_path, marker);
}

void MarkerController::on_recording_file_changed(const QString &closed_file, const QString &next_file)
{
	Q_UNUSED(next_file);
	finalize_closed_file(closed_file);
}

void MarkerController::on_recording_stopped(const QString &closed_file)
{
	finalize_closed_file(closed_file);
}

void MarkerController::retry_recovery_queue()
{
	m_premiere_xmp_sink.retry_recovery_queue();
}

void MarkerController::set_shutting_down(bool shutting_down)
{
	m_shutting_down.store(shutting_down);
}

bool MarkerController::capture_pending_context(PendingMarkerContext *out_ctx, bool show_warning_ui) const
{
	if (!out_ctx)
		return false;
	if (!m_tracker)
		return false;

	if (!m_tracker->can_add_marker()) {
		if (show_warning_ui)
			show_warning_async(bm_text("BetterMarkers.Warning.RecordingRequired"));
		blog(LOG_WARNING, "[better-markers] marker ignored: recording is not active or is paused");
		return false;
	}

	out_ctx->frozen_frame = m_tracker->capture_frame_now();
	out_ctx->media_path = m_tracker->current_media_path();
	out_ctx->trigger_time_ns = os_gettime_ns();
	if (out_ctx->media_path.isEmpty()) {
		if (show_warning_ui)
			show_warning_async(bm_text("BetterMarkers.Warning.CouldNotResolvePath"));
		blog(LOG_WARNING, "[better-markers] marker ignored: current recording file path is empty");
		return false;
	}

	return true;
}

MarkerRecord MarkerController::marker_from_inputs(const PendingMarkerContext &ctx, const QString &title,
						  const QString &description, int color_id) const
{
	MarkerRecord marker;
	marker.start_frame = ctx.frozen_frame;
	marker.duration_frames = 0;
	marker.name = title;
	marker.comment = description;
	marker.type = "Comment";
	marker.guid = QUuid::createUuid().toString(QUuid::WithoutBraces);
	marker.color_id = color_id;
	return marker;
}

void MarkerController::prepare_marker_dialog(MarkerDialog *dialog) const
{
	if (!dialog || !m_store)
		return;

	if (!m_store->auto_focus_marker_dialog())
		return;

	dialog->prepare_for_immediate_input(true);
	if (m_parent_window) {
		if (QWidget *top_level = m_parent_window->window()) {
			top_level->raise();
			top_level->activateWindow();
		}
		m_parent_window->raise();
		m_parent_window->activateWindow();
	}
}

void MarkerController::maybe_send_synthetic_keypress(bool before_focus) const
{
	if (!m_store)
		return;
	if (!m_store->auto_focus_marker_dialog())
		return;
	if (!m_store->synthetic_keypress_around_focus_enabled())
		return;

	const QString portable = before_focus ? m_store->synthetic_keypress_before_focus_portable()
					      : m_store->synthetic_keypress_after_unfocus_portable();
	const SyntheticKeypressResult result = send_synthetic_keypress_portable(portable);
	if (result.status == SyntheticKeypressStatus::Success) {
		blog(LOG_DEBUG, "[better-markers] synthetic keypress %s succeeded",
		     before_focus ? "before-focus" : "after-unfocus");
		return;
	}

	if (result.status == SyntheticKeypressStatus::Empty) {
		blog(LOG_DEBUG, "[better-markers] synthetic keypress %s skipped: key not configured",
		     before_focus ? "before-focus" : "after-unfocus");
		return;
	}

	blog(LOG_WARNING, "[better-markers] synthetic keypress %s failed: status=%s reason=%s",
	     before_focus ? "before-focus" : "after-unfocus", synthetic_keypress_status_name(result.status),
	     result.technical_reason.toUtf8().constData());
	if (!m_synthetic_keypress_warning_shown.exchange(true))
		show_warning_async(bm_text("BetterMarkers.Warning.SyntheticKeypressFailed"));
}

void MarkerController::append_marker(const QString &media_path, const MarkerRecord &marker)
{
	QVector<MarkerRecord> markers;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_markers_by_file[media_path].push_back(marker);
		markers = m_markers_by_file.value(media_path);
	}

	const MarkerExportRecordingContext ctx = make_recording_context(media_path);
	QString error;
	if (!dispatch_marker_added(ctx, marker, markers, &error)) {
		blog(LOG_ERROR, "[better-markers] failed to export marker for '%s': %s",
		     media_path.toUtf8().constData(), error.toUtf8().constData());
		show_warning_async(bm_text("BetterMarkers.Warning.FailedToWriteSidecar").arg(error));
		return;
	}

	blog(LOG_INFO, "[better-markers] marker added: file=%s frame=%lld color=%d title='%s'",
	     media_path.toUtf8().constData(), static_cast<long long>(marker.start_frame), marker.color_id,
	     marker.name.toUtf8().constData());
}

void MarkerController::finalize_closed_file(const QString &closed_file)
{
	if (closed_file.isEmpty())
		return;

	const MarkerExportRecordingContext ctx = make_recording_context(closed_file);
	QString error;
	if (!dispatch_recording_closed(ctx, &error))
		show_warning_async(bm_text("BetterMarkers.Warning.FailedToEmbedXmp").arg(error));

	std::lock_guard<std::mutex> lock(m_mutex);
	m_markers_by_file.remove(closed_file);
}

MarkerExportRecordingContext MarkerController::make_recording_context(const QString &media_path) const
{
	MarkerExportRecordingContext ctx;
	ctx.media_path = media_path;
	ctx.fps_num = m_tracker ? m_tracker->fps_num() : 30;
	ctx.fps_den = m_tracker ? m_tracker->fps_den() : 1;
	return ctx;
}

bool MarkerController::dispatch_marker_added(const MarkerExportRecordingContext &ctx, const MarkerRecord &marker,
					     const QVector<MarkerRecord> &full_marker_list, QString *error)
{
	QVector<MarkerExportSink *> sinks;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		sinks = m_export_sinks;
	}
	for (MarkerExportSink *sink : sinks) {
		if (!sink)
			continue;
		QString sink_error;
		if (!sink->on_marker_added(ctx, marker, full_marker_list, &sink_error)) {
			blog(LOG_ERROR, "[better-markers][%s] marker export failed: %s",
			     sink->sink_name().toUtf8().constData(), sink_error.toUtf8().constData());
			if (error) {
				const QString prefix = error->isEmpty() ? QString() : QString("; ");
				*error += prefix + QString("%1: %2").arg(sink->sink_name(), sink_error);
			}
		}
	}
	return !error || error->isEmpty();
}

bool MarkerController::dispatch_recording_closed(const MarkerExportRecordingContext &ctx, QString *error)
{
	QVector<MarkerExportSink *> sinks;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		sinks = m_export_sinks;
	}
	for (MarkerExportSink *sink : sinks) {
		if (!sink)
			continue;
		QString sink_error;
		if (!sink->on_recording_closed(ctx, &sink_error)) {
			blog(LOG_WARNING, "[better-markers][%s] finalize export failed: %s",
			     sink->sink_name().toUtf8().constData(), sink_error.toUtf8().constData());
			if (error) {
				const QString prefix = error->isEmpty() ? QString() : QString("; ");
				*error += prefix + QString("%1: %2").arg(sink->sink_name(), sink_error);
			}
		}
	}
	return !error || error->isEmpty();
}

void MarkerController::show_warning_async(const QString &message) const
{
	if (m_shutting_down.load() || !m_parent_window)
		return;

	QCoreApplication *app = QCoreApplication::instance();
	QPointer<QWidget> parent(m_parent_window);
	if (!app || parent.isNull())
		return;

	QMetaObject::invokeMethod(
		app,
		[parent, message]() {
			if (parent.isNull())
				return;
			QMessageBox::warning(parent.data(), bm_text("BetterMarkers.DockTitle"), message);
		},
		Qt::QueuedConnection);
}

bool MarkerController::template_has_editables(const MarkerTemplate &templ)
{
	return templ.editable_title || templ.editable_description || templ.editable_color;
}

} // namespace bm
