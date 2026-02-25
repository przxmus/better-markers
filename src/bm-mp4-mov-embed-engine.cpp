#include "bm-mp4-mov-embed-engine.hpp"

#include <QFile>
#include <QFileInfo>

namespace bm {
namespace {

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

QByteArray patch_udta(const QByteArray &udta_atom, const QByteArray &xmp_payload, QString *error)
{
	QVector<Atom> children;
	if (!parse_atoms_from_buffer(udta_atom, 8, udta_atom.size(), children, error))
		return {};

	bool ok = true;
	const QByteArray xmp_atom = make_atom("XMP_", xmp_payload, &ok);
	if (!ok) {
		if (error)
			*error = "XMP atom too large";
		return {};
	}

	QByteArray new_payload;
	new_payload.reserve(udta_atom.size() + xmp_atom.size());
	bool replaced = false;
	for (const Atom &child : children) {
		if (child.type == "XMP_") {
			new_payload.append(xmp_atom);
			replaced = true;
		} else {
			new_payload.append(udta_atom.mid(static_cast<int>(child.offset), static_cast<int>(child.size)));
		}
	}
	if (!replaced)
		new_payload.append(xmp_atom);

	return make_atom("udta", new_payload, &ok);
}

QByteArray patch_moov(const QByteArray &moov_atom, const QByteArray &xmp_payload, QString *error)
{
	QVector<Atom> children;
	if (!parse_atoms_from_buffer(moov_atom, 8, moov_atom.size(), children, error))
		return {};

	QByteArray new_payload;
	new_payload.reserve(moov_atom.size() + xmp_payload.size() + 64);
	bool patched_udta = false;
	bool ok = true;

	for (const Atom &child : children) {
		if (child.type == "udta") {
			const QByteArray udta_atom = moov_atom.mid(static_cast<int>(child.offset), static_cast<int>(child.size));
			const QByteArray patched = patch_udta(udta_atom, xmp_payload, error);
			if (patched.isEmpty())
				return {};
			new_payload.append(patched);
			patched_udta = true;
		} else {
			new_payload.append(moov_atom.mid(static_cast<int>(child.offset), static_cast<int>(child.size)));
		}
	}

	if (!patched_udta) {
		const QByteArray udta_atom = make_atom("udta", make_atom("XMP_", xmp_payload, &ok), &ok);
		if (!ok) {
			if (error)
				*error = "Failed to build udta atom";
			return {};
		}
		new_payload.append(udta_atom);
	}

	const QByteArray new_moov = make_atom("moov", new_payload, &ok);
	if (!ok) {
		if (error)
			*error = "Failed to build moov atom";
		return {};
	}
	return new_moov;
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
		if (atom.type != "moov")
			continue;

		if (!file.seek(static_cast<qint64>(atom.offset)))
			return false;
		const QByteArray moov = file.read(static_cast<qint64>(atom.size));
		if (moov.size() != static_cast<qint64>(atom.size))
			return false;

		QVector<Atom> moov_children;
		if (!parse_atoms_from_buffer(moov, 8, moov.size(), moov_children, &error))
			return false;
		for (const Atom &child : moov_children) {
			if (child.type != "udta")
				continue;

			const QByteArray udta = moov.mid(static_cast<int>(child.offset), static_cast<int>(child.size));
			QVector<Atom> udta_children;
			if (!parse_atoms_from_buffer(udta, 8, udta.size(), udta_children, &error))
				return false;
			for (const Atom &udta_child : udta_children) {
				if (udta_child.type == "XMP_")
					return true;
			}
		}
	}

	return false;
}

} // namespace

EmbedResult Mp4MovEmbedEngine::embed_from_sidecar(const QString &media_path, const QString &sidecar_path) const
{
	QFile sidecar(sidecar_path);
	if (!sidecar.exists())
		return {false, QString("Missing sidecar: %1").arg(sidecar_path)};
	if (!sidecar.open(QIODevice::ReadOnly))
		return {false, QString("Failed to open sidecar: %1").arg(sidecar_path)};

	const QByteArray payload = sidecar.readAll();
	if (payload.isEmpty())
		return {false, QString("Sidecar is empty: %1").arg(sidecar_path)};

	return embed_xmp(media_path, payload);
}

EmbedResult Mp4MovEmbedEngine::embed_xmp(const QString &media_path, const QByteArray &xmp_payload) const
{
	QFile input(media_path);
	if (!input.exists())
		return {false, QString("Recording file not found: %1").arg(media_path)};
	if (!input.open(QIODevice::ReadOnly))
		return {false, QString("Failed to open recording file: %1").arg(media_path)};

	QVector<Atom> top_level;
	QString error;
	if (!parse_top_level_atoms(input, top_level, &error))
		return {false, QString("Failed to parse MP4/MOV atoms: %1").arg(error)};

	int moov_index = -1;
	for (int i = 0; i < top_level.size(); ++i) {
		if (top_level.at(i).type == "moov") {
			moov_index = i;
			break;
		}
	}
	if (moov_index < 0)
		return {false, "Missing moov atom"};

	const Atom moov = top_level.at(moov_index);
	for (const Atom &atom : top_level) {
		if (atom.type == "mdat" && atom.offset > moov.offset)
			return {false, "Unsupported layout: moov before mdat"};
	}

	if (!input.seek(static_cast<qint64>(moov.offset)))
		return {false, "Failed to seek moov atom"};
	const QByteArray moov_data = input.read(static_cast<qint64>(moov.size));
	if (moov_data.size() != static_cast<qint64>(moov.size))
		return {false, "Failed to read full moov atom"};

	const QByteArray new_moov = patch_moov(moov_data, xmp_payload, &error);
	if (new_moov.isEmpty())
		return {false, QString("Failed to patch moov atom: %1").arg(error)};

	const QString temp_path = media_path + ".better-markers.tmp";
	const QString backup_path = media_path + ".better-markers.bak";
	QFile::remove(temp_path);

	QFile output(temp_path);
	if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return {false, QString("Failed to open temp file: %1").arg(temp_path)};

	if (!copy_range(input, output, 0, moov.offset, &error)) {
		output.close();
		QFile::remove(temp_path);
		return {false, error};
	}
	if (output.write(new_moov) != new_moov.size()) {
		output.close();
		QFile::remove(temp_path);
		return {false, "Failed to write patched moov atom"};
	}

	const quint64 tail_offset = moov.offset + moov.size;
	const quint64 tail_size = static_cast<quint64>(input.size()) - tail_offset;
	if (!copy_range(input, output, tail_offset, tail_size, &error)) {
		output.close();
		QFile::remove(temp_path);
		return {false, error};
	}
	output.close();
	input.close();

	if (!has_xmp_atom(temp_path)) {
		QFile::remove(temp_path);
		return {false, "Embedded file validation failed (missing XMP_ atom)"};
	}

	QFile::remove(backup_path);
	if (!QFile::rename(media_path, backup_path)) {
		QFile::remove(temp_path);
		return {false, "Failed to create backup before atomic replace"};
	}

	if (!QFile::rename(temp_path, media_path)) {
		QFile::rename(backup_path, media_path);
		QFile::remove(temp_path);
		return {false, "Failed to replace original file with embedded file"};
	}

	QFile::remove(backup_path);
	return {true, {}};
}

} // namespace bm
