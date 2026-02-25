#include "bm-recovery-queue.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace bm {

void RecoveryQueue::set_queue_path(const QString &queue_path)
{
	m_queue_path = queue_path;
}

bool RecoveryQueue::load()
{
	m_jobs.clear();
	if (m_queue_path.isEmpty())
		return true;

	QFile file(m_queue_path);
	if (!file.exists())
		return true;
	if (!file.open(QIODevice::ReadOnly))
		return false;

	QJsonParseError error;
	const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
	if (error.error != QJsonParseError::NoError || !doc.isObject())
		return false;

	const QJsonArray jobs = doc.object().value("jobs").toArray();
	for (QJsonValue value : jobs) {
		if (!value.isObject())
			continue;

		const QJsonObject obj = value.toObject();
		PendingEmbedJob job;
		job.media_path = obj.value("mediaPath").toString();
		job.attempts = obj.value("attempts").toInt(0);
		job.last_error = obj.value("lastError").toString();
		job.last_attempt_unix_ms = obj.value("lastAttemptUnixMs").toVariant().toLongLong();
		if (!job.media_path.isEmpty())
			m_jobs.push_back(job);
	}

	return true;
}

bool RecoveryQueue::save() const
{
	if (m_queue_path.isEmpty())
		return false;

	QFileInfo info(m_queue_path);
	QDir dir = info.dir();
	if (!dir.exists() && !dir.mkpath("."))
		return false;

	QJsonArray jobs;
	for (const PendingEmbedJob &job : m_jobs) {
		QJsonObject obj;
		obj.insert("mediaPath", job.media_path);
		obj.insert("attempts", job.attempts);
		obj.insert("lastError", job.last_error);
		obj.insert("lastAttemptUnixMs", static_cast<qint64>(job.last_attempt_unix_ms));
		jobs.push_back(obj);
	}

	QJsonObject root;
	root.insert("jobs", jobs);

	QSaveFile file(m_queue_path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) == -1)
		return false;
	return file.commit();
}

void RecoveryQueue::upsert(const QString &media_path, const QString &last_error)
{
	for (PendingEmbedJob &job : m_jobs) {
		if (job.media_path == media_path) {
			job.attempts += 1;
			job.last_error = last_error;
			job.last_attempt_unix_ms = QDateTime::currentMSecsSinceEpoch();
			return;
		}
	}

	PendingEmbedJob job;
	job.media_path = media_path;
	job.attempts = 1;
	job.last_error = last_error;
	job.last_attempt_unix_ms = QDateTime::currentMSecsSinceEpoch();
	m_jobs.push_back(job);
}

void RecoveryQueue::remove(const QString &media_path)
{
	for (int i = 0; i < m_jobs.size(); ++i) {
		if (m_jobs.at(i).media_path == media_path) {
			m_jobs.removeAt(i);
			return;
		}
	}
}

const QVector<PendingEmbedJob> &RecoveryQueue::jobs() const
{
	return m_jobs;
}

} // namespace bm
