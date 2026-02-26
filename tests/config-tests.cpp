#include "bm-scope-store.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char *message)
{
	if (condition)
		return;
	std::cerr << "Config test failed: " << message << std::endl;
	std::exit(1);
}

void test_export_profile_defaults()
{
	const bm::ExportProfile profile = bm::export_profile_from_json(QJsonObject());
	require(profile.enable_premiere_xmp, "default enables Premiere XMP");
	require(!profile.enable_resolve_fcpxml, "default disables Resolve FCPXML");
	require(!profile.enable_final_cut_fcpxml, "default disables Final Cut FCPXML");
}

void test_export_profile_fallback_values()
{
	QJsonObject json_obj;
	json_obj.insert("enablePremiereXmp", false);
	json_obj.insert("enableResolveFcpxml", true);
	json_obj.insert("resolveMode", "unexpected");
	json_obj.insert("writeCadence", "unexpected");

	const bm::ExportProfile profile = bm::export_profile_from_json(json_obj);
	require(!profile.enable_premiere_xmp, "respects explicit Premiere toggle");
	require(profile.enable_resolve_fcpxml, "respects explicit Resolve toggle");
	require(profile.resolve_mode == bm::ResolveExportMode::TimelineMarkers, "resolve mode fallback");
	require(profile.write_cadence == bm::ExportWriteCadence::Immediate, "write cadence fallback");
}

void test_scope_store_migration_defaults()
{
	QTemporaryDir temp_dir;
	require(temp_dir.isValid(), "temporary directory created");

	const QString store_path = temp_dir.path() + "/global-store.json";
	{
		QFile file(store_path);
		require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "open old-format global store");
		QJsonObject legacy_store;
		legacy_store.insert("templates", QJsonArray());
		legacy_store.insert("hotkeys", QJsonObject());
		legacy_store.insert("quickHotkeys", QJsonObject());
		require(file.write(QJsonDocument(legacy_store).toJson(QJsonDocument::Compact)) != -1,
			"write old-format global store");
	}

	bm::ScopeStore store;
	store.set_base_dir(temp_dir.path());
	require(store.load_global(), "load global store");
	const bm::ExportProfile profile = store.export_profile();
	require(profile.enable_premiere_xmp, "legacy migration defaults to Premiere enabled");
	require(!profile.enable_resolve_fcpxml, "legacy migration defaults to Resolve disabled");
	require(!profile.enable_final_cut_fcpxml, "legacy migration defaults to Final Cut disabled");

	require(store.save_global(), "save migrated global store");
	QFile migrated(store_path);
	require(migrated.open(QIODevice::ReadOnly), "read migrated global store");
	const QJsonDocument doc = QJsonDocument::fromJson(migrated.readAll());
	require(doc.isObject(), "migrated global store is valid json object");
	const QJsonObject export_profile = doc.object().value("exportProfile").toObject();
	require(!export_profile.isEmpty(), "migrated global store includes exportProfile");
	require(export_profile.value("enablePremiereXmp").toBool(false), "migrated exportProfile Premiere value");
	require(doc.object().value("skippedUpdateTag").toString().isEmpty(), "migrated global store has empty skipped tag");
	require(doc.object().value("autoFocusMarkerDialog").toBool(false), "migrated global store has autofocus enabled");
}

void test_scope_store_skipped_update_tag_persistence()
{
	QTemporaryDir temp_dir;
	require(temp_dir.isValid(), "temporary directory created for skipped tag");

	bm::ScopeStore store;
	store.set_base_dir(temp_dir.path());
	require(store.load_global(), "load empty global store for skipped tag");
	require(store.skipped_update_tag().isEmpty(), "default skipped tag is empty");

	store.set_skipped_update_tag("v1.0.1");
	require(store.save_global(), "save global store with skipped tag");

	bm::ScopeStore reloaded;
	reloaded.set_base_dir(temp_dir.path());
	require(reloaded.load_global(), "reload global store with skipped tag");
	require(reloaded.skipped_update_tag() == "v1.0.1", "persisted skipped tag matches");
}

void test_scope_store_auto_focus_persistence()
{
	QTemporaryDir temp_dir;
	require(temp_dir.isValid(), "temporary directory created for auto focus");

	bm::ScopeStore store;
	store.set_base_dir(temp_dir.path());
	require(store.load_global(), "load empty global store for auto focus");
	require(store.auto_focus_marker_dialog(), "default auto focus is enabled");

	store.set_auto_focus_marker_dialog(false);
	require(store.save_global(), "save global store with auto focus disabled");

	bm::ScopeStore reloaded;
	reloaded.set_base_dir(temp_dir.path());
	require(reloaded.load_global(), "reload global store with auto focus disabled");
	require(!reloaded.auto_focus_marker_dialog(), "persisted auto focus value matches");
}

} // namespace

void run_config_tests()
{
	test_export_profile_defaults();
	test_export_profile_fallback_values();
	test_scope_store_migration_defaults();
	test_scope_store_skipped_update_tag_persistence();
	test_scope_store_auto_focus_persistence();
}
