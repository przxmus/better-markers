#include "bm-recording-session-tracker.hpp"

#include <util/base.h>
#include <util/platform.h>

namespace bm {

RecordingSessionTracker::~RecordingSessionTracker()
{
	shutdown();
}

void RecordingSessionTracker::set_file_changed_callback(FileChangedCallback cb)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_file_changed_cb = std::move(cb);
}

void RecordingSessionTracker::set_recording_stopped_callback(RecordingStoppedCallback cb)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_recording_stopped_cb = std::move(cb);
}

void RecordingSessionTracker::sync_from_frontend_state()
{
	if (obs_frontend_recording_active())
		on_recording_started();
}

void RecordingSessionTracker::handle_frontend_event(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		on_recording_started();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		on_recording_stopped();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		on_recording_paused(true);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		on_recording_paused(false);
		break;
	default:
		break;
	}
}

void RecordingSessionTracker::shutdown()
{
	detach_output_hooks();
}

bool RecordingSessionTracker::is_recording_active() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_recording_active;
}

bool RecordingSessionTracker::is_recording_paused() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_recording_paused;
}

bool RecordingSessionTracker::can_add_marker() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_recording_active && !m_recording_paused && !m_current_media_path.isEmpty();
}

QString RecordingSessionTracker::current_media_path() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_current_media_path;
}

uint32_t RecordingSessionTracker::fps_num() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_fps_num;
}

uint32_t RecordingSessionTracker::fps_den() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_fps_den;
}

int64_t RecordingSessionTracker::capture_frame_now() const
{
	const int64_t latest_dts_usec = m_latest_video_dts_usec.load();

	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_fps_den == 0)
		return 0;

	if (latest_dts_usec > 0) {
		const long double numerator = static_cast<long double>(latest_dts_usec) * m_fps_num;
		const long double denominator = static_cast<long double>(m_fps_den) * 1000000.0L;
		const auto frame = static_cast<int64_t>(numerator / denominator);
		return frame < 0 ? 0 : frame;
	}

	const uint64_t now_ns = os_gettime_ns();
	uint64_t paused_ns = m_total_paused_ns;
	if (m_recording_paused && m_pause_start_ns != 0 && now_ns > m_pause_start_ns)
		paused_ns += now_ns - m_pause_start_ns;

	if (now_ns <= m_recording_start_ns)
		return 0;

	const uint64_t active_ns = (now_ns - m_recording_start_ns > paused_ns) ? (now_ns - m_recording_start_ns - paused_ns)
										      : 0;
	const long double numerator = static_cast<long double>(active_ns) * m_fps_num;
	const long double denominator = static_cast<long double>(m_fps_den) * 1000000000.0L;
	const auto frame = static_cast<int64_t>(numerator / denominator);
	return frame < 0 ? 0 : frame;
}

void RecordingSessionTracker::packet_callback(obs_output_t *, struct encoder_packet *pkt, struct encoder_packet_time *,
					      void *param)
{
	auto *self = static_cast<RecordingSessionTracker *>(param);
	if (!self || !pkt)
		return;

	if (pkt->type == OBS_ENCODER_VIDEO)
		self->m_latest_video_dts_usec.store(pkt->dts_usec);
}

void RecordingSessionTracker::file_changed_signal(void *param, calldata_t *data)
{
	auto *self = static_cast<RecordingSessionTracker *>(param);
	if (!self)
		return;

	const char *next_file_c = calldata_string(data, "next_file");
	const QString next_file = QString::fromUtf8(next_file_c ? next_file_c : "");

	QString closed_file;
	FileChangedCallback callback;
	{
		std::lock_guard<std::mutex> lock(self->m_mutex);
		closed_file = self->m_current_media_path;
		self->m_current_media_path = next_file;
		callback = self->m_file_changed_cb;
	}

	self->m_latest_video_dts_usec.store(-1);

	if (callback && !closed_file.isEmpty())
		callback(closed_file, next_file);
}

void RecordingSessionTracker::on_recording_started()
{
	obs_video_info ovi{};
	if (!obs_get_video_info(&ovi)) {
		ovi.fps_num = 30;
		ovi.fps_den = 1;
	}

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_recording_active = true;
		m_recording_paused = obs_frontend_recording_paused();
		m_recording_start_ns = os_gettime_ns();
		m_pause_start_ns = m_recording_paused ? m_recording_start_ns : 0;
		m_total_paused_ns = 0;
		m_fps_num = ovi.fps_num == 0 ? 30 : ovi.fps_num;
		m_fps_den = ovi.fps_den == 0 ? 1 : ovi.fps_den;
		m_current_media_path = query_current_recording_path();
	}

	m_latest_video_dts_usec.store(-1);
	attach_output_hooks();

	blog(LOG_INFO, "[better-markers] recording started: %s", m_current_media_path.toUtf8().constData());
}

void RecordingSessionTracker::on_recording_stopped()
{
	QString closed_file;
	RecordingStoppedCallback cb;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		closed_file = m_current_media_path;
		cb = m_recording_stopped_cb;
		m_recording_active = false;
		m_recording_paused = false;
		m_pause_start_ns = 0;
		m_total_paused_ns = 0;
		m_current_media_path.clear();
	}

	m_latest_video_dts_usec.store(-1);
	detach_output_hooks();

	if (cb && !closed_file.isEmpty())
		cb(closed_file);
}

void RecordingSessionTracker::on_recording_paused(bool paused)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_recording_active)
		return;

	const uint64_t now_ns = os_gettime_ns();
	if (paused) {
		if (!m_recording_paused) {
			m_recording_paused = true;
			m_pause_start_ns = now_ns;
		}
	} else {
		if (m_recording_paused) {
			m_recording_paused = false;
			if (m_pause_start_ns != 0 && now_ns > m_pause_start_ns)
				m_total_paused_ns += now_ns - m_pause_start_ns;
			m_pause_start_ns = 0;
		}
	}
}

void RecordingSessionTracker::attach_output_hooks()
{
	detach_output_hooks();

	obs_output_t *output = obs_frontend_get_recording_output();
	if (!output)
		return;

	m_output = obs_output_get_ref(output);
	if (!m_output)
		return;

	obs_output_add_packet_callback(m_output, &RecordingSessionTracker::packet_callback, this);
	signal_handler_t *signal_handler = obs_output_get_signal_handler(m_output);
	if (signal_handler)
		signal_handler_connect(signal_handler, "file_changed", &RecordingSessionTracker::file_changed_signal, this);
}

void RecordingSessionTracker::detach_output_hooks()
{
	if (!m_output)
		return;

	obs_output_remove_packet_callback(m_output, &RecordingSessionTracker::packet_callback, this);
	signal_handler_t *signal_handler = obs_output_get_signal_handler(m_output);
	if (signal_handler)
		signal_handler_disconnect(signal_handler, "file_changed", &RecordingSessionTracker::file_changed_signal, this);
	obs_output_release(m_output);
	m_output = nullptr;
}

QString RecordingSessionTracker::query_current_recording_path() const
{
	char *path = obs_frontend_get_current_record_output_path();
	if (path && *path) {
		const QString result = QString::fromUtf8(path);
		bfree(path);
		return result;
	}
	if (path)
		bfree(path);

	path = obs_frontend_get_last_recording();
	if (path && *path) {
		const QString result = QString::fromUtf8(path);
		bfree(path);
		return result;
	}
	if (path)
		bfree(path);

	return {};
}

} // namespace bm
