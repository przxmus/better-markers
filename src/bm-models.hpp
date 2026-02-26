#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace bm {

enum class TemplateScope {
	Global,
	Profile,
	SceneCollection,
};

struct MarkerTemplate {
	QString id;
	TemplateScope scope = TemplateScope::SceneCollection;
	QString scope_target;
	QString name;
	QString title;
	QString description;
	int color_id = 0;
	bool editable_title = false;
	bool editable_description = false;
	bool editable_color = false;
};

struct ScopedStoreData {
	QVector<MarkerTemplate> templates;
	QJsonObject hotkey_bindings;
	QJsonObject quick_hotkeys;
};

enum class ResolveExportMode {
	TimelineMarkers,
};

enum class ExportWriteCadence {
	Immediate,
};

struct ExportProfile {
	bool enable_premiere_xmp = true;
	bool enable_resolve_fcpxml = false;
	bool enable_final_cut_fcpxml = false;
	ResolveExportMode resolve_mode = ResolveExportMode::TimelineMarkers;
	ExportWriteCadence write_cadence = ExportWriteCadence::Immediate;
};

const char *scope_to_key(TemplateScope scope);
TemplateScope scope_from_key(const QString &scope_key);

QJsonObject marker_template_to_json(const MarkerTemplate &templ);
MarkerTemplate marker_template_from_json(const QJsonObject &json_obj);

QJsonObject scoped_store_to_json(const ScopedStoreData &store);
ScopedStoreData scoped_store_from_json(const QJsonObject &json_obj);

const char *resolve_export_mode_to_key(ResolveExportMode mode);
ResolveExportMode resolve_export_mode_from_key(const QString &mode_key);

const char *export_write_cadence_to_key(ExportWriteCadence cadence);
ExportWriteCadence export_write_cadence_from_key(const QString &cadence_key);

QJsonObject export_profile_to_json(const ExportProfile &profile);
ExportProfile export_profile_from_json(const QJsonObject &json_obj);

} // namespace bm
