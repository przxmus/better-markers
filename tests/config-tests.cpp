#include "bm-focus-policy.hpp"
#include "bm-scope-store.hpp"
#include "bm-startup-recovery-policy.hpp"

#include <QFile>
#include <QFileInfo>
#include <QDir>
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
	require(doc.object().value("skippedUpdateTag").toString().isEmpty(),
		"migrated global store has empty skipped tag");
	require(doc.object().value("autoFocusMarkerDialog").toBool(false),
		"migrated global store has autofocus enabled");
	require(doc.object().value("pauseRecordingDuringMarkerDialog").toBool(false),
		"migrated global store has pause-during-dialog enabled");
	require(!doc.object().value("syntheticKeypressAroundFocusEnabled").toBool(true),
		"migrated global store has synthetic keypress disabled");
	require(doc.object().value("syntheticKeypressBeforeFocus").toString() == "Esc",
		"migrated global store has default synthetic pre key");
	require(doc.object().value("syntheticKeypressAfterUnfocus").toString() == "Esc",
		"migrated global store has default synthetic post key");
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

void test_scope_store_pause_recording_during_dialog_persistence()
{
	QTemporaryDir temp_dir;
	require(temp_dir.isValid(), "temporary directory created for pause during dialog");

	bm::ScopeStore store;
	store.set_base_dir(temp_dir.path());
	require(store.load_global(), "load empty global store for pause during dialog");
	require(store.pause_recording_during_marker_dialog(), "default pause during dialog is enabled");

	store.set_pause_recording_during_marker_dialog(false);
	require(store.save_global(), "save global store with pause during dialog disabled");

	bm::ScopeStore reloaded;
	reloaded.set_base_dir(temp_dir.path());
	require(reloaded.load_global(), "reload global store with pause during dialog disabled");
	require(!reloaded.pause_recording_during_marker_dialog(), "persisted pause during dialog value matches");
}

void test_scope_store_synthetic_keypress_defaults()
{
	QTemporaryDir temp_dir;
	require(temp_dir.isValid(), "temporary directory created for synthetic defaults");

	bm::ScopeStore store;
	store.set_base_dir(temp_dir.path());
	require(store.load_global(), "load empty global store for synthetic defaults");
	require(!store.synthetic_keypress_around_focus_enabled(), "default synthetic keypress is disabled");
	require(store.synthetic_keypress_before_focus_portable() == "Esc", "default synthetic pre key is Esc");
	require(store.synthetic_keypress_after_unfocus_portable() == "Esc", "default synthetic post key is Esc");
}

void test_scope_store_synthetic_keypress_persistence()
{
	QTemporaryDir temp_dir;
	require(temp_dir.isValid(), "temporary directory created for synthetic keypress persistence");

	bm::ScopeStore store;
	store.set_base_dir(temp_dir.path());
	require(store.load_global(), "load empty global store for synthetic keypress");
	store.set_synthetic_keypress_around_focus_enabled(true);
	store.set_synthetic_keypress_before_focus_portable("Ctrl+Shift+K");
	store.set_synthetic_keypress_after_unfocus_portable("Alt+F12");
	require(store.save_global(), "save global store with synthetic keypress");

	bm::ScopeStore reloaded;
	reloaded.set_base_dir(temp_dir.path());
	require(reloaded.load_global(), "reload global store with synthetic keypress");
	require(reloaded.synthetic_keypress_around_focus_enabled(), "persisted synthetic enabled value matches");
	require(reloaded.synthetic_keypress_before_focus_portable() == "Ctrl+Shift+K",
		"persisted synthetic pre key matches");
	require(reloaded.synthetic_keypress_after_unfocus_portable() == "Alt+F12",
		"persisted synthetic post key matches");
}

void test_scope_store_synthetic_keypress_empty_values()
{
	QTemporaryDir temp_dir;
	require(temp_dir.isValid(), "temporary directory created for synthetic empty value persistence");

	bm::ScopeStore store;
	store.set_base_dir(temp_dir.path());
	require(store.load_global(), "load empty global store for synthetic empty value");
	store.set_synthetic_keypress_around_focus_enabled(true);
	store.set_synthetic_keypress_before_focus_portable("");
	store.set_synthetic_keypress_after_unfocus_portable("");
	require(store.save_global(), "save global store with empty synthetic key values");

	bm::ScopeStore reloaded;
	reloaded.set_base_dir(temp_dir.path());
	require(reloaded.load_global(), "reload global store with empty synthetic key values");
	require(reloaded.synthetic_keypress_around_focus_enabled(),
		"synthetic enabled remains true with empty key values");
	require(reloaded.synthetic_keypress_before_focus_portable().isEmpty(), "empty synthetic pre key remains empty");
	require(reloaded.synthetic_keypress_after_unfocus_portable().isEmpty(),
		"empty synthetic post key remains empty");
}

