#include "bm-mp4-mov-embed-engine.hpp"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>
#include <limits>

namespace bm {
namespace {

const QByteArray ADOBE_XMP_UUID = QByteArray::fromHex("be7acfcb97a942e89c71999491e3afac");

struct Atom {
	quint64 offset = 0;
	quint64 size = 0;
	quint64 header_size = 0;
	QByteArray type;
};

quint32 read_be32(const char *data)
{
	return (static_cast<quint32>(static_cast<unsigned char>(data[0])) << 24) |
	       (static_cast<quint32>(static_cast<unsigned char>(data[1])) << 16) |
	       (static_cast<quint32>(static_cast<unsigned char>(data[2])) << 8) |
	       (static_cast<quint32>(static_cast<unsigned char>(data[3])));
}

quint64 read_be64(const char *data)
{
	return (static_cast<quint64>(read_be32(data)) << 32) | read_be32(data + 4);
}

void write_be32(char *data, quint32 value)
{
	data[0] = static_cast<char>((value >> 24) & 0xFF);
	data[1] = static_cast<char>((value >> 16) & 0xFF);
	data[2] = static_cast<char>((value >> 8) & 0xFF);
	data[3] = static_cast<char>(value & 0xFF);
}

bool is_xmp_uuid_atom(const QByteArray &atom)
{
	if (atom.size() < 24)
		return false;
	if (atom.mid(4, 4) != "uuid")
		return false;
	if (atom.mid(8, 16) != ADOBE_XMP_UUID)
		return false;
	return atom.indexOf("<?xpacket", 24) >= 0;
}

bool parse_atoms_from_buffer(const QByteArray &buffer, int start, int end, QVector<Atom> &atoms, QString *error)
{
	atoms.clear();
	int pos = start;
	while (pos + 8 <= end) {
		const char *ptr = buffer.constData() + pos;
		const quint32 size32 = read_be32(ptr);
		const QByteArray type(ptr + 4, 4);

		quint64 size = size32;
		quint64 header = 8;
		if (size32 == 1) {
			if (pos + 16 > end) {
				if (error)
					*error = "Truncated extended atom header";
				return false;
			}
			size = read_be64(ptr + 8);
			header = 16;
		} else if (size32 == 0) {
			size = static_cast<quint64>(end - pos);
		}

		if (size < header || pos + static_cast<qint64>(size) > end) {
			if (error)
				*error = "Invalid atom size while parsing buffer";
			return false;
		}

		atoms.push_back(Atom{static_cast<quint64>(pos), size, header, type});
		pos += static_cast<int>(size);
	}

	if (pos != end) {
		if (error)
			*error = "Unaligned atom payload";
		return false;
	}

	return true;
}

bool parse_top_level_atoms(QFile &file, QVector<Atom> &atoms, QString *error)
{
	atoms.clear();
	const quint64 file_size = static_cast<quint64>(file.size());
	quint64 offset = 0;

	while (offset + 8 <= file_size) {
		if (!file.seek(static_cast<qint64>(offset))) {
			if (error)
				*error = "Failed to seek while parsing atoms";
			return false;
		}

		QByteArray header = file.read(16);
		if (header.size() < 8) {
			if (error)
				*error = "Truncated atom header";
			return false;
		}

		const quint32 size32 = read_be32(header.constData());
		const QByteArray type = header.mid(4, 4);

		quint64 size = size32;
		quint64 header_size = 8;
		if (size32 == 1) {
			if (header.size() < 16) {
				if (error)
					*error = "Truncated extended atom header";
				return false;
			}
			size = read_be64(header.constData() + 8);
			header_size = 16;
		} else if (size32 == 0) {
			size = file_size - offset;
		}

		if (size < header_size || offset + size > file_size) {
			if (error)
				*error = "Invalid top-level atom size";
			return false;
		}

		atoms.push_back(Atom{offset, size, header_size, type});
		offset += size;
	}

	if (offset != file_size) {
		if (error)
			*error = "Top-level atom parse did not end on file boundary";
		return false;
	}

	return true;
}

QByteArray make_atom(const QByteArray &type, const QByteArray &payload, bool *ok)
{
	if (type.size() != 4) {
		if (ok)
			*ok = false;
		return {};
	}

	const quint64 total_size = 8 + static_cast<quint64>(payload.size());
	if (total_size > 0xFFFFFFFFULL) {
		if (ok)
			*ok = false;
		return {};
	}

	QByteArray atom;
	atom.resize(static_cast<int>(total_size));
	write_be32(atom.data(), static_cast<quint32>(total_size));
	memcpy(atom.data() + 4, type.constData(), 4);
	if (!payload.isEmpty())
		memcpy(atom.data() + 8, payload.constData(), payload.size());

	if (ok)
		*ok = true;
	return atom;
}

QByteArray make_uuid_atom(const QByteArray &payload, bool *ok)
{
	const quint64 total_size = 8 + 16 + static_cast<quint64>(payload.size());
	if (total_size > 0xFFFFFFFFULL) {
		if (ok)
			*ok = false;
		return {};
	}

	QByteArray atom;
	atom.resize(static_cast<int>(total_size));
	write_be32(atom.data(), static_cast<quint32>(total_size));
	memcpy(atom.data() + 4, "uuid", 4);
	memcpy(atom.data() + 8, ADOBE_XMP_UUID.constData(), 16);
	if (!payload.isEmpty())
		memcpy(atom.data() + 24, payload.constData(), payload.size());

	if (ok)
		*ok = true;
	return atom;
}

bool copy_range(QFile &input, QFile &output, quint64 offset, quint64 length, QString *error)
{
	if (!input.seek(static_cast<qint64>(offset))) {
		if (error)
			*error = "Failed to seek input for copy";
		return false;
	}

	quint64 remaining = length;
	while (remaining > 0) {
		const qint64 chunk = static_cast<qint64>(remaining > (1ULL << 20) ? (1ULL << 20) : remaining);
		QByteArray data = input.read(chunk);
		if (data.size() != chunk) {
			if (error)
				*error = "Failed to read input while copying";
			return false;
		}
		if (output.write(data) != data.size()) {
			if (error)
				*error = "Failed to write output while copying";
			return false;
		}
		remaining -= static_cast<quint64>(chunk);
	}

	return true;
}

bool has_xmp_atom(const QString &path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	QVector<Atom> top;
	QString error;
	if (!parse_top_level_atoms(file, top, &error))
		return false;

	for (const Atom &atom : top) {
		if (atom.type == "XMP_")
			return true;
		if (atom.type != "uuid")
			continue;

		if (!file.seek(static_cast<qint64>(atom.offset)))
			return false;
		const QByteArray uuid_atom = file.read(static_cast<qint64>(atom.size));
		if (uuid_atom.size() != static_cast<qint64>(atom.size))
			return false;
		if (is_xmp_uuid_atom(uuid_atom))
			return true;
	}

	// Compatibility fallback for files that store XMP inside moov/udta.
	for (const Atom &atom : top) {
		if (atom.type != "moov")
			continue;

		if (!file.seek(static_cast<qint64>(atom.offset)))
			return false;
		const QByteArray moov = file.read(static_cast<qint64>(atom.size));
		if (moov.size() != static_cast<qint64>(atom.size))
			return false;
		if (moov.size() > std::numeric_limits<int>::max())
			return false;
		const int moov_size = static_cast<int>(moov.size());

		QVector<Atom> moov_children;
		if (!parse_atoms_from_buffer(moov, 8, moov_size, moov_children, &error))
			return false;
		for (const Atom &child : moov_children) {
			if (child.type != "udta")
				continue;

			const QByteArray udta = moov.mid(static_cast<int>(child.offset), static_cast<int>(child.size));
			if (udta.size() > std::numeric_limits<int>::max())
				return false;
			const int udta_size = static_cast<int>(udta.size());
			QVector<Atom> udta_children;
			if (!parse_atoms_from_buffer(udta, 8, udta_size, udta_children, &error))
				return false;
			for (const Atom &udta_child : udta_children) {
				if (udta_child.type == "XMP_")
					return true;
				if (udta_child.type == "uuid") {
					const QByteArray uuid_atom = udta.mid(static_cast<int>(udta_child.offset),
									      static_cast<int>(udta_child.size));
					if (is_xmp_uuid_atom(uuid_atom))
						return true;
				}
			}
		}
	}

	return false;
}

EmbedResult embed_failure(const QString &error, bool retryable)
{
	return {false, error, retryable};
}

EmbedResult try_embed_with_exiftool(const QString &media_path, const QString &sidecar_path)
{
	const QString exiftool = QStandardPaths::findExecutable("exiftool");
	if (exiftool.isEmpty())
		return embed_failure("ExifTool is not installed", true);

	QProcess process;
	process.start(exiftool, {"-overwrite_original", QString("-XMP<=%1").arg(sidecar_path), media_path});
	if (!process.waitForStarted(1000))
		return embed_failure("Failed to start ExifTool process", true);
	if (!process.waitForFinished(20000)) {
		process.kill();
		process.waitForFinished(1000);
		return embed_failure("ExifTool process timed out", true);
	}

	if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
		const QString stderr_text = QString::fromUtf8(process.readAllStandardError()).trimmed();
		const QString stdout_text = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
		const QString detail = !stderr_text.isEmpty() ? stderr_text : stdout_text;
		return embed_failure(detail.isEmpty() ? "ExifTool returned non-zero status"
						      : QString("ExifTool failed: %1").arg(detail),
				     true);
	}

