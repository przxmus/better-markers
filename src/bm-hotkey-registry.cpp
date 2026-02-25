#include "bm-hotkey-registry.hpp"

#include <obs-data.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace bm {
namespace {

QJsonArray obs_array_to_qjson(obs_data_array_t *array)
{
	QJsonArray json_array;
	if (!array)
		return json_array;

	const size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; ++i) {
		obs_data_t *item = obs_data_array_item(array, i);
		if (!item)
			continue;

		const char *json = obs_data_get_json(item);
		const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json ? json : "{}"));
		if (doc.isObject())
			json_array.push_back(doc.object());
		obs_data_release(item);
	}

	return json_array;
}

obs_data_array_t *qjson_to_obs_array(const QJsonValue &value)
{
	obs_data_array_t *array = obs_data_array_create();
	if (!value.isArray())
		return array;

	const QJsonArray json_array = value.toArray();
	for (QJsonValue entry : json_array) {
		if (!entry.isObject())
			continue;

		const QByteArray json = QJsonDocument(entry.toObject()).toJson(QJsonDocument::Compact);
		obs_data_t *item = obs_data_create_from_json(json.constData());
		if (!item)
			continue;

		obs_data_array_push_back(array, item);
		obs_data_release(item);
	}

	return array;
}

} // namespace

HotkeyRegistry::HotkeyRegistry(ScopeStore *store) : m_store(store)
{
}

HotkeyRegistry::~HotkeyRegistry()
{
	shutdown();
}

void HotkeyRegistry::set_callbacks(QuickCallback quick_cb, TemplateCallback templ_cb)
{
	m_quick_cb = std::move(quick_cb);
	m_template_cb = std::move(templ_cb);
}

void HotkeyRegistry::initialize()
{
	register_quick_hotkeys();
}

void HotkeyRegistry::refresh_templates(const QVector<MarkerTemplate> &active_templates)
{
	unregister_template_hotkeys();
	register_template_hotkeys(active_templates);
}

void HotkeyRegistry::save_bindings()
{
	if (!m_store)
		return;

	if (m_quick_marker != OBS_INVALID_HOTKEY_ID)
		m_store->for_scope(TemplateScope::Global).quick_hotkeys.insert("quickMarker", save_hotkey_to_json(m_quick_marker));
	if (m_quick_custom_marker != OBS_INVALID_HOTKEY_ID) {
		m_store->for_scope(TemplateScope::Global)
			.quick_hotkeys.insert("quickCustomMarker", save_hotkey_to_json(m_quick_custom_marker));
	}

	for (const TemplateHotkey &templ_hotkey : m_template_hotkeys) {
		if (templ_hotkey.hotkey_id == OBS_INVALID_HOTKEY_ID)
			continue;

		m_store->for_scope(templ_hotkey.templ.scope)
			.hotkey_bindings.insert(templ_hotkey.templ.id, save_hotkey_to_json(templ_hotkey.hotkey_id));
	}
}

void HotkeyRegistry::shutdown()
{
	save_bindings();
	unregister_template_hotkeys();
	unregister_quick_hotkeys();
}

void HotkeyRegistry::quick_marker_callback(void *data, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
	if (!data || !pressed)
		return;

	auto *self = static_cast<HotkeyRegistry *>(data);
	if (!self->m_quick_cb)
		return;

	const bool custom = (id == self->m_quick_custom_marker);
	self->m_quick_cb(custom);
}

void HotkeyRegistry::template_callback(void *data, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
	if (!data || !pressed)
		return;

	auto *self = static_cast<HotkeyRegistry *>(data);
	if (!self->m_template_cb)
		return;

	for (const TemplateHotkey &templ_hotkey : self->m_template_hotkeys) {
		if (templ_hotkey.hotkey_id == id) {
			self->m_template_cb(templ_hotkey.templ);
			return;
		}
	}
}

