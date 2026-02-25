#pragma once

#include "bm-models.hpp"

#include <QString>

namespace bm {

class ScopeStore {
public:
	ScopeStore() = default;

	void set_base_dir(const QString &base_dir);
	void set_profile_name(const QString &profile_name);

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

private:
	QString m_base_dir;
	QString m_profile_name;
	ScopedStoreData m_global;
	ScopedStoreData m_profile;
	ScopedStoreData m_scene;
};

} // namespace bm
