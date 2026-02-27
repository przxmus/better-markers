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
#include <util/platform.h>

#include <curl/curl.h>

#include <QAction>
#include <QDesktopServices>
#include <QDialog>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QVersionNumber>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

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
constexpr const char *LATEST_RELEASE_API_URL = "https://api.github.com/repos/przxmus/better-markers/releases/latest";
constexpr int UPDATE_ACTION_SKIP = 1;
constexpr int UPDATE_ACTION_IGNORE = 2;
constexpr int UPDATE_ACTION_INSTALL = 3;

struct ReleaseDetails {
	QString tag;
	QString url;
};

QString normalize_release_tag(const QString &tag)
{
	QString normalized = tag.trimmed();
	if (normalized.startsWith('v', Qt::CaseInsensitive))
		normalized.remove(0, 1);
	return normalized;
}

bool release_tags_match(const QString &lhs, const QString &rhs)
{
	const QString lhs_tag = normalize_release_tag(lhs);
	const QString rhs_tag = normalize_release_tag(rhs);
	if (lhs_tag.compare(rhs_tag, Qt::CaseInsensitive) == 0)
		return true;

	qsizetype lhs_suffix_index = 0;
	qsizetype rhs_suffix_index = 0;
	const QVersionNumber lhs_version = QVersionNumber::fromString(lhs_tag, &lhs_suffix_index);
	const QVersionNumber rhs_version = QVersionNumber::fromString(rhs_tag, &rhs_suffix_index);
	const bool lhs_semver_like = !lhs_version.isNull() && lhs_suffix_index == lhs_tag.size();
	const bool rhs_semver_like = !rhs_version.isNull() && rhs_suffix_index == rhs_tag.size();
	if (lhs_semver_like && rhs_semver_like)
		return lhs_version == rhs_version;

	return false;
}

std::optional<ReleaseDetails> parse_latest_release_payload(const QByteArray &payload)
{
	const QJsonDocument doc = QJsonDocument::fromJson(payload);
	if (!doc.isObject())
		return std::nullopt;

	const QJsonObject release = doc.object();
	const QString latest_tag = release.value("tag_name").toString().trimmed();
	if (latest_tag.isEmpty())
		return std::nullopt;

	QString release_url = release.value("html_url").toString().trimmed();
	if (release_url.isEmpty())
		release_url = QString("https://github.com/przxmus/better-markers/releases/tag/%1").arg(latest_tag);

	return ReleaseDetails{latest_tag, release_url};
}

size_t curl_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	const size_t total = size * nmemb;
	if (!userdata || !ptr || total == 0)
		return total;

	auto *response = static_cast<std::string *>(userdata);
	response->append(ptr, total);
	return total;
}