	if (!has_xmp_atom(media_path))
		return embed_failure("ExifTool reported success but XMP metadata was not found", false);

	return {true, {}};
}

} // namespace

EmbedResult Mp4MovEmbedEngine::embed_from_sidecar(const QString &media_path, const QString &sidecar_path) const
{
	QFile sidecar(sidecar_path);
	if (!sidecar.exists())
		return embed_failure(QString("Missing sidecar: %1").arg(sidecar_path), false);
	if (!sidecar.open(QIODevice::ReadOnly))
		return embed_failure(QString("Failed to open sidecar: %1").arg(sidecar_path), true);

	const QByteArray payload = sidecar.readAll();
	if (payload.isEmpty())
		return embed_failure(QString("Sidecar is empty: %1").arg(sidecar_path), false);

	const EmbedResult exiftool_result = try_embed_with_exiftool(media_path, sidecar_path);
	if (exiftool_result.ok)
		return exiftool_result;

	return embed_xmp(media_path, payload);
}

EmbedResult Mp4MovEmbedEngine::embed_from_sidecar_with_retry(const QString &media_path, const QString &sidecar_path,
							     int max_attempts, int initial_delay_ms,
							     int max_delay_ms) const
{
	const int attempts = std::max(1, max_attempts);
	int delay_ms = std::max(0, initial_delay_ms);
	const int max_delay = std::max(delay_ms, max_delay_ms);

	EmbedResult result = embed_from_sidecar(media_path, sidecar_path);
	for (int attempt = 1; attempt < attempts && !result.ok && result.retryable; ++attempt) {
		if (delay_ms > 0)
			QThread::msleep(static_cast<unsigned long>(delay_ms));
		result = embed_from_sidecar(media_path, sidecar_path);
		if (delay_ms > 0)
			delay_ms = std::min(max_delay, delay_ms * 2);
	}

	return result;
}

