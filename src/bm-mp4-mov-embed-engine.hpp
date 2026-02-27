#pragma once

#include <QByteArray>
#include <QString>

namespace bm {

struct EmbedResult {
	bool ok = false;
	QString error;
	bool retryable = false;
};

class Mp4MovEmbedEngine {
public:
	EmbedResult embed_from_sidecar(const QString &media_path, const QString &sidecar_path) const;
	EmbedResult embed_from_sidecar_with_retry(const QString &media_path, const QString &sidecar_path,
						  int max_attempts, int initial_delay_ms, int max_delay_ms) const;
	EmbedResult embed_xmp(const QString &media_path, const QByteArray &xmp_payload) const;
};

} // namespace bm
