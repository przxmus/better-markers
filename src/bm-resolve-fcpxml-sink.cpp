#include "bm-resolve-fcpxml-sink.hpp"

namespace bm {

QString ResolveFcpxmlSink::sink_name() const
{
	return "resolve-fcpxml";
}

bool ResolveFcpxmlSink::on_marker_added(const MarkerExportRecordingContext &recording_ctx, const MarkerRecord &,
					const QVector<MarkerRecord> &full_marker_list, QString *error)
{
	if (!is_mp4_or_mov_path(recording_ctx.media_path))
		return true;

	FcpxmlDocumentInput input;
	input.profile = FcpxmlProfile::ResolveTimelineMarkers;
	input.media_path = recording_ctx.media_path;
	input.markers = full_marker_list;
	input.fps_num = recording_ctx.fps_num;
	input.fps_den = recording_ctx.fps_den;

	const QString output_path = FcpxmlWriter::artifact_path_for_media(recording_ctx.media_path, input.profile);
	return m_writer.write_document(output_path, input, error);
}

bool ResolveFcpxmlSink::on_recording_closed(const MarkerExportRecordingContext &, QString *)
{
	return true;
}

} // namespace bm
