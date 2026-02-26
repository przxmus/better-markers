#pragma once

#include "bm-scope-store.hpp"

#include <QDialog>
#include <QStringList>
#include <functional>

class QListWidget;
class QPushButton;

namespace bm {

class SettingsDialog : public QDialog {
public:
	using PersistCallback = std::function<void()>;

	explicit SettingsDialog(ScopeStore *store, QWidget *parent = nullptr);

	void set_persist_callback(PersistCallback callback);
	void refresh();

private:
	void add_template();
	void edit_template();
	void delete_template();
	void on_selection_changed();
	QStringList available_profiles() const;
	QStringList available_scene_collections() const;

	struct SelectedTemplate {
		int index = -1;
	};

	SelectedTemplate selected_template() const;

	ScopeStore *m_store = nullptr;
	PersistCallback m_persist_callback;
	QListWidget *m_template_list = nullptr;
	QPushButton *m_edit_button = nullptr;
	QPushButton *m_delete_button = nullptr;
};

} // namespace bm
