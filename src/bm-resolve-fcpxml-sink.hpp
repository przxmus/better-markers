#pragma once

#include "bm-fcpxml-writer.hpp"
#include "bm-marker-export-sink.hpp"

namespace bm {

class ResolveFcpxmlSink : public MarkerExportSink {
public:
	QString sink_name() const override;
	bool on_marker_added(const MarkerExportRecordingContext &recording_ctx, const MarkerRecord &marker,
			     const QVector<MarkerRecord> &full_marker_list, QString *error) override;
	bool on_recording_closed(const MarkerExportRecordingContext &recording_ctx, QString *error) override;

private:
	FcpxmlWriter m_writer;
};

} // namespace bm