EmbedResult Mp4MovEmbedEngine::embed_xmp(const QString &media_path, const QByteArray &xmp_payload) const
{
	QFile input(media_path);
	if (!input.exists())
		return embed_failure(QString("Recording file not found: %1").arg(media_path), true);
	if (!input.open(QIODevice::ReadOnly))
		return embed_failure(QString("Failed to open recording file: %1").arg(media_path), true);

	QVector<Atom> top_level;
	QString error;
	if (!parse_top_level_atoms(input, top_level, &error))
		return embed_failure(QString("Failed to parse MP4/MOV atoms: %1").arg(error), true);

	bool ok = true;
	const QByteArray xmp_atom = make_uuid_atom(xmp_payload, &ok);
	if (!ok)
		return embed_failure("XMP payload is too large for MP4 uuid atom", false);

	int existing_xmp_index = -1;
	for (int i = 0; i < top_level.size(); ++i) {
		const Atom &atom = top_level.at(i);
		if (atom.type != "uuid")
			continue;
		if (!input.seek(static_cast<qint64>(atom.offset)))
			return embed_failure("Failed to seek existing uuid atom", true);
		const QByteArray uuid_atom = input.read(static_cast<qint64>(atom.size));
		if (uuid_atom.size() != static_cast<qint64>(atom.size))
			return embed_failure("Failed to read existing uuid atom", true);
		if (is_xmp_uuid_atom(uuid_atom)) {
			existing_xmp_index = i;
			break;
		}
	}

	const QString temp_path = media_path + ".better-markers.tmp";
	const QString backup_path = media_path + ".better-markers.bak";
	QFile::remove(temp_path);

	QFile output(temp_path);
	if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return embed_failure(QString("Failed to open temp file: %1").arg(temp_path), true);

	if (existing_xmp_index >= 0) {
		const Atom existing_xmp = top_level.at(existing_xmp_index);

		if (!copy_range(input, output, 0, existing_xmp.offset, &error)) {
			output.close();
			QFile::remove(temp_path);
			return embed_failure(error, true);
		}
		if (output.write(xmp_atom) != xmp_atom.size()) {
			output.close();
			QFile::remove(temp_path);
			return embed_failure("Failed to write replacement XMP uuid atom", true);
		}

		const quint64 tail_offset = existing_xmp.offset + existing_xmp.size;
		const quint64 tail_size = static_cast<quint64>(input.size()) - tail_offset;
		if (!copy_range(input, output, tail_offset, tail_size, &error)) {
			output.close();
			QFile::remove(temp_path);
			return embed_failure(error, true);
		}
	} else {
		if (!copy_range(input, output, 0, static_cast<quint64>(input.size()), &error)) {
			output.close();
			QFile::remove(temp_path);
			return embed_failure(error, true);
		}
		if (output.write(xmp_atom) != xmp_atom.size()) {
			output.close();
			QFile::remove(temp_path);
			return embed_failure("Failed to append XMP uuid atom", true);
		}
	}
	output.close();
	input.close();

	if (!has_xmp_atom(temp_path)) {
		QFile::remove(temp_path);
		return embed_failure("Embedded file validation failed (missing XMP metadata atom)", false);
	}

	QFile::remove(backup_path);
	if (!QFile::rename(media_path, backup_path)) {
		QFile::remove(temp_path);
		return embed_failure("Failed to create backup before atomic replace", true);
	}

	if (!QFile::rename(temp_path, media_path)) {
		QFile::rename(backup_path, media_path);
		QFile::remove(temp_path);
		return embed_failure("Failed to replace original file with embedded file", true);
	}

	QFile::remove(backup_path);
	return {true, {}};
}

} // namespace bm
