#pragma once

#include "bm-marker-export-sink.hpp"
#include "bm-mp4-mov-embed-engine.hpp"
#include "bm-recovery-queue.hpp"
#include "bm-startup-recovery-policy.hpp"
#include "bm-xmp-sidecar-writer.hpp"

#include <util/base.h>
#include <util/platform.h>

#include <QFile>
#include <QVector>

#include <atomic>
#include <mutex>
#include <thread>

namespace bm {

class PremiereXmpSink : public MarkerExportSink {
public:
	explicit PremiereXmpSink(const QString &queue_path);
	~PremiereXmpSink() override;

	QString sink_name() const override;
	bool on_marker_added(const MarkerExportRecordingContext &recording_ctx, const MarkerRecord &marker,
			     const QVector<MarkerRecord> &full_marker_list, QString *error) override;
	bool on_recording_closed(const MarkerExportRecordingContext &recording_ctx, QString *error) override;

	void start_startup_recovery_async();
	void stop_startup_recovery();

private:
	static constexpr int kFinalizeRetryAttempts = 8;
	static constexpr int kFinalizeRetryInitialDelayMs = 120;
	static constexpr int kFinalizeRetryMaxDelayMs = 2000;

	bool load_recovery_queue_locked();
	bool save_recovery_queue_locked();
	void run_startup_recovery_worker();
	void remove_job_and_save_locked(const QString &media_path);
	void upsert_job_and_save_locked(const QString &media_path, const QString &last_error);

