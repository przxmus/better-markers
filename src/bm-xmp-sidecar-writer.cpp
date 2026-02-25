#include "bm-xmp-sidecar-writer.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

namespace bm {

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

	stream << "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n";
	stream << "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"Better Markers\">\n";
	stream << "  <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n";
	stream << "    <rdf:Description rdf:about=\"\" xmlns:xmpDM=\"http://ns.adobe.com/xmp/1.0/DynamicMedia/\">\n";
	stream << "      <xmpDM:Tracks>\n";
	stream << "        <rdf:Bag>\n";
	stream << "          <rdf:li rdf:parseType=\"Resource\">\n";
	stream << "            <xmpDM:trackName>Adobe Premiere Pro Clip Marker</xmpDM:trackName>\n";
	stream << "            <xmpDM:trackType>Clip</xmpDM:trackType>\n";
	stream << "            <xmpDM:frameRate>" << fps_num << "f" << fps_den << "s</xmpDM:frameRate>\n";
	stream << "            <xmpDM:markers>\n";
	stream << "              <rdf:Seq>\n";

	for (const MarkerRecord &marker : markers) {
		stream << "                <rdf:li rdf:parseType=\"Resource\">\n";
		stream << "                  <xmpDM:startTime>" << marker.start_frame << "</xmpDM:startTime>\n";
		stream << "                  <xmpDM:duration>" << marker.duration_frames << "</xmpDM:duration>\n";
		stream << "                  <xmpDM:name>" << xml_escape(marker.name) << "</xmpDM:name>\n";
		stream << "                  <xmpDM:comment>" << xml_escape(marker.comment) << "</xmpDM:comment>\n";
		stream << "                  <xmpDM:type>Cue</xmpDM:type>\n";
		stream << "                  <xmpDM:guid>" << xml_escape(marker.guid) << "</xmpDM:guid>\n";
		stream << "                  <xmpDM:cuePointParams>\n";
		stream << "                    <rdf:Seq>\n";
		stream << "                      <rdf:li xmpDM:key=\"marker_guid\" xmpDM:value=\""
		       << xml_escape(marker.guid) << "\"/>\n";
		stream << "                      <rdf:li xmpDM:key=\"color\" xmpDM:value=\"" << marker.color_id
		       << "\"/>\n";
		stream << "                    </rdf:Seq>\n";
		stream << "                  </xmpDM:cuePointParams>\n";
		stream << "                </rdf:li>\n";
	}

	stream << "              </rdf:Seq>\n";
	stream << "            </xmpDM:markers>\n";
	stream << "          </rdf:li>\n";
	stream << "        </rdf:Bag>\n";
	stream << "      </xmpDM:Tracks>\n";
	stream << "    </rdf:Description>\n";
	stream << "  </rdf:RDF>\n";
	stream << "</x:xmpmeta>\n";
	stream << "<?xpacket end=\"w\"?>\n";

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
