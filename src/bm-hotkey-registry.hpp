#pragma once

#include "bm-models.hpp"
#include "bm-scope-store.hpp"

#include <obs.h>

#include <functional>

namespace bm {

class HotkeyRegistry {
public:
	using QuickCallback = std::function<void(bool custom)>;
	using TemplateCallback = std::function<void(const MarkerTemplate &templ)>;

	explicit HotkeyRegistry(ScopeStore *store);
	~HotkeyRegistry();

	void set_callbacks(QuickCallback quick_cb, TemplateCallback templ_cb);

	void initialize();
	void refresh_templates(const QVector<MarkerTemplate> &active_templates);
	void save_bindings();
	void shutdown();

private:
	struct TemplateHotkey {
		MarkerTemplate templ;
		obs_hotkey_id hotkey_id = OBS_INVALID_HOTKEY_ID;
	};

	static void quick_marker_callback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void template_callback(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

	void register_quick_hotkeys();
	void unregister_quick_hotkeys();

	void register_template_hotkeys(const QVector<MarkerTemplate> &active_templates);
	void unregister_template_hotkeys();

	void load_hotkey_from_json(obs_hotkey_id hotkey_id, const QJsonValue &bindings_json) const;
	QJsonArray save_hotkey_to_json(obs_hotkey_id hotkey_id) const;

	static QString make_hotkey_name(const MarkerTemplate &templ);
	static QString make_hotkey_desc(const MarkerTemplate &templ);
	static QString sanitize(const QString &value);

	ScopeStore *m_store = nullptr;
	QuickCallback m_quick_cb;
	TemplateCallback m_template_cb;

	obs_hotkey_id m_quick_marker = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id m_quick_custom_marker = OBS_INVALID_HOTKEY_ID;

	QVector<TemplateHotkey> m_template_hotkeys;
};

} // namespace bm
