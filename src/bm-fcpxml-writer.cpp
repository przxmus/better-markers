#include "bm-fcpxml-writer.hpp"

#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QUrl>

#include <algorithm>
#include <cstdlib>
#include <numeric>

namespace bm {
namespace {

struct Rational {
	int64_t num = 0;
	int64_t den = 1;
};

Rational normalize_rational(int64_t num, int64_t den)
{
	if (den == 0)
		return {0, 1};
	if (den < 0) {
		num = -num;
		den = -den;
	}

	const int64_t divisor = std::gcd(std::llabs(num), std::llabs(den));
	if (divisor <= 1)
		return {num, den};
	return {num / divisor, den / divisor};
}

Rational seconds_from_frames(int64_t frame, uint32_t fps_num, uint32_t fps_den)
{
	if (fps_num == 0)
		fps_num = 30;
	if (fps_den == 0)
		fps_den = 1;
	return normalize_rational(frame * static_cast<int64_t>(fps_den), static_cast<int64_t>(fps_num));
}

Rational frame_duration_seconds(uint32_t fps_num, uint32_t fps_den)
{
	if (fps_num == 0)
		fps_num = 30;
	if (fps_den == 0)
		fps_den = 1;
	return normalize_rational(static_cast<int64_t>(fps_den), static_cast<int64_t>(fps_num));
}

QString rational_to_fcpx_time(const Rational &rational)
{
	return QString("%1/%2s").arg(rational.num).arg(rational.den);
}

} // namespace

QString FcpxmlWriter::artifact_path_for_media(const QString &media_path, FcpxmlProfile profile)
{
	const QFileInfo info(media_path);
	const QString stem = info.completeBaseName();
	if (profile == FcpxmlProfile::FinalCutClipMarkers)
		return info.dir().filePath(stem + ".better-markers.fcp.fcpxml");
	return info.dir().filePath(stem + ".better-markers.resolve.fcpxml");
}

QString FcpxmlWriter::rational_time_from_frames(int64_t frame, uint32_t fps_num, uint32_t fps_den)
{
	return rational_to_fcpx_time(seconds_from_frames(frame, fps_num, fps_den));
}

QString FcpxmlWriter::frame_duration_rational(uint32_t fps_num, uint32_t fps_den)
{
	return rational_to_fcpx_time(frame_duration_seconds(fps_num, fps_den));
}

QString FcpxmlWriter::file_url_from_path(const QString &path)
{
	return QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
}

bool FcpxmlWriter::write_document(const QString &output_path, const FcpxmlDocumentInput &input, QString *error) const
{
	QSaveFile file(output_path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		if (error)
			*error = QString("Failed to open FCPXML for write: %1").arg(output_path);
		return false;
	}

	const QByteArray payload = build_document(input).toUtf8();
	if (file.write(payload) == -1) {
		if (error)
			*error = QString("Failed to write FCPXML: %1").arg(output_path);
		return false;
	}

	if (!file.commit()) {
		if (error)
			*error = QString("Failed to commit FCPXML: %1").arg(output_path);
		return false;
	}

	return true;
}

QString FcpxmlWriter::build_document(const FcpxmlDocumentInput &input) const
{
	const int64_t timeline_duration_frames = compute_timeline_duration_frames(input.markers);
	const QString frame_duration = frame_duration_rational(input.fps_num, input.fps_den);
	const QString timeline_duration = rational_time_from_frames(timeline_duration_frames, input.fps_num, input.fps_den);
	const QString media_url = file_url_from_path(input.media_path);
	const QString clip_name = QFileInfo(input.media_path).completeBaseName();
	const QString project_name = clip_name.isEmpty() ? QString("Better Markers Export") : clip_name;

	QString xml;
	QTextStream stream(&xml);
	stream.setEncoding(QStringConverter::Utf8);

	stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	stream << "<!DOCTYPE fcpxml>\n";
	stream << "<fcpxml version=\"1.11\">\n";
	stream << "  <resources>\n";
	stream << "    <format id=\"r1\" frameDuration=\"" << frame_duration
	       << "\" width=\"1920\" height=\"1080\" colorSpace=\"1-1-1 (Rec. 709)\"/>\n";
	stream << "    <asset id=\"r2\" name=\"" << xml_escape(clip_name) << "\" src=\"" << xml_escape(media_url)
	       << "\" start=\"0s\" duration=\"" << timeline_duration << "\" hasVideo=\"1\" hasAudio=\"1\" format=\"r1\"/>\n";
	stream << "  </resources>\n";
	stream << "  <library>\n";
	stream << "    <event name=\"Better Markers\">\n";
	stream << "      <project name=\"" << xml_escape(project_name) << "\">\n";
	stream << "        <sequence format=\"r1\" duration=\"" << timeline_duration
	       << "\" tcStart=\"0s\" tcFormat=\"NDF\" audioLayout=\"stereo\" audioRate=\"48k\">\n";
	stream << "          <spine>\n";
	stream << "            <asset-clip ref=\"r2\" name=\"" << xml_escape(clip_name)
	       << "\" offset=\"0s\" start=\"0s\" duration=\"" << timeline_duration << "\"/>\n";
	stream << "          </spine>\n";
	stream << "        </sequence>\n";
	stream << "      </project>\n";
	stream << "    </event>\n";
	stream << "  </library>\n";
	stream << "</fcpxml>\n";

	return xml;
}

QString FcpxmlWriter::xml_escape(const QString &value)
{
	QString escaped = value;
	escaped.replace('&', "&amp;");
	escaped.replace('<', "&lt;");
	escaped.replace('>', "&gt;");
	escaped.replace('"', "&quot;");
	escaped.replace('\'', "&apos;");
	return escaped;
}

int64_t FcpxmlWriter::compute_timeline_duration_frames(const QVector<MarkerRecord> &markers)
{
	int64_t max_frame = 1;
	for (const MarkerRecord &marker : markers)
		max_frame = std::max<int64_t>(max_frame, marker.start_frame + 1);
	return max_frame;
}

} // namespace bm
