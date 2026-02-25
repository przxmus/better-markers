#pragma once

#include "bm-marker-data.hpp"

#include <QString>
#include <QVector>

namespace bm {

class XmpSidecarWriter {
public:
	static QString sidecar_path_for_media(const QString &media_path);

	bool write_sidecar(const QString &media_path, const QVector<MarkerRecord> &markers, uint32_t fps_num,
			  uint32_t fps_den, QString *error) const;

private:
	QString build_xmp(const QVector<MarkerRecord> &markers, uint32_t fps_num, uint32_t fps_den) const;
	static QString xml_escape(const QString &value);
};

} // namespace bm
