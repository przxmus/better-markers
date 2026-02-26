#pragma once

#include "bm-scope-store.hpp"

#include <QDialog>
#include <QStringList>
#include <functional>

class QListWidget;
class QCheckBox;
class QPushButton;
class QLabel;

namespace bm {

class SettingsDialog : public QDialog {
public:
	using PersistCallback = std::function<void()>;

	explicit SettingsDialog(ScopeStore *store, QWidget *parent = nullptr);

	void set_persist_callback(PersistCallback callback);
	void set_update_availability(bool update_available, const QString &release_url);
	void refresh();

private:
	void add_template();
	void edit_template();
	void delete_template();
	void on_selection_changed();
	void update_export_profile_from_ui();
	QStringList available_profiles() const;
	QStringList available_scene_collections() const;

	struct SelectedTemplate {
		int index = -1;
	};

	SelectedTemplate selected_template() const;

	ScopeStore *m_store = nullptr;
	PersistCallback m_persist_callback;
	QListWidget *m_template_list = nullptr;
	QCheckBox *m_premiere_toggle = nullptr;
	QCheckBox *m_resolve_toggle = nullptr;
	QCheckBox *m_final_cut_toggle = nullptr;
	QPushButton *m_edit_button = nullptr;
	QPushButton *m_delete_button = nullptr;
	QLabel *m_version_label = nullptr;
	QLabel *m_update_available_label = nullptr;
	QString m_release_url;
};

} // namespace bm
