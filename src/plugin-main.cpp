/*
Plugin Name
Copyright (C) <Year> <Developer> <Email Address>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/base.h>
#include <util/platform.h>

#include <QAction>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <memory>

#include "bm-settings-dialog.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

constexpr const char *SCENE_STORE_KEY = "better-markers.scene-store";

class BetterMarkersPlugin {
public:
	bool load()
	{
		char *config_path = obs_module_config_path("stores");
		if (!config_path)
			return false;

		m_store.set_base_dir(QString::fromUtf8(config_path));
		bfree(config_path);
		reload_profile_store();
		m_store.load_global();

		obs_frontend_add_save_callback(&BetterMarkersPlugin::on_frontend_save, this);
		obs_frontend_add_event_callback(&BetterMarkersPlugin::on_frontend_event, this);

		m_settings_action = static_cast<QAction *>(
			obs_frontend_add_tools_menu_qaction(obs_module_text("BetterMarkers.SettingsMenu")));
		if (m_settings_action) {
			QObject::connect(m_settings_action, &QAction::triggered, [this]() { show_settings_dialog(); });
		}

		obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
		return true;
	}

	void unload()
	{
		obs_frontend_remove_save_callback(&BetterMarkersPlugin::on_frontend_save, this);
		obs_frontend_remove_event_callback(&BetterMarkersPlugin::on_frontend_event, this);

		if (m_settings_dialog)
			delete m_settings_dialog;
		m_settings_dialog = nullptr;

		obs_log(LOG_INFO, "plugin unloaded");
	}

private:
	static void on_frontend_save(obs_data_t *save_data, bool saving, void *private_data)
	{
		auto *self = static_cast<BetterMarkersPlugin *>(private_data);
		if (saving) {
			self->persist_non_scene();

			const QJsonObject scene_store_json = self->m_store.save_scene();
			const QByteArray json = QJsonDocument(scene_store_json).toJson(QJsonDocument::Compact);
			obs_data_t *scene_obj = obs_data_create_from_json(json.constData());
			if (scene_obj) {
				obs_data_set_obj(save_data, SCENE_STORE_KEY, scene_obj);
				obs_data_release(scene_obj);
			}
			return;
		}

		obs_data_t *scene_obj = obs_data_get_obj(save_data, SCENE_STORE_KEY);
		QJsonObject scene_store_json;
		if (scene_obj) {
			const char *json = obs_data_get_json(scene_obj);
			const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json ? json : "{}"));
			if (doc.isObject())
				scene_store_json = doc.object();
			obs_data_release(scene_obj);
		}

		self->m_store.load_scene(scene_store_json);
		if (self->m_settings_dialog)
			self->m_settings_dialog->refresh();
	}

	static void on_frontend_event(enum obs_frontend_event event, void *private_data)
	{
		auto *self = static_cast<BetterMarkersPlugin *>(private_data);
		if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGED) {
			self->reload_profile_store();
			if (self->m_settings_dialog)
				self->m_settings_dialog->refresh();
		}
	}

	void show_settings_dialog()
	{
		if (!m_settings_dialog) {
			QMainWindow *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
			m_settings_dialog = new bm::SettingsDialog(&m_store, main_window);
			m_settings_dialog->set_persist_callback([this]() {
				persist_non_scene();
			});
		}

		m_settings_dialog->refresh();
		m_settings_dialog->show();
		m_settings_dialog->raise();
		m_settings_dialog->activateWindow();
	}

	void reload_profile_store()
	{
		char *profile_name = obs_frontend_get_current_profile();
		m_store.set_profile_name(QString::fromUtf8(profile_name ? profile_name : ""));
		if (profile_name)
			bfree(profile_name);
		m_store.load_profile();
	}

	void persist_non_scene()
	{
		m_store.save_global();
		m_store.save_profile();
	}

	bm::ScopeStore m_store;
	QAction *m_settings_action = nullptr;
	bm::SettingsDialog *m_settings_dialog = nullptr;
};

std::unique_ptr<BetterMarkersPlugin> g_plugin;

} // namespace

bool obs_module_load(void)
{
	g_plugin = std::make_unique<BetterMarkersPlugin>();
	if (!g_plugin->load()) {
		g_plugin.reset();
		return false;
	}
	return true;
}

void obs_module_unload(void)
{
	if (g_plugin) {
		g_plugin->unload();
		g_plugin.reset();
	}
}
