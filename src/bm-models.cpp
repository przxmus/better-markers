#include "bm-models.hpp"

namespace bm {

const char *scope_to_key(TemplateScope scope)
{
	switch (scope) {
	case TemplateScope::Global:
		return "global";
	case TemplateScope::Profile:
		return "profile";
	case TemplateScope::SceneCollection:
	default:
		return "scene_collection";
	}
}

TemplateScope scope_from_key(const QString &scope_key)
{
	if (scope_key == "global")
		return TemplateScope::Global;
	if (scope_key == "profile")
		return TemplateScope::Profile;
	return TemplateScope::SceneCollection;
}

QJsonObject marker_template_to_json(const MarkerTemplate &templ)
{
	QJsonObject json_obj;
	json_obj.insert("id", templ.id);
	json_obj.insert("scope", scope_to_key(templ.scope));
	json_obj.insert("scopeTarget", templ.scope_target);
	json_obj.insert("name", templ.name);
	json_obj.insert("title", templ.title);
	json_obj.insert("description", templ.description);
	json_obj.insert("colorId", templ.color_id);
	json_obj.insert("editableTitle", templ.editable_title);
	json_obj.insert("editableDescription", templ.editable_description);
	json_obj.insert("editableColor", templ.editable_color);
	return json_obj;
}

MarkerTemplate marker_template_from_json(const QJsonObject &json_obj)
{
	MarkerTemplate templ;
	templ.id = json_obj.value("id").toString();
	templ.scope = scope_from_key(json_obj.value("scope").toString());
	templ.scope_target = json_obj.value("scopeTarget").toString();
	templ.name = json_obj.value("name").toString();
	templ.title = json_obj.value("title").toString();
	templ.description = json_obj.value("description").toString();
	templ.color_id = json_obj.value("colorId").toInt(0);
	templ.editable_title = json_obj.value("editableTitle").toBool(false);
	templ.editable_description = json_obj.value("editableDescription").toBool(false);
	templ.editable_color = json_obj.value("editableColor").toBool(false);
	return templ;
}

QJsonObject scoped_store_to_json(const ScopedStoreData &store)
{
	QJsonObject root;
	QJsonArray templates;

	for (const MarkerTemplate &templ : store.templates)
		templates.push_back(marker_template_to_json(templ));

	root.insert("templates", templates);
	root.insert("hotkeys", store.hotkey_bindings);
	root.insert("quickHotkeys", store.quick_hotkeys);
	return root;
}

ScopedStoreData scoped_store_from_json(const QJsonObject &json_obj)
{
	ScopedStoreData store;

	const QJsonArray templates = json_obj.value("templates").toArray();
	store.templates.reserve(templates.size());
	for (QJsonValue value : templates) {
		if (!value.isObject())
			continue;
		store.templates.push_back(marker_template_from_json(value.toObject()));
	}

	if (json_obj.contains("hotkeys") && json_obj.value("hotkeys").isObject())
		store.hotkey_bindings = json_obj.value("hotkeys").toObject();
	if (json_obj.contains("quickHotkeys") && json_obj.value("quickHotkeys").isObject())
		store.quick_hotkeys = json_obj.value("quickHotkeys").toObject();

	return store;
}

const char *resolve_export_mode_to_key(ResolveExportMode mode)
{
	switch (mode) {
	case ResolveExportMode::TimelineMarkers:
	default:
		return "timeline_markers";
	}
}

ResolveExportMode resolve_export_mode_from_key(const QString &mode_key)
{
	if (mode_key == "timeline_markers")
		return ResolveExportMode::TimelineMarkers;
	return ResolveExportMode::TimelineMarkers;
}

const char *export_write_cadence_to_key(ExportWriteCadence cadence)
{
	switch (cadence) {
	case ExportWriteCadence::Immediate:
	default:
		return "immediate";
	}
}

ExportWriteCadence export_write_cadence_from_key(const QString &cadence_key)
{
	if (cadence_key == "immediate")
		return ExportWriteCadence::Immediate;
	return ExportWriteCadence::Immediate;
}

QJsonObject export_profile_to_json(const ExportProfile &profile)
{
	QJsonObject json_obj;
	json_obj.insert("enablePremiereXmp", profile.enable_premiere_xmp);
	json_obj.insert("enableResolveFcpxml", profile.enable_resolve_fcpxml);
	json_obj.insert("enableFinalCutFcpxml", profile.enable_final_cut_fcpxml);
	json_obj.insert("resolveMode", resolve_export_mode_to_key(profile.resolve_mode));
	json_obj.insert("writeCadence", export_write_cadence_to_key(profile.write_cadence));
	return json_obj;
}

ExportProfile export_profile_from_json(const QJsonObject &json_obj)
{
	ExportProfile profile;
	if (!json_obj.isEmpty()) {
		profile.enable_premiere_xmp = json_obj.value("enablePremiereXmp").toBool(true);
		profile.enable_resolve_fcpxml = json_obj.value("enableResolveFcpxml").toBool(false);
		profile.enable_final_cut_fcpxml = json_obj.value("enableFinalCutFcpxml").toBool(false);
		profile.resolve_mode = resolve_export_mode_from_key(json_obj.value("resolveMode").toString("timeline_markers"));
		profile.write_cadence = export_write_cadence_from_key(json_obj.value("writeCadence").toString("immediate"));
	}
	return profile;
}

} // namespace bm
