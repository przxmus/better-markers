#pragma once

#include "bm-marker-export-sink.hpp"
#include "bm-mp4-mov-embed-engine.hpp"
#include "bm-recovery-queue.hpp"
#include "bm-xmp-sidecar-writer.hpp"

#include <util/base.h>

#include <QFile>

namespace bm {

class PremiereXmpSink : public MarkerExportSink {
public:
	explicit PremiereXmpSink(const QString &queue_path);

	QString sink_name() const override;
	bool on_marker_added(const MarkerExportRecordingContext &recording_ctx, const MarkerRecord &marker,
			     const QVector<MarkerRecord> &full_marker_list, QString *error) override;
	bool on_recording_closed(const MarkerExportRecordingContext &recording_ctx, QString *error) override;

	void retry_recovery_queue();

private:
	static constexpr int kFinalizeRetryAttempts = 8;
	static constexpr int kFinalizeRetryInitialDelayMs = 120;
	static constexpr int kFinalizeRetryMaxDelayMs = 2000;
	static constexpr int kRecoveryRetryAttempts = 4;
	static constexpr int kRecoveryRetryInitialDelayMs = 200;
	static constexpr int kRecoveryRetryMaxDelayMs = 2000;

	XmpSidecarWriter m_xmp_writer;
	Mp4MovEmbedEngine m_embed_engine;
	RecoveryQueue m_recovery;
};

inline PremiereXmpSink::PremiereXmpSink(const QString &queue_path)
{
	m_recovery.set_queue_path(queue_path);
	m_recovery.load();
}

inline QString PremiereXmpSink::sink_name() const
{
	return "premiere-xmp";
}

inline bool PremiereXmpSink::on_marker_added(const MarkerExportRecordingContext &recording_ctx, const MarkerRecord &,
					     const QVector<MarkerRecord> &full_marker_list, QString *error)
{
	return m_xmp_writer.write_sidecar(recording_ctx.media_path, full_marker_list, recording_ctx.fps_num,
					  recording_ctx.fps_den, error);
}

inline bool PremiereXmpSink::on_recording_closed(const MarkerExportRecordingContext &recording_ctx, QString *error)
{
	if (!is_mp4_or_mov_path(recording_ctx.media_path))
		return true;

	const QString sidecar = XmpSidecarWriter::sidecar_path_for_media(recording_ctx.media_path);
	if (!QFile::exists(sidecar))
		return true;

	const EmbedResult result =
		m_embed_engine.embed_from_sidecar_with_retry(recording_ctx.media_path, sidecar, kFinalizeRetryAttempts,
							     kFinalizeRetryInitialDelayMs, kFinalizeRetryMaxDelayMs);
	if (result.ok) {
		m_recovery.remove(recording_ctx.media_path);
		m_recovery.save();
		blog(LOG_INFO, "[better-markers][%s] embedded XMP into '%s'", sink_name().toUtf8().constData(),
		     recording_ctx.media_path.toUtf8().constData());
		return true;
	}

	blog(LOG_WARNING, "[better-markers][%s] XMP embed retries exhausted for '%s': %s",
	     sink_name().toUtf8().constData(), recording_ctx.media_path.toUtf8().constData(),
	     result.error.toUtf8().constData());
	m_recovery.upsert(recording_ctx.media_path, result.error);
	m_recovery.save();
	if (error)
		*error = result.error;
	return false;
}

inline void PremiereXmpSink::retry_recovery_queue()
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

		const EmbedResult result = m_embed_engine.embed_from_sidecar_with_retry(job.media_path, sidecar,
											kRecoveryRetryAttempts,
											kRecoveryRetryInitialDelayMs,
											kRecoveryRetryMaxDelayMs);
		if (result.ok)
			m_recovery.remove(job.media_path);
		else
			m_recovery.upsert(job.media_path, result.error);
	}
	m_recovery.save();
}

} // namespace bm
