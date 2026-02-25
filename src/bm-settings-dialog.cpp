#include "bm-settings-dialog.hpp"

#include "bm-colors.hpp"
#include "bm-template-editor-dialog.hpp"

#include <QUuid>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace bm {
namespace {

QString scope_label(TemplateScope scope)
{
	switch (scope) {
	case TemplateScope::Global:
		return "Global";
	case TemplateScope::Profile:
		return "Profile";
	case TemplateScope::SceneCollection:
	default:
		return "Scene Collection";
	}
}

} // namespace

SettingsDialog::SettingsDialog(ScopeStore *store, QWidget *parent) : QDialog(parent), m_store(store)
{
	setWindowTitle("Better Markers Settings");
	setMinimumSize(760, 520);

	auto *main_layout = new QVBoxLayout(this);
	main_layout->addWidget(new QLabel("Marker Templates", this));

	m_template_list = new QListWidget(this);
	main_layout->addWidget(m_template_list, 1);

	auto *button_row = new QHBoxLayout();
	auto *add_btn = new QPushButton("Add", this);
	m_edit_button = new QPushButton("Edit", this);
	m_delete_button = new QPushButton("Delete", this);
	auto *close_btn = new QPushButton("Close", this);
	button_row->addWidget(add_btn);
	button_row->addWidget(m_edit_button);
	button_row->addWidget(m_delete_button);
	button_row->addStretch(1);
	button_row->addWidget(close_btn);
	main_layout->addLayout(button_row);

	main_layout->addWidget(new QLabel(
		"You can assign hotkeys for each template and quick actions in OBS Settings -> Hotkeys.", this));

	connect(add_btn, &QPushButton::clicked, this, [this]() { add_template(); });
	connect(m_edit_button, &QPushButton::clicked, this, [this]() { edit_template(); });
	connect(m_delete_button, &QPushButton::clicked, this, [this]() { delete_template(); });
	connect(close_btn, &QPushButton::clicked, this, &QDialog::hide);
	connect(m_template_list, &QListWidget::itemSelectionChanged, this, [this]() { on_selection_changed(); });

	refresh();
	on_selection_changed();
}

void SettingsDialog::set_persist_callback(PersistCallback callback)
{
	m_persist_callback = std::move(callback);
}

void SettingsDialog::refresh()
{
	m_template_list->clear();

	for (TemplateScope scope : {TemplateScope::SceneCollection, TemplateScope::Profile, TemplateScope::Global}) {
		const ScopedStoreData &data = m_store->for_scope(scope);
		for (int i = 0; i < data.templates.size(); ++i) {
			const MarkerTemplate &templ = data.templates.at(i);
			QString text = QString("[%1] %2")
					       .arg(scope_label(scope), templ.name);
			text += QString(" - %1 (%2)").arg(templ.title, color_label_for_id(templ.color_id));

			auto *item = new QListWidgetItem(text, m_template_list);
			item->setData(Qt::UserRole, static_cast<int>(scope));
			item->setData(Qt::UserRole + 1, i);
		}
	}
}

void SettingsDialog::add_template()
{
	TemplateEditorDialog editor(this);
	MarkerTemplate templ;
	templ.scope = TemplateScope::SceneCollection;
	templ.color_id = 0;
	editor.set_template(templ);

	if (editor.exec() != QDialog::Accepted)
		return;

	MarkerTemplate created = editor.result_template();
	created.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
	m_store->for_scope(created.scope).templates.push_back(created);

	if (m_persist_callback)
		m_persist_callback();
	refresh();
}

void SettingsDialog::edit_template()
{
	const SelectedTemplate selected = selected_template();
	if (selected.index < 0)
		return;

	ScopedStoreData &scope_store = m_store->for_scope(selected.scope);
	if (selected.index >= scope_store.templates.size())
		return;

	TemplateEditorDialog editor(this);
	editor.set_template(scope_store.templates.at(selected.index));

	if (editor.exec() != QDialog::Accepted)
		return;

	MarkerTemplate edited = editor.result_template();
	if (edited.id.isEmpty())
		edited.id = scope_store.templates.at(selected.index).id;

	if (edited.scope == selected.scope) {
		scope_store.templates[selected.index] = edited;
	} else {
		scope_store.templates.removeAt(selected.index);
		m_store->for_scope(edited.scope).templates.push_back(edited);
	}

	if (m_persist_callback)
		m_persist_callback();
	refresh();
}

void SettingsDialog::delete_template()
{
	const SelectedTemplate selected = selected_template();
	if (selected.index < 0)
		return;

	ScopedStoreData &scope_store = m_store->for_scope(selected.scope);
	if (selected.index >= scope_store.templates.size())
		return;

	const MarkerTemplate templ = scope_store.templates.at(selected.index);
	const auto answer = QMessageBox::question(
		this, "Delete Template", QString("Delete template '%1'?" ).arg(templ.name));
	if (answer != QMessageBox::Yes)
		return;

	scope_store.templates.removeAt(selected.index);
	if (m_persist_callback)
		m_persist_callback();
	refresh();
}

void SettingsDialog::on_selection_changed()
{
	const bool has_selection = selected_template().index >= 0;
	if (m_edit_button)
		m_edit_button->setEnabled(has_selection);
	if (m_delete_button)
		m_delete_button->setEnabled(has_selection);
}

SettingsDialog::SelectedTemplate SettingsDialog::selected_template() const
{
	SelectedTemplate selected;
	const QList<QListWidgetItem *> items = m_template_list->selectedItems();
	if (items.isEmpty())
		return selected;

	QListWidgetItem *item = items.first();
	selected.scope = static_cast<TemplateScope>(item->data(Qt::UserRole).toInt());
	selected.index = item->data(Qt::UserRole + 1).isValid() ? item->data(Qt::UserRole + 1).toInt() : -1;
	return selected;
}

} // namespace bm
