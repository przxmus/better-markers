#pragma once

#include <QByteArray>
#include <QString>

namespace bm {

struct EmbedResult {
	bool ok = false;
	QString error;
};

class Mp4MovEmbedEngine {
public:
	EmbedResult embed_from_sidecar(const QString &media_path, const QString &sidecar_path) const;
	EmbedResult embed_xmp(const QString &media_path, const QByteArray &xmp_payload) const;
};

} // namespace bm
