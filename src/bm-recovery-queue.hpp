#pragma once

#include <QString>
#include <QVector>

namespace bm {

struct PendingEmbedJob {
	QString media_path;
	int attempts = 0;
	QString last_error;
	qint64 last_attempt_unix_ms = 0;
};

class RecoveryQueue {
public:
	void set_queue_path(const QString &queue_path);
	bool load();
	bool save() const;

	void upsert(const QString &media_path, const QString &last_error);
	void remove(const QString &media_path);

	const QVector<PendingEmbedJob> &jobs() const;

private:
	QString m_queue_path;
	QVector<PendingEmbedJob> m_jobs;
};

} // namespace bm