bool fetch_latest_release_with_curl(QByteArray *payload, QString *error)
{
	if (!payload)
		return false;

	static std::once_flag curl_init_once;
	static CURLcode curl_init_result = CURLE_OK;
	std::call_once(curl_init_once,
		       []() { curl_init_result = static_cast<CURLcode>(curl_global_init(CURL_GLOBAL_DEFAULT)); });
	if (curl_init_result != CURLE_OK) {
		if (error)
			*error = QString("curl global init failed: %1").arg(curl_easy_strerror(curl_init_result));
		return false;
	}

	CURL *curl = curl_easy_init();
	if (!curl) {
		if (error)
			*error = "curl init failed";
		return false;
	}

	char error_buffer[CURL_ERROR_SIZE] = {};
	std::string response;
	struct curl_slist *headers = nullptr;
	headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
	const std::string user_agent = std::string("User-Agent: better-markers/") + PLUGIN_VERSION;
	headers = curl_slist_append(headers, user_agent.c_str());

	curl_easy_setopt(curl, CURLOPT_URL, LATEST_RELEASE_API_URL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

	const CURLcode code = curl_easy_perform(curl);
	long response_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (code != CURLE_OK) {
		if (error) {
			const char *curl_error = error_buffer[0] ? error_buffer : curl_easy_strerror(code);
			*error = QString::fromUtf8(curl_error);
		}
		return false;
	}

	if (response_code < 200 || response_code >= 300) {
		if (error)
			*error = QString("HTTP %1").arg(response_code);
		return false;
	}

	*payload = QByteArray(response.data(), static_cast<qsizetype>(response.size()));
	return true;
}

class BetterMarkersPlugin {
public:
	bool load()
	{
		const uint64_t load_begin_ns = os_gettime_ns();
		obs_log(LOG_INFO, "[better-markers] plugin load begin");

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
		m_controller =
			std::make_unique<bm::MarkerController>(&m_store, &m_tracker, main_window, m_store_base_dir);
		m_controller->set_shutting_down(false);
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
		const uint64_t after_controller_init_ns = os_gettime_ns();
		obs_log(LOG_INFO, "[better-markers] plugin load phase ready: stores/controller/hotkeys initialized (%llu ms)",
			static_cast<unsigned long long>((after_controller_init_ns - load_begin_ns) / 1000000ULL));

		obs_frontend_add_save_callback(&BetterMarkersPlugin::on_frontend_save, this);
		obs_frontend_add_event_callback(&BetterMarkersPlugin::on_frontend_event, this);

		m_settings_action = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(
			bm::bm_text("BetterMarkers.SettingsMenu").toUtf8().constData()));
		if (m_settings_action) {
			m_settings_action_connection = QObject::connect(m_settings_action, &QAction::triggered,
									[this]() { show_settings_dialog(); });
		}

		create_main_dock(main_window);
		refresh_runtime_bindings();
		m_tracker.sync_from_frontend_state();
		m_controller->retry_recovery_queue();
		check_for_updates_on_startup();
		const uint64_t after_startup_tasks_ns = os_gettime_ns();
		obs_log(LOG_INFO, "[better-markers] plugin load phase ready: startup tasks scheduled/completed (%llu ms)",
			static_cast<unsigned long long>((after_startup_tasks_ns - load_begin_ns) / 1000000ULL));

		obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
		const uint64_t load_end_ns = os_gettime_ns();
		obs_log(LOG_INFO, "[better-markers] plugin load complete (%llu ms)",
			static_cast<unsigned long long>((load_end_ns - load_begin_ns) / 1000000ULL));
		return true;
	}

	void unload()
	{
		begin_shutdown();
		obs_frontend_remove_save_callback(&BetterMarkersPlugin::on_frontend_save, this);
		obs_frontend_remove_event_callback(&BetterMarkersPlugin::on_frontend_event, this);

		if (m_settings_action_connection)
			QObject::disconnect(m_settings_action_connection);
		m_settings_action_connection = {};
		m_settings_action = nullptr;

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
			delete m_dock_widget;
		m_dock_widget = nullptr;
		if (m_update_check_reply) {
			if (m_update_check_connection)
				QObject::disconnect(m_update_check_connection);
			m_update_check_connection = {};
			m_update_check_reply->abort();
			m_update_check_reply = nullptr;
		}
		m_update_network.reset();

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
		if (event == OBS_FRONTEND_EVENT_EXIT || event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) {
			self->begin_shutdown();
			return;
		}

		if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGING ||
		    event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING) {
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
		if (m_is_shutting_down)
			return;

		if (!m_settings_dialog) {
			QMainWindow *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
			m_settings_dialog = new bm::SettingsDialog(&m_store, main_window);
			m_settings_dialog->set_persist_callback([this]() {
				persist_non_scene();
				refresh_runtime_bindings();
			});
			m_settings_dialog->set_update_availability(m_has_update_available, m_latest_release_url);
		}

		m_settings_dialog->refresh();
		m_settings_dialog->set_update_availability(m_has_update_available, m_latest_release_url);
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
		if (m_controller) {
			m_controller->set_active_templates(active_templates);
			m_controller->set_export_profile(m_store.export_profile());
		}
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

	void check_for_updates_on_startup()
	{
		if (!m_update_network)
			m_update_network = std::make_unique<QNetworkAccessManager>();

		QNetworkRequest request(QUrl(QString::fromUtf8(LATEST_RELEASE_API_URL)));
		request.setRawHeader("Accept", "application/vnd.github+json");
		request.setRawHeader("User-Agent", QByteArray("better-markers/") + PLUGIN_VERSION);
		m_update_check_reply = m_update_network->get(request);
		m_update_check_connection = QObject::connect(m_update_check_reply, &QNetworkReply::finished, [this]() {
			QNetworkReply *reply = m_update_check_reply;
			m_update_check_reply = nullptr;
			m_update_check_connection = {};
			if (!reply)
				return;
			if (m_is_shutting_down) {
				reply->deleteLater();
				return;
			}

			if (reply->error() != QNetworkReply::NoError) {
				obs_log(LOG_WARNING, "Better Markers update check failed: %s",
					reply->errorString().toUtf8().constData());
				reply->deleteLater();
				try_update_check_with_curl();
				return;
			}

			const QByteArray payload = reply->readAll();
			reply->deleteLater();
			apply_update_from_payload(payload);
		});
	}

	void try_update_check_with_curl()
	{
		if (m_is_shutting_down)
			return;

		QByteArray payload;
		QString error;
		if (!fetch_latest_release_with_curl(&payload, &error)) {
			obs_log(LOG_WARNING, "Better Markers update check fallback failed: %s",
				error.toUtf8().constData());
			return;
		}

		apply_update_from_payload(payload);
	}

	void apply_update_from_payload(const QByteArray &payload)
	{
		if (m_is_shutting_down)
			return;

		const std::optional<ReleaseDetails> release = parse_latest_release_payload(payload);
		if (!release.has_value())
			return;

		const QString latest_tag = release->tag;
		if (release_tags_match(latest_tag, QString::fromUtf8(PLUGIN_VERSION)))
			return;
		m_has_update_available = true;

		m_latest_release_url = release->url;
		if (m_settings_dialog)
			m_settings_dialog->set_update_availability(true, m_latest_release_url);

		if (release_tags_match(m_store.skipped_update_tag(), latest_tag))
			return;

		show_update_available_dialog(latest_tag, m_latest_release_url);
	}

	void show_update_available_dialog(const QString &latest_tag, const QString &release_url)
	{
		if (m_is_shutting_down)
			return;

		QWidget *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
		QDialog dialog(parent);
		dialog.setWindowTitle(bm::bm_text("BetterMarkers.Update.Title"));
		dialog.setModal(true);

		auto *layout = new QVBoxLayout(&dialog);
		auto *message = new QLabel(bm::bm_text("BetterMarkers.Update.Message")
						   .arg(QString::fromUtf8(PLUGIN_VERSION))
						   .arg(latest_tag),
					   &dialog);
		message->setWordWrap(true);
		layout->addWidget(message);

		auto *buttons_layout = new QHBoxLayout();
		auto *skip_button = new QPushButton(bm::bm_text("BetterMarkers.Update.SkipVersion"), &dialog);
		auto *ignore_button = new QPushButton(bm::bm_text("BetterMarkers.Update.Ignore"), &dialog);
		auto *install_button = new QPushButton(bm::bm_text("BetterMarkers.Update.Install"), &dialog);
		install_button->setDefault(true);
		buttons_layout->addWidget(skip_button);
		buttons_layout->addStretch(1);
		buttons_layout->addWidget(ignore_button);
		buttons_layout->addWidget(install_button);
		layout->addLayout(buttons_layout);

		QObject::connect(skip_button, &QPushButton::clicked, [&dialog]() { dialog.done(UPDATE_ACTION_SKIP); });
		QObject::connect(ignore_button, &QPushButton::clicked,
				 [&dialog]() { dialog.done(UPDATE_ACTION_IGNORE); });
		QObject::connect(install_button, &QPushButton::clicked,
				 [&dialog]() { dialog.done(UPDATE_ACTION_INSTALL); });

		const int action = dialog.exec();
		if (action == UPDATE_ACTION_SKIP) {
			m_store.set_skipped_update_tag(latest_tag);
			m_store.save_global();
			return;
		}

		if (action == UPDATE_ACTION_INSTALL)
			QDesktopServices::openUrl(QUrl(release_url));
	}

	void begin_shutdown()
	{
		if (m_is_shutting_down)
			return;
		m_is_shutting_down = true;
		if (m_controller)
			m_controller->set_shutting_down(true);
		if (m_settings_dialog)
			m_settings_dialog->hide();
	}

	QString m_store_base_dir;
	bm::ScopeStore m_store;
	bm::RecordingSessionTracker m_tracker;
	std::unique_ptr<bm::MarkerController> m_controller;
	std::unique_ptr<bm::HotkeyRegistry> m_hotkeys;
	std::unique_ptr<QNetworkAccessManager> m_update_network;
	QNetworkReply *m_update_check_reply = nullptr;
	bool m_has_update_available = false;
	QString m_latest_release_url;
	bool m_is_shutting_down = false;

	QAction *m_settings_action = nullptr;
	QMetaObject::Connection m_settings_action_connection;
	QMetaObject::Connection m_update_check_connection;
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
