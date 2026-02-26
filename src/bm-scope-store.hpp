#pragma once

#include "bm-models.hpp"

#include <QString>

namespace bm {

class ScopeStore {
public:
	ScopeStore() = default;

	void set_base_dir(const QString &base_dir);
	void set_profile_name(const QString &profile_name);
	void set_scene_collection_name(const QString &scene_collection_name);

	bool load_global();
	bool save_global() const;

	bool load_profile();
	bool save_profile() const;

	void load_scene(const QJsonObject &scene_store_json);
	QJsonObject save_scene() const;

	ScopedStoreData &for_scope(TemplateScope scope);
	const ScopedStoreData &for_scope(TemplateScope scope) const;

	QVector<MarkerTemplate> merged_templates() const;

	QString global_store_path() const;
	QString profile_store_path() const;
	QString current_profile_name() const;
	QString current_scene_collection_name() const;

private:
	void merge_legacy_templates(const ScopedStoreData &legacy_store, TemplateScope scope, const QString &target_name);
	bool has_template_id(const QString &id) const;

	QString m_base_dir;
	QString m_profile_name;
	QString m_scene_collection_name;
	ScopedStoreData m_global;
	ScopedStoreData m_profile;
	ScopedStoreData m_scene;
};

} // namespace bm
