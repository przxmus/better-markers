#pragma once

#include "bm-marker-data.hpp"

#include <QString>
#include <QVector>

#include <cstdint>

namespace bm {

struct MarkerExportRecordingContext {
	QString media_path;
	uint32_t fps_num = 30;
	uint32_t fps_den = 1;
};

class MarkerExportSink {
public:
	virtual ~MarkerExportSink() = default;

	virtual QString sink_name() const = 0;
	virtual bool on_marker_added(const MarkerExportRecordingContext &recording_ctx, const MarkerRecord &marker,
				     const QVector<MarkerRecord> &full_marker_list, QString *error) = 0;
	virtual bool on_recording_closed(const MarkerExportRecordingContext &recording_ctx, QString *error) = 0;
};

} // namespace bm