void test_focus_policy_hotkey_scope()
{
	require(!bm::should_use_hotkey_focus_session(false, true, true), "focus session disabled when auto focus off");
	require(!bm::should_use_hotkey_focus_session(true, false, true),
		"focus session disabled for non-hotkey trigger");
	require(!bm::should_use_hotkey_focus_session(true, true, false),
		"focus session disabled when dialog not shown");
	require(bm::should_use_hotkey_focus_session(true, true, true), "focus session enabled only for hotkey dialog");
}

void test_focus_policy_restore_condition()
{
	require(!bm::should_restore_focus_after_dialog(false, true), "restore disabled when focus session inactive");
	require(!bm::should_restore_focus_after_dialog(true, false), "restore disabled when dialog was not executed");
	require(bm::should_restore_focus_after_dialog(true, true), "restore enabled for active executed dialog");
}

void test_startup_recovery_drops_stale_jobs()
{
	QTemporaryDir temp_dir;
	require(temp_dir.isValid(), "temporary directory created for startup recovery stale job test");

	const bm::StartupRecoveryDecision missing_media =
		bm::decide_startup_recovery(temp_dir.path() + "/missing-recording.mp4");
	require(missing_media.action == bm::StartupRecoveryAction::DropMissingMedia,
		"missing media should be dropped");

	const QString unsupported_path = temp_dir.path() + "/recording.mkv";
	{
		QFile unsupported_file(unsupported_path);
		require(unsupported_file.open(QIODevice::WriteOnly | QIODevice::Truncate),
			"create unsupported extension media");
		require(unsupported_file.write("stub") == 4, "write unsupported extension media");
	}
	const bm::StartupRecoveryDecision unsupported = bm::decide_startup_recovery(unsupported_path);
	require(unsupported.action == bm::StartupRecoveryAction::DropUnsupportedMedia,
		"unsupported extension should be dropped");

	const QString missing_sidecar_media = temp_dir.path() + "/recording.mp4";
	{
		QFile media_file(missing_sidecar_media);
		require(media_file.open(QIODevice::WriteOnly | QIODevice::Truncate), "create mp4 without sidecar");
		require(media_file.write("stub") == 4, "write mp4 without sidecar");
	}
	const bm::StartupRecoveryDecision missing_sidecar = bm::decide_startup_recovery(missing_sidecar_media);
	require(missing_sidecar.action == bm::StartupRecoveryAction::DropMissingSidecar,
		"missing sidecar should be dropped");
}

void test_startup_recovery_retries_once()
{
	QTemporaryDir temp_dir;
	require(temp_dir.isValid(), "temporary directory created for startup recovery retry policy test");

	const QString media_path = temp_dir.path() + "/recording.mov";
	{
		QFile media_file(media_path);
		require(media_file.open(QIODevice::WriteOnly | QIODevice::Truncate), "create mov with sidecar");
		require(media_file.write("stub") == 4, "write mov with sidecar");
	}

	const QFileInfo info(media_path);
	const QString sidecar_path = info.dir().filePath(info.completeBaseName() + ".xmp");
	{
		QFile sidecar_file(sidecar_path);
		require(sidecar_file.open(QIODevice::WriteOnly | QIODevice::Truncate), "create sidecar for startup retry");
		require(sidecar_file.write("<xmp/>") == 6, "write sidecar for startup retry");
	}

	const bm::StartupRecoveryDecision decision = bm::decide_startup_recovery(media_path);
	require(decision.action == bm::StartupRecoveryAction::RetryOnce, "valid job should be retried");
	require(decision.sidecar_path == sidecar_path, "startup retry sidecar path should match");
	require(bm::startup_recovery_retry_attempts() == 1, "startup retry attempts should be exactly one");
}

} // namespace

void run_config_tests()
{
	test_export_profile_defaults();
	test_export_profile_fallback_values();
	test_scope_store_migration_defaults();
	test_scope_store_skipped_update_tag_persistence();
	test_scope_store_auto_focus_persistence();
	test_scope_store_pause_recording_during_dialog_persistence();
	test_scope_store_synthetic_keypress_defaults();
	test_scope_store_synthetic_keypress_persistence();
	test_scope_store_synthetic_keypress_empty_values();
	test_focus_policy_hotkey_scope();
	test_focus_policy_restore_condition();
	test_startup_recovery_drops_stale_jobs();
	test_startup_recovery_retries_once();
}
