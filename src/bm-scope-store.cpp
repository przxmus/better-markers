#include "bm-scope-store.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

namespace bm {
namespace {

bool read_json_file(const QString &path, QJsonObject *out_obj)
{
	QFile file(path);
	if (!file.exists()) {
		*out_obj = QJsonObject();
		return true;
	}
	if (!file.open(QIODevice::ReadOnly))
		return false;

	QJsonParseError parse_error;
	const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parse_error);
	if (parse_error.error != QJsonParseError::NoError || !doc.isObject())
		return false;

	*out_obj = doc.object();
	return true;
}

bool write_json_file(const QString &path, const QJsonObject &json_obj)
{
	QFileInfo info(path);
	QDir dir = info.dir();
	if (!dir.exists() && !dir.mkpath("."))
		return false;

	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;

	const QJsonDocument doc(json_obj);
	if (file.write(doc.toJson(QJsonDocument::Indented)) == -1)
		return false;

	return file.commit();
}

} // namespace

void ScopeStore::set_base_dir(const QString &base_dir)
{
	m_base_dir = base_dir;
}

void ScopeStore::set_profile_name(const QString &profile_name)
{
	m_profile_name = profile_name;
}

void ScopeStore::set_scene_collection_name(const QString &scene_collection_name)
{
	m_scene_collection_name = scene_collection_name;
}

bool ScopeStore::load_global()
{
	QJsonObject json_obj;
	if (!read_json_file(global_store_path(), &json_obj))
		return false;

	m_global = scoped_store_from_json(json_obj);
	return true;
}

bool ScopeStore::save_global() const
{
	return write_json_file(global_store_path(), scoped_store_to_json(m_global));
}

bool ScopeStore::load_profile()
{
	QJsonObject json_obj;
	if (!read_json_file(profile_store_path(), &json_obj))
		return false;

	m_profile = scoped_store_from_json(json_obj);
	merge_legacy_templates(m_profile, TemplateScope::Profile, m_profile_name);
	return true;
}

bool ScopeStore::save_profile() const
{
	return write_json_file(profile_store_path(), scoped_store_to_json(m_profile));
}

void ScopeStore::load_scene(const QJsonObject &scene_store_json)
{
	m_scene = scoped_store_from_json(scene_store_json);
	merge_legacy_templates(m_scene, TemplateScope::SceneCollection, m_scene_collection_name);
}

QJsonObject ScopeStore::save_scene() const
{
	return scoped_store_to_json(m_scene);
}

ScopedStoreData &ScopeStore::for_scope(TemplateScope scope)
{
	switch (scope) {
	case TemplateScope::Global:
		return m_global;
	case TemplateScope::Profile:
		return m_profile;
	case TemplateScope::SceneCollection:
	default:
		return m_scene;
	}
}

const ScopedStoreData &ScopeStore::for_scope(TemplateScope scope) const
{
	switch (scope) {
	case TemplateScope::Global:
		return m_global;
	case TemplateScope::Profile:
		return m_profile;
	case TemplateScope::SceneCollection:
	default:
		return m_scene;
	}
}

QVector<MarkerTemplate> ScopeStore::merged_templates() const
{
	QVector<MarkerTemplate> templates;
	templates.reserve(m_global.templates.size());

	for (const MarkerTemplate &templ : m_global.templates) {
		if (templ.scope == TemplateScope::Global) {
			templates.push_back(templ);
			continue;
		}

		if (templ.scope == TemplateScope::Profile) {
			if (templ.scope_target.isEmpty() || templ.scope_target == m_profile_name)
				templates.push_back(templ);
			continue;
		}

		if (templ.scope_target.isEmpty() || templ.scope_target == m_scene_collection_name)
			templates.push_back(templ);
	}

	return templates;
}

QString ScopeStore::global_store_path() const
{
	return m_base_dir + "/global-store.json";
}

QString ScopeStore::profile_store_path() const
{
	QString safe_profile = m_profile_name;
	if (safe_profile.isEmpty())
		safe_profile = "default-profile";

	safe_profile.replace('/', '_');
	safe_profile.replace('\\', '_');
	safe_profile.replace(':', '_');
	return m_base_dir + "/profiles/" + safe_profile + "/profile-store.json";
}

QString ScopeStore::current_profile_name() const
{
	return m_profile_name;
}

QString ScopeStore::current_scene_collection_name() const
{
	return m_scene_collection_name;
}

void ScopeStore::merge_legacy_templates(const ScopedStoreData &legacy_store, TemplateScope scope, const QString &target_name)
{
	for (MarkerTemplate templ : legacy_store.templates) {
		if (templ.id.isEmpty() || has_template_id(templ.id))
			continue;
		templ.scope = scope;
		if (templ.scope != TemplateScope::Global && templ.scope_target.trimmed().isEmpty())
			templ.scope_target = target_name;
		m_global.templates.push_back(templ);
	}
}

bool ScopeStore::has_template_id(const QString &id) const
{
	for (const MarkerTemplate &templ : m_global.templates) {
		if (templ.id == id)
			return true;
	}
	return false;
}

} // namespace bm
