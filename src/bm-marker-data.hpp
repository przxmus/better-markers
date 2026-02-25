#pragma once

#include <QString>

namespace bm {

struct MarkerRecord {
	int64_t start_frame = 0;
	int64_t duration_frames = 0;
	QString name;
	QString comment;
	QString type = "Cue";
	QString guid;
	int color_id = 0;
};

struct PendingMarkerContext {
	int64_t frozen_frame = 0;
	QString media_path;
	uint64_t trigger_time_ns = 0;
};

inline bool is_mp4_or_mov_path(const QString &path)
{
	const QString lower = path.toLower();
	return lower.endsWith(".mp4") || lower.endsWith(".mov");
}

} // namespace bm