	XmpSidecarWriter m_xmp_writer;
	Mp4MovEmbedEngine m_embed_engine;
	RecoveryQueue m_recovery;
	std::mutex m_recovery_mutex;
	std::mutex m_embed_mutex;
	std::thread m_startup_recovery_thread;
	std::atomic_bool m_stop_startup_recovery{false};
	std::atomic_bool m_startup_recovery_running{false};
};

inline PremiereXmpSink::PremiereXmpSink(const QString &queue_path)
{
	std::lock_guard<std::mutex> lock(m_recovery_mutex);
	m_recovery.set_queue_path(queue_path);
	if (!load_recovery_queue_locked())
		blog(LOG_WARNING, "[better-markers][%s] failed to load recovery queue '%s'",
		     sink_name().toUtf8().constData(), queue_path.toUtf8().constData());
}

inline PremiereXmpSink::~PremiereXmpSink()
{
	stop_startup_recovery();
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

	EmbedResult result;
	{
		std::lock_guard<std::mutex> embed_lock(m_embed_mutex);
		result = m_embed_engine.embed_from_sidecar_with_retry(recording_ctx.media_path, sidecar, kFinalizeRetryAttempts,
								      kFinalizeRetryInitialDelayMs,
								      kFinalizeRetryMaxDelayMs);
	}
	if (result.ok) {
		std::lock_guard<std::mutex> lock(m_recovery_mutex);
		remove_job_and_save_locked(recording_ctx.media_path);
		blog(LOG_INFO, "[better-markers][%s] embedded XMP into '%s'", sink_name().toUtf8().constData(),
		     recording_ctx.media_path.toUtf8().constData());
		return true;
	}

	blog(LOG_WARNING, "[better-markers][%s] XMP embed retries exhausted for '%s': %s",
	     sink_name().toUtf8().constData(), recording_ctx.media_path.toUtf8().constData(),
	     result.error.toUtf8().constData());
	{
		std::lock_guard<std::mutex> lock(m_recovery_mutex);
		upsert_job_and_save_locked(recording_ctx.media_path, result.error);
	}
	if (error)
		*error = result.error;
	return false;
}

inline void PremiereXmpSink::start_startup_recovery_async()
{
	bool expected = false;
	if (!m_startup_recovery_running.compare_exchange_strong(expected, true)) {
		blog(LOG_INFO, "[better-markers][%s] startup recovery already running; skipping duplicate start",
		     sink_name().toUtf8().constData());
		return;
	}

	m_stop_startup_recovery.store(false);
	if (m_startup_recovery_thread.joinable())
		m_startup_recovery_thread.join();
	m_startup_recovery_thread = std::thread([this]() { run_startup_recovery_worker(); });
}

inline void PremiereXmpSink::stop_startup_recovery()
{
	m_stop_startup_recovery.store(true);
	if (m_startup_recovery_thread.joinable())
		m_startup_recovery_thread.join();
	m_startup_recovery_running.store(false);
}

inline bool PremiereXmpSink::load_recovery_queue_locked()
{
	return m_recovery.load();
}

inline bool PremiereXmpSink::save_recovery_queue_locked()
{
	return m_recovery.save();
}

inline void PremiereXmpSink::remove_job_and_save_locked(const QString &media_path)
{
	m_recovery.remove(media_path);
	if (!save_recovery_queue_locked())
		blog(LOG_WARNING, "[better-markers][%s] failed to persist recovery queue after remove",
		     sink_name().toUtf8().constData());
}

inline void PremiereXmpSink::upsert_job_and_save_locked(const QString &media_path, const QString &last_error)
{
	m_recovery.upsert(media_path, last_error);
	if (!save_recovery_queue_locked())
		blog(LOG_WARNING, "[better-markers][%s] failed to persist recovery queue after upsert",
		     sink_name().toUtf8().constData());
}

inline void PremiereXmpSink::run_startup_recovery_worker()
{
	const uint64_t begin_ns = os_gettime_ns();
	QVector<PendingEmbedJob> jobs;
	{
		std::lock_guard<std::mutex> lock(m_recovery_mutex);
		jobs = m_recovery.jobs();
	}
	blog(LOG_INFO, "[better-markers][%s] startup recovery begin: jobs=%lld", sink_name().toUtf8().constData(),
	     static_cast<long long>(jobs.size()));

	for (const PendingEmbedJob &job : jobs) {
		if (m_stop_startup_recovery.load())
			break;

		const uint64_t job_begin_ns = os_gettime_ns();
		const StartupRecoveryDecision decision = decide_startup_recovery(job.media_path);
		if (decision.action != StartupRecoveryAction::RetryOnce) {
			{
				std::lock_guard<std::mutex> lock(m_recovery_mutex);
				remove_job_and_save_locked(job.media_path);
			}
			blog(LOG_INFO,
			     "[better-markers][%s] startup recovery dropped stale job: '%s' action=%s (%llu ms)",
			     sink_name().toUtf8().constData(), job.media_path.toUtf8().constData(),
			     startup_recovery_action_name(decision.action),
			     static_cast<unsigned long long>((os_gettime_ns() - job_begin_ns) / 1000000ULL));
			continue;
		}

		EmbedResult result;
		{
			std::lock_guard<std::mutex> embed_lock(m_embed_mutex);
			result = m_embed_engine.embed_from_sidecar_with_retry(job.media_path, decision.sidecar_path,
									      startup_recovery_retry_attempts(),
									      0, 0);
		}
		if (result.ok) {
			{
				std::lock_guard<std::mutex> lock(m_recovery_mutex);
				remove_job_and_save_locked(job.media_path);
			}
			blog(LOG_INFO, "[better-markers][%s] startup recovery success: '%s' (%llu ms)",
			     sink_name().toUtf8().constData(), job.media_path.toUtf8().constData(),
			     static_cast<unsigned long long>((os_gettime_ns() - job_begin_ns) / 1000000ULL));
		} else {
			{
				std::lock_guard<std::mutex> lock(m_recovery_mutex);
				upsert_job_and_save_locked(job.media_path, result.error);
			}
			blog(LOG_WARNING, "[better-markers][%s] startup recovery failed: '%s' error='%s' (%llu ms)",
			     sink_name().toUtf8().constData(), job.media_path.toUtf8().constData(),
			     result.error.toUtf8().constData(),
			     static_cast<unsigned long long>((os_gettime_ns() - job_begin_ns) / 1000000ULL));
		}
	}

	m_startup_recovery_running.store(false);
	blog(LOG_INFO, "[better-markers][%s] startup recovery complete (%llu ms)", sink_name().toUtf8().constData(),
	     static_cast<unsigned long long>((os_gettime_ns() - begin_ns) / 1000000ULL));
}

} // namespace bm
