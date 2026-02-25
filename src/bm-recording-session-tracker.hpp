#pragma once

#include <obs-frontend-api.h>
#include <obs.h>

#include <atomic>
#include <functional>
#include <mutex>

#include <QString>

namespace bm {

class RecordingSessionTracker {
public:
	using FileChangedCallback = std::function<void(const QString &closed_file, const QString &next_file)>;
	using RecordingStoppedCallback = std::function<void(const QString &closed_file)>;

	RecordingSessionTracker() = default;
	~RecordingSessionTracker();

	void set_file_changed_callback(FileChangedCallback cb);
	void set_recording_stopped_callback(RecordingStoppedCallback cb);

	void sync_from_frontend_state();
	void handle_frontend_event(enum obs_frontend_event event);
	void shutdown();

	bool is_recording_active() const;
	bool is_recording_paused() const;
	bool can_add_marker() const;

	QString current_media_path() const;
	uint32_t fps_num() const;
	uint32_t fps_den() const;

	int64_t capture_frame_now() const;

private:
	static void packet_callback(obs_output_t *output, struct encoder_packet *pkt, struct encoder_packet_time *pkt_time,
				    void *param);
	static void file_changed_signal(void *param, calldata_t *data);

	void on_recording_started();
	void on_recording_stopped();
	void on_recording_paused(bool paused);

	void attach_output_hooks();
	void detach_output_hooks();
	QString query_current_recording_path() const;

	mutable std::mutex m_mutex;
	bool m_recording_active = false;
	bool m_recording_paused = false;
	uint64_t m_recording_start_ns = 0;
	uint64_t m_pause_start_ns = 0;
	uint64_t m_total_paused_ns = 0;
	uint32_t m_fps_num = 30;
	uint32_t m_fps_den = 1;
	QString m_current_media_path;

	std::atomic<int64_t> m_latest_video_dts_usec{-1};

	obs_output_t *m_output = nullptr;
	FileChangedCallback m_file_changed_cb;
	RecordingStoppedCallback m_recording_stopped_cb;
};

} // namespace bm
