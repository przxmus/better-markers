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

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <util/base.h>

#include <QAction>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

#include "bm-hotkey-registry.hpp"
#include "bm-localization.hpp"
#include "bm-marker-controller.hpp"
#include "bm-recording-session-tracker.hpp"
#include "bm-settings-dialog.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

constexpr const char *SCENE_STORE_KEY = "better-markers.scene-store";
constexpr const char *DOCK_ID = "better-markers.dock";

class BetterMarkersPlugin {
public:
	bool load()
	{
		char *config_path = obs_module_config_path("stores");
		if (!config_path)
			return false;
		m_store_base_dir = QString::fromUtf8(config_path);
		bfree(config_path);

		m_store.set_base_dir(m_store_base_dir);
		reload_profile_store();
		reload_scene_collection_store();
		m_store.load_global();

		QMainWindow *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		m_controller = std::make_unique<bm::MarkerController>(&m_store, &m_tracker, main_window, m_store_base_dir);
		m_tracker.set_file_changed_callback([this](const QString &closed_file, const QString &next_file) {
			if (m_controller)
				m_controller->on_recording_file_changed(closed_file, next_file);
		});
		m_tracker.set_recording_stopped_callback([this](const QString &closed_file) {
			if (m_controller)
				m_controller->on_recording_stopped(closed_file);
		});

		m_hotkeys = std::make_unique<bm::HotkeyRegistry>(&m_store);
		m_hotkeys->set_callbacks(
			[this](bool custom) {
				if (!m_controller)
					return;
				if (custom)
					m_controller->quick_custom_marker();
				else
					m_controller->quick_marker();
			},
			[this](const bm::MarkerTemplate &templ) {
				if (m_controller)
					m_controller->add_marker_from_template_hotkey(templ);
			});
		m_hotkeys->initialize();

		obs_frontend_add_save_callback(&BetterMarkersPlugin::on_frontend_save, this);
		obs_frontend_add_event_callback(&BetterMarkersPlugin::on_frontend_event, this);

		m_settings_action = static_cast<QAction *>(
			obs_frontend_add_tools_menu_qaction(bm::bm_text("BetterMarkers.SettingsMenu").toUtf8().constData()));
		if (m_settings_action) {
			QObject::connect(m_settings_action, &QAction::triggered, [this]() { show_settings_dialog(); });
		}

		create_main_dock(main_window);
		refresh_runtime_bindings();
		m_tracker.sync_from_frontend_state();
		m_controller->retry_recovery_queue();

		obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
		return true;
	}

	void unload()
	{
		obs_frontend_remove_save_callback(&BetterMarkersPlugin::on_frontend_save, this);
		obs_frontend_remove_event_callback(&BetterMarkersPlugin::on_frontend_event, this);

		if (m_hotkeys)
			m_hotkeys->shutdown();
		m_hotkeys.reset();

		m_tracker.shutdown();
		m_controller.reset();

		if (m_settings_dialog)
			delete m_settings_dialog;
		m_settings_dialog = nullptr;

		obs_frontend_remove_dock(DOCK_ID);
		if (m_dock_widget)
			m_dock_widget->deleteLater();
		m_dock_widget = nullptr;

		obs_log(LOG_INFO, "plugin unloaded");
	}

private:
	static void on_frontend_save(obs_data_t *save_data, bool saving, void *private_data)
	{
		auto *self = static_cast<BetterMarkersPlugin *>(private_data);
		if (saving) {
			if (self->m_hotkeys)
				self->m_hotkeys->save_bindings();
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
		self->refresh_runtime_bindings();
		if (self->m_settings_dialog)
			self->m_settings_dialog->refresh();
	}

	static void on_frontend_event(enum obs_frontend_event event, void *private_data)
	{
		auto *self = static_cast<BetterMarkersPlugin *>(private_data);
		self->m_tracker.handle_frontend_event(event);

		if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGING || event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING) {
			if (self->m_hotkeys)
				self->m_hotkeys->save_bindings();
			self->persist_non_scene();
		}

		if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGED) {
			self->reload_profile_store();
			self->refresh_runtime_bindings();
			if (self->m_settings_dialog)
				self->m_settings_dialog->refresh();
			return;
		}

		if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
			self->reload_scene_collection_store();
			self->refresh_runtime_bindings();
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
				refresh_runtime_bindings();
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
		m_store.save_profile(); // Kept for backwards compatibility with legacy scope stores.
	}

	void refresh_runtime_bindings()
	{
		const QVector<bm::MarkerTemplate> active_templates = m_store.merged_templates();
		if (m_controller)
			m_controller->set_active_templates(active_templates);
		if (m_hotkeys)
			m_hotkeys->refresh_templates(active_templates);
	}

	void create_main_dock(QMainWindow *main_window)
	{
		m_dock_widget = new QWidget(main_window);
		auto *layout = new QVBoxLayout(m_dock_widget);
		layout->setContentsMargins(8, 8, 8, 8);

		auto *add_marker_button = new QPushButton(bm::bm_text("BetterMarkers.AddMarkerButton"), m_dock_widget);
		layout->addWidget(add_marker_button);
		layout->addStretch(1);

		QObject::connect(add_marker_button, &QPushButton::clicked, [this]() {
			if (m_controller)
				m_controller->add_marker_from_main_button();
		});

		obs_frontend_add_dock_by_id(DOCK_ID, bm::bm_text("BetterMarkers.DockTitle").toUtf8().constData(),
					    m_dock_widget);
	}

	void reload_scene_collection_store()
	{
		char *scene_collection = obs_frontend_get_current_scene_collection();
		m_store.set_scene_collection_name(QString::fromUtf8(scene_collection ? scene_collection : ""));
		if (scene_collection)
			bfree(scene_collection);
	}

	QString m_store_base_dir;
	bm::ScopeStore m_store;
	bm::RecordingSessionTracker m_tracker;
	std::unique_ptr<bm::MarkerController> m_controller;
	std::unique_ptr<bm::HotkeyRegistry> m_hotkeys;

	QAction *m_settings_action = nullptr;
	QWidget *m_dock_widget = nullptr;
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
