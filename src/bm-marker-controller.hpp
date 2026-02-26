#pragma once

#include "bm-marker-data.hpp"
#include "bm-marker-export-sink.hpp"
#include "bm-final-cut-fcpxml-sink.hpp"
#include "bm-premiere-xmp-sink.hpp"
#include "bm-recording-session-tracker.hpp"
#include "bm-resolve-fcpxml-sink.hpp"
#include "bm-scope-store.hpp"

#include <QHash>
#include <QVector>

#include <atomic>
#include <mutex>

class QWidget;

namespace bm {

class MarkerDialog;

class MarkerController {
public:
	MarkerController(ScopeStore *store, RecordingSessionTracker *tracker, QWidget *parent_window,
			 const QString &base_store_dir);

	void set_active_templates(const QVector<MarkerTemplate> &templates);
	void set_export_profile(const ExportProfile &profile);
	void set_export_sinks(const QVector<MarkerExportSink *> &sinks);

	void add_marker_from_main_button();
	void add_marker_from_template_hotkey(const MarkerTemplate &templ);
	void quick_marker();
	void quick_custom_marker();

	void on_recording_file_changed(const QString &closed_file, const QString &next_file);
	void on_recording_stopped(const QString &closed_file);
	void retry_recovery_queue();
	void set_shutting_down(bool shutting_down);

private:
	bool capture_pending_context(PendingMarkerContext *out_ctx, bool show_warning_ui) const;
	MarkerRecord marker_from_inputs(const PendingMarkerContext &ctx, const QString &title,
					const QString &description, int color_id) const;
	void prepare_marker_dialog(MarkerDialog *dialog) const;

	void append_marker(const QString &media_path, const MarkerRecord &marker);
	void finalize_closed_file(const QString &closed_file);
	MarkerExportRecordingContext make_recording_context(const QString &media_path) const;
	bool dispatch_marker_added(const MarkerExportRecordingContext &ctx, const MarkerRecord &marker,
				   const QVector<MarkerRecord> &full_marker_list, QString *error);
	bool dispatch_recording_closed(const MarkerExportRecordingContext &ctx, QString *error);
	void show_warning_async(const QString &message) const;

	static bool template_has_editables(const MarkerTemplate &templ);

	ScopeStore *m_store = nullptr;
	RecordingSessionTracker *m_tracker = nullptr;
	QWidget *m_parent_window = nullptr;

	PremiereXmpSink m_premiere_xmp_sink;
	ResolveFcpxmlSink m_resolve_fcpxml_sink;
	FinalCutFcpxmlSink m_final_cut_fcpxml_sink;

	mutable std::mutex m_mutex;
	QVector<MarkerTemplate> m_active_templates;
	QVector<MarkerExportSink *> m_export_sinks;
	QHash<QString, QVector<MarkerRecord>> m_markers_by_file;
	std::atomic_bool m_shutting_down{false};
};

} // namespace bm
