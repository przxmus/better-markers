#pragma once

#include "bm-marker-data.hpp"

#include <QString>
#include <QTextStream>
#include <QVector>

#include <cstdint>

namespace bm {

enum class FcpxmlProfile {
	FinalCutClipMarkers,
	ResolveTimelineMarkers,
};

struct FcpxmlDocumentInput {
	FcpxmlProfile profile = FcpxmlProfile::FinalCutClipMarkers;
	QString media_path;
	QVector<MarkerRecord> markers;
	uint32_t fps_num = 30;
	uint32_t fps_den = 1;
};

class FcpxmlWriter {
public:
	static QString artifact_path_for_media(const QString &media_path, FcpxmlProfile profile);
	static QString rational_time_from_frames(int64_t frame, uint32_t fps_num, uint32_t fps_den);
	static QString frame_duration_rational(uint32_t fps_num, uint32_t fps_den);
	static QString file_url_from_path(const QString &path);

	bool write_document(const QString &output_path, const FcpxmlDocumentInput &input, QString *error) const;
	QString build_document(const FcpxmlDocumentInput &input) const;

private:
	void append_final_cut_clip_markers(QTextStream &stream, const QVector<MarkerRecord> &markers, uint32_t fps_num,
					   uint32_t fps_den) const;
	static QString xml_escape(const QString &value);
	static int64_t compute_timeline_duration_frames(const QVector<MarkerRecord> &markers);
};

} // namespace bm
