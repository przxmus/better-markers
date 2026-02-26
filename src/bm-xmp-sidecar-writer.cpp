#include "bm-xmp-sidecar-writer.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#include <QUuid>

#include <algorithm>
#include <optional>

namespace bm {
namespace {

QString premiere_frame_rate_value(uint32_t fps_num, uint32_t fps_den)
{
	if (fps_num == 0 || fps_den == 0)
		return "f30";

	const double fps = static_cast<double>(fps_num) / static_cast<double>(fps_den);
	const int rounded_fps = std::max(1, qRound(fps));
	return QString("f%1").arg(rounded_fps);
}

std::optional<quint32> premiere_color_argb_value(int color_id)
{
	switch (color_id) {
	case 0:
		return std::nullopt;
	case 1:
		return 4281740498U; // Red
	case 2:
		return 4280578025U; // Orange
	case 3:
		return 4281049552U; // Yellow
	case 4:
		return 4294967295U; // White
	case 5:
		return 4294741314U; // Blue
	case 6:
		return 4292277273U; // Cyan
	case 7:
		return 4289825711U; // Lavender
	case 8:
		return 4294902015U; // Magenta
	default:
		return std::nullopt;
	}
}

} // namespace

QString XmpSidecarWriter::sidecar_path_for_media(const QString &media_path)
{
	const QFileInfo info(media_path);
	return info.dir().filePath(info.completeBaseName() + ".xmp");
}

bool XmpSidecarWriter::write_sidecar(const QString &media_path, const QVector<MarkerRecord> &markers, uint32_t fps_num,
					     uint32_t fps_den, QString *error) const
{
	if (fps_num == 0)
		fps_num = 30;
	if (fps_den == 0)
		fps_den = 1;

	const QString sidecar_path = sidecar_path_for_media(media_path);
	QSaveFile file(sidecar_path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		if (error)
			*error = QString("Failed to open sidecar for write: %1").arg(sidecar_path);
		return false;
	}

	const QByteArray payload = build_xmp(markers, fps_num, fps_den).toUtf8();
	if (file.write(payload) == -1) {
		if (error)
			*error = QString("Failed to write sidecar: %1").arg(sidecar_path);
		return false;
	}

	if (!file.commit()) {
		if (error)
			*error = QString("Failed to commit sidecar: %1").arg(sidecar_path);
		return false;
	}

	return true;
}

QString XmpSidecarWriter::build_xmp(const QVector<MarkerRecord> &markers, uint32_t fps_num, uint32_t fps_den) const
{
	QString xml;
	QTextStream stream(&xml);
	stream.setEncoding(QStringConverter::Utf8);

	const QString frame_rate = premiere_frame_rate_value(fps_num, fps_den);

	stream << "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n";
	stream << "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"Better Markers\">\n";
	stream << "  <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n";
	stream << "    <rdf:Description rdf:about=\"\" xmlns:xmpDM=\"http://ns.adobe.com/xmp/1.0/DynamicMedia/\">\n";
	stream << "      <xmpDM:Tracks>\n";
	stream << "        <rdf:Bag>\n";
	stream << "          <rdf:li>\n";
	stream << "            <rdf:Description xmpDM:trackName=\"Markers\" xmpDM:frameRate=\"" << frame_rate << "\">\n";
	stream << "              <xmpDM:markers>\n";
	stream << "                <rdf:Seq>\n";

	for (const MarkerRecord &marker : markers) {
		stream << "                  <rdf:li>\n";
		stream << "                    <rdf:Description xmpDM:startTime=\"" << marker.start_frame << "\"";
		stream << " xmpDM:name=\"" << xml_escape(marker.name) << "\"";
		stream << " xmpDM:comment=\"" << xml_escape(marker.comment) << "\"";
		stream << " xmpDM:type=\"" << xml_escape(marker.type.isEmpty() ? QString("Comment") : marker.type) << "\"";
		stream << " xmpDM:guid=\"" << xml_escape(marker.guid) << "\">\n";
		stream << "                      <xmpDM:cuePointParams>\n";
		stream << "                        <rdf:Seq>\n";
		stream << "                          <rdf:li xmpDM:key=\"marker_guid\" xmpDM:value=\""
		       << xml_escape(marker.guid) << "\"/>\n";

		const std::optional<quint32> argb_color = premiere_color_argb_value(marker.color_id);
		if (argb_color.has_value()) {
			const QString color_keyword = "keywordExtDVAv1_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
			const QString color_payload = QString("{\"color\":%1}").arg(argb_color.value());
			stream << "                          <rdf:li xmpDM:key=\"" << color_keyword << "\" xmpDM:value=\""
			       << xml_escape(color_payload) << "\"/>\n";
		}

		stream << "                        </rdf:Seq>\n";
		stream << "                      </xmpDM:cuePointParams>\n";
		stream << "                    </rdf:Description>\n";
		stream << "                  </rdf:li>\n";
	}

	stream << "                </rdf:Seq>\n";
	stream << "              </xmpDM:markers>\n";
	stream << "            </rdf:Description>\n";
	stream << "          </rdf:li>\n";
	stream << "        </rdf:Bag>\n";
	stream << "      </xmpDM:Tracks>\n";
	stream << "    </rdf:Description>\n";
	stream << "  </rdf:RDF>\n";
	stream << "</x:xmpmeta>\n";
	stream << "<?xpacket end=\"w\"?>";

	return xml;
}

QString XmpSidecarWriter::xml_escape(const QString &value)
{
	QString escaped = value;
	escaped.replace('&', "&amp;");
	escaped.replace('<', "&lt;");
	escaped.replace('>', "&gt;");
	escaped.replace('"', "&quot;");
	escaped.replace('\'', "&apos;");
	return escaped;
}

} // namespace bm
