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

const char *scope_to_key(TemplateScope scope);
TemplateScope scope_from_key(const QString &scope_key);

QJsonObject marker_template_to_json(const MarkerTemplate &templ);
MarkerTemplate marker_template_from_json(const QJsonObject &json_obj);

QJsonObject scoped_store_to_json(const ScopedStoreData &store);
ScopedStoreData scoped_store_from_json(const QJsonObject &json_obj);

} // namespace bm
