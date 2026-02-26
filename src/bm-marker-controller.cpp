#include "bm-marker-controller.hpp"

#include "bm-localization.hpp"
#include "bm-marker-dialog.hpp"

#include <util/base.h>
#include <util/platform.h>

#include <QFile>
#include <QMetaObject>
#include <QMessageBox>
#include <QUuid>

namespace bm {

MarkerController::MarkerController(ScopeStore *store, RecordingSessionTracker *tracker, QWidget *parent_window,
				   const QString &base_store_dir)
	: m_store(store), m_tracker(tracker), m_parent_window(parent_window)
{
	m_recovery.set_queue_path(base_store_dir + "/pending-embed.json");
	m_recovery.load();
}

void MarkerController::set_active_templates(const QVector<MarkerTemplate> &templates)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_active_templates = templates;
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
	if (dialog.exec() != QDialog::Accepted)
		return;

	const MarkerRecord marker = marker_from_inputs(ctx, dialog.marker_title(), dialog.marker_description(),
					       dialog.marker_color_id());
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
		QVector<MarkerTemplate> templates{templ};
		MarkerDialog dialog(templates, MarkerDialog::Mode::FixedTemplate, templ.id, m_parent_window);
		if (dialog.exec() != QDialog::Accepted)
			return;

		title = dialog.marker_title();
		description = dialog.marker_description();
		color_id = dialog.marker_color_id();
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

	QVector<MarkerTemplate> none;
	MarkerDialog dialog(none, MarkerDialog::Mode::NoTemplate, QString(), m_parent_window);
	if (dialog.exec() != QDialog::Accepted)
		return;

	const MarkerRecord marker = marker_from_inputs(ctx, dialog.marker_title(), dialog.marker_description(),
					       dialog.marker_color_id());
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
	const QVector<PendingEmbedJob> jobs = m_recovery.jobs();
	for (const PendingEmbedJob &job : jobs) {
		if (!QFile::exists(job.media_path))
			continue;
		if (!is_mp4_or_mov_path(job.media_path))
			continue;

		const QString sidecar = XmpSidecarWriter::sidecar_path_for_media(job.media_path);
		if (!QFile::exists(sidecar))
			continue;

		const EmbedResult result = m_embed_engine.embed_from_sidecar(job.media_path, sidecar);
		if (result.ok)
			m_recovery.remove(job.media_path);
		else
			m_recovery.upsert(job.media_path, result.error);
	}
	m_recovery.save();
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

void MarkerController::append_marker(const QString &media_path, const MarkerRecord &marker)
{
	QVector<MarkerRecord> markers;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_markers_by_file[media_path].push_back(marker);
		markers = m_markers_by_file.value(media_path);
	}

	QString error;
	const uint32_t fps_num = m_tracker ? m_tracker->fps_num() : 30;
	const uint32_t fps_den = m_tracker ? m_tracker->fps_den() : 1;
	if (!m_xmp_writer.write_sidecar(media_path, markers, fps_num, fps_den, &error)) {
		blog(LOG_ERROR, "[better-markers] failed to write sidecar for '%s': %s", media_path.toUtf8().constData(),
			error.toUtf8().constData());
		show_warning_async(bm_text("BetterMarkers.Warning.FailedToWriteSidecar").arg(error));
		return;
	}

	blog(LOG_INFO,
		"[better-markers] marker added: file=%s frame=%lld color=%d title='%s'",
		media_path.toUtf8().constData(), static_cast<long long>(marker.start_frame), marker.color_id,
		marker.name.toUtf8().constData());
}

void MarkerController::finalize_closed_file(const QString &closed_file)
{
	if (closed_file.isEmpty())
		return;

	if (!is_mp4_or_mov_path(closed_file)) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_markers_by_file.remove(closed_file);
		return;
	}

	const QString sidecar = XmpSidecarWriter::sidecar_path_for_media(closed_file);
	if (!QFile::exists(sidecar)) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_markers_by_file.remove(closed_file);
		return;
	}

	const EmbedResult result = m_embed_engine.embed_from_sidecar(closed_file, sidecar);
	if (result.ok) {
		m_recovery.remove(closed_file);
		m_recovery.save();
		blog(LOG_INFO, "[better-markers] embedded XMP into '%s'", closed_file.toUtf8().constData());
	} else {
		m_recovery.upsert(closed_file, result.error);
		m_recovery.save();
		blog(LOG_WARNING,
			"[better-markers] failed to embed XMP into '%s': %s (sidecar kept, recovery queued)",
			closed_file.toUtf8().constData(), result.error.toUtf8().constData());
		show_warning_async(
			bm_text("BetterMarkers.Warning.FailedToEmbedXmp").arg(result.error));
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	m_markers_by_file.remove(closed_file);
}

void MarkerController::show_warning_async(const QString &message) const
{
	if (!m_parent_window)
		return;

	QMetaObject::invokeMethod(m_parent_window,
				  [parent = m_parent_window, message]() {
					  QMessageBox::warning(parent, bm_text("BetterMarkers.DockTitle"), message);
				  },
				  Qt::QueuedConnection);
}

bool MarkerController::template_has_editables(const MarkerTemplate &templ)
{
	return templ.editable_title || templ.editable_description || templ.editable_color;
}

} // namespace bm