void HotkeyRegistry::register_quick_hotkeys()
{
	if (!m_store)
		return;

	m_quick_marker = obs_hotkey_register_frontend("BetterMarkers.QuickMarker", "Better Markers: Quick Marker",
						      &HotkeyRegistry::quick_marker_callback, this);
	m_quick_custom_marker = obs_hotkey_register_frontend("BetterMarkers.QuickCustomMarker",
						     "Better Markers: Quick Custom Marker",
						     &HotkeyRegistry::quick_marker_callback, this);

	const QJsonObject quick = m_store->for_scope(TemplateScope::Global).quick_hotkeys;
	load_hotkey_from_json(m_quick_marker, quick.value("quickMarker"));
	load_hotkey_from_json(m_quick_custom_marker, quick.value("quickCustomMarker"));
}

void HotkeyRegistry::unregister_quick_hotkeys()
{
	if (m_quick_marker != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(m_quick_marker);
	if (m_quick_custom_marker != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(m_quick_custom_marker);

	m_quick_marker = OBS_INVALID_HOTKEY_ID;
	m_quick_custom_marker = OBS_INVALID_HOTKEY_ID;
}

void HotkeyRegistry::register_template_hotkeys(const QVector<MarkerTemplate> &active_templates)
{
	if (!m_store)
		return;

	m_template_hotkeys.reserve(active_templates.size());
	for (const MarkerTemplate &templ : active_templates) {
		if (templ.id.isEmpty())
			continue;

		const QString name = make_hotkey_name(templ);
		const QString desc = make_hotkey_desc(templ);
		const obs_hotkey_id hotkey_id = obs_hotkey_register_frontend(name.toUtf8().constData(),
									 desc.toUtf8().constData(),
									 &HotkeyRegistry::template_callback, this);

		TemplateHotkey templ_hotkey;
		templ_hotkey.templ = templ;
		templ_hotkey.hotkey_id = hotkey_id;
		m_template_hotkeys.push_back(templ_hotkey);

		const QJsonObject scope_bindings = m_store->for_scope(templ.scope).hotkey_bindings;
		load_hotkey_from_json(hotkey_id, scope_bindings.value(templ.id));
	}
}

void HotkeyRegistry::unregister_template_hotkeys()
{
	for (const TemplateHotkey &templ_hotkey : m_template_hotkeys) {
		if (templ_hotkey.hotkey_id != OBS_INVALID_HOTKEY_ID)
			obs_hotkey_unregister(templ_hotkey.hotkey_id);
	}
	m_template_hotkeys.clear();
}

void HotkeyRegistry::load_hotkey_from_json(obs_hotkey_id hotkey_id, const QJsonValue &bindings_json) const
{
	if (hotkey_id == OBS_INVALID_HOTKEY_ID)
		return;

	obs_data_array_t *array = qjson_to_obs_array(bindings_json);
	obs_hotkey_load(hotkey_id, array);
	obs_data_array_release(array);
}

QJsonArray HotkeyRegistry::save_hotkey_to_json(obs_hotkey_id hotkey_id) const
{
	if (hotkey_id == OBS_INVALID_HOTKEY_ID)
		return {};

	obs_data_array_t *array = obs_hotkey_save(hotkey_id);
	const QJsonArray result = obs_array_to_qjson(array);
	obs_data_array_release(array);
	return result;
}

QString HotkeyRegistry::make_hotkey_name(const MarkerTemplate &templ)
{
	return QString("BetterMarkers.Template.%1.%2").arg(scope_to_key(templ.scope), sanitize(templ.id));
}

QString HotkeyRegistry::make_hotkey_desc(const MarkerTemplate &templ)
{
	return QString("Better Markers: Add Marker (%1)").arg(templ.name);
}

QString HotkeyRegistry::sanitize(const QString &value)
{
	QString out;
	out.reserve(value.size());
	for (const QChar ch : value) {
		if (ch.isLetterOrNumber())
			out.push_back(ch);
		else
			out.push_back('_');
	}
	return out;
}

} // namespace bm
