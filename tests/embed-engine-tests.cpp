#include "bm-mp4-mov-embed-engine.hpp"

#include <QFile>
#include <QTemporaryDir>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {

void require_embed(bool condition, const char *message)
{
	if (condition)
		return;
	std::cerr << "Embed test failed: " << message << std::endl;
	std::exit(1);
}

QByteArray invalid_top_level_atom_file()
{
	QByteArray bytes;
	bytes.resize(8);
	bytes[0] = 0x00;
	bytes[1] = 0x00;
	bytes[2] = 0x00;
	bytes[3] = 0x20;
	bytes[4] = 'f';
	bytes[5] = 'r';
	bytes[6] = 'e';
	bytes[7] = 'e';
	return bytes;
}

QByteArray valid_single_free_atom_file()
{
	QByteArray bytes;
	bytes.resize(8);
	bytes[0] = 0x00;
	bytes[1] = 0x00;
	bytes[2] = 0x00;
	bytes[3] = 0x08;
	bytes[4] = 'f';
	bytes[5] = 'r';
	bytes[6] = 'e';
	bytes[7] = 'e';
	return bytes;
}

QByteArray sample_xmp_payload()
{
	return QByteArray(
		"<?xpacket begin=\"\"?><x:xmpmeta xmlns:x=\"adobe:ns:meta/\"></x:xmpmeta><?xpacket end=\"w\"?>");
}

void write_file_or_fail(const QString &path, const QByteArray &contents, const char *message)
{
	QFile file(path);
	require_embed(file.open(QIODevice::WriteOnly | QIODevice::Truncate), message);
	require_embed(file.write(contents) == contents.size(), message);
}

class ScopedEnvVar {
public:
	explicit ScopedEnvVar(const char *name)
		: m_name(name),
		  m_old_value(qgetenv(name)),
		  m_had_value(!m_old_value.isNull())
	{
	}
	~ScopedEnvVar()
	{
		if (m_had_value)
			qputenv(m_name, m_old_value);
		else
			qunsetenv(m_name);
	}

private:
	const char *m_name;
	QByteArray m_old_value;
	bool m_had_value = false;
};

void test_parse_error_is_retryable()
{
	QTemporaryDir temp_dir;
	require_embed(temp_dir.isValid(), "temporary directory created for retryable parse test");

	const QString media_path = temp_dir.path() + "/recording.mp4";
	write_file_or_fail(media_path, invalid_top_level_atom_file(), "write invalid media file");

	bm::Mp4MovEmbedEngine engine;
	const bm::EmbedResult result = engine.embed_xmp(media_path, sample_xmp_payload());
	require_embed(!result.ok, "embed fails for invalid atom size");
	require_embed(result.retryable, "invalid top-level atom parse should be retryable");
	require_embed(result.error.contains("Failed to parse MP4/MOV atoms"), "parse failure reason surfaced");
}

void test_retry_succeeds_after_media_stabilizes()
{
	QTemporaryDir temp_dir;
	require_embed(temp_dir.isValid(), "temporary directory created for retry success test");

	const QString media_path = temp_dir.path() + "/recording.mp4";
	const QString sidecar_path = temp_dir.path() + "/recording.xmp";

	write_file_or_fail(media_path, invalid_top_level_atom_file(), "write initial invalid media file");
	write_file_or_fail(sidecar_path, sample_xmp_payload(), "write sidecar file");

	std::thread make_file_valid([media_path]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(180));
		write_file_or_fail(media_path, valid_single_free_atom_file(),
				   "rewrite media file to valid atom layout");
	});

	bm::Mp4MovEmbedEngine engine;
	const bm::EmbedResult result = engine.embed_from_sidecar_with_retry(media_path, sidecar_path, 6, 100, 400);
	make_file_valid.join();

	require_embed(result.ok, "embed succeeds once media file becomes valid");

	QFile embedded(media_path);
	require_embed(embedded.open(QIODevice::ReadOnly), "open embedded media file");
	const QByteArray bytes = embedded.readAll();
	require_embed(bytes.contains("<?xpacket"), "embedded media contains xmp payload");
}

void test_empty_sidecar_is_not_retryable()
{
	QTemporaryDir temp_dir;
	require_embed(temp_dir.isValid(), "temporary directory created for non-retryable sidecar test");

	const QString media_path = temp_dir.path() + "/recording.mp4";
	const QString sidecar_path = temp_dir.path() + "/recording.xmp";

	write_file_or_fail(media_path, valid_single_free_atom_file(), "write valid media file");
	write_file_or_fail(sidecar_path, QByteArray(), "write empty sidecar file");

	bm::Mp4MovEmbedEngine engine;
	const bm::EmbedResult result = engine.embed_from_sidecar_with_retry(media_path, sidecar_path, 8, 50, 200);
	require_embed(!result.ok, "embed fails for empty sidecar");
	require_embed(!result.retryable, "empty sidecar is non-retryable");
	require_embed(result.error.contains("Sidecar is empty"), "empty sidecar reason surfaced");
}

void test_fallback_works_when_exiftool_fails()
{
	QTemporaryDir temp_dir;
	require_embed(temp_dir.isValid(), "temporary directory created for exiftool fallback test");

	const QString media_path = temp_dir.path() + "/recording.mp4";
	const QString sidecar_path = temp_dir.path() + "/recording.xmp";
	const QString exiftool_path = temp_dir.path() + "/exiftool";

	write_file_or_fail(media_path, valid_single_free_atom_file(), "write valid media file for fallback test");
	write_file_or_fail(sidecar_path, sample_xmp_payload(), "write sidecar file for fallback test");
	write_file_or_fail(exiftool_path, QByteArray("#!/bin/sh\nexit 1\n"), "write fake exiftool script");
	require_embed(QFile::setPermissions(exiftool_path,
					    QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner),
		      "mark fake exiftool script executable");

	ScopedEnvVar scoped_path("PATH");
	const QByteArray old_path = qgetenv("PATH");
	const QByteArray test_path = temp_dir.path().toUtf8() + ":" + old_path;
	require_embed(qputenv("PATH", test_path), "set PATH for exiftool fallback test");

	bm::Mp4MovEmbedEngine engine;
	const bm::EmbedResult result = engine.embed_from_sidecar(media_path, sidecar_path);
	require_embed(result.ok, "embed falls back to internal engine when exiftool exits non-zero");
}

} // namespace

void run_embed_engine_tests()
{
	test_parse_error_is_retryable();
	test_retry_succeeds_after_media_stabilizes();
	test_empty_sidecar_is_not_retryable();
	test_fallback_works_when_exiftool_fails();
}
