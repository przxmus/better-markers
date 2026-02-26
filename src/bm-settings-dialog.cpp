#include "bm-settings-dialog.hpp"

#include "bm-colors.hpp"
#include "bm-localization.hpp"
#include "bm-template-editor-dialog.hpp"

#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/base.h>

#include <QUuid>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>

namespace bm {
namespace {

QString scope_label(TemplateScope scope)
{
	switch (scope) {
	case TemplateScope::Global:
		return bm::bm_text("BetterMarkers.Scope.Global");
	case TemplateScope::Profile:
		return bm::bm_text("BetterMarkers.Scope.Profile");
	case TemplateScope::SceneCollection:
	default:
		return bm::bm_text("BetterMarkers.Scope.SceneCollection");
	}
}

} // namespace

SettingsDialog::SettingsDialog(ScopeStore *store, QWidget *parent) : QDialog(parent), m_store(store)
{
	setWindowTitle(bm_text("BetterMarkers.Settings.Title"));
	setMinimumSize(760, 520);

	auto *main_layout = new QVBoxLayout(this);
	auto *header_row = new QHBoxLayout();
	header_row->addStretch(1);
	auto *version_layout = new QVBoxLayout();
	version_layout->setSpacing(2);
	m_version_label = new QLabel(bm_text("BetterMarkers.Settings.Version").arg(QString::fromUtf8(PLUGIN_VERSION)), this);
	m_version_label->setAlignment(Qt::AlignRight | Qt::AlignTop);
	version_layout->addWidget(m_version_label);
	m_update_available_label = new QLabel(this);
	m_update_available_label->setAlignment(Qt::AlignRight | Qt::AlignTop);
	m_update_available_label->setTextFormat(Qt::RichText);
	m_update_available_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	m_update_available_label->setOpenExternalLinks(false);
	m_update_available_label->hide();
	version_layout->addWidget(m_update_available_label);
	header_row->addLayout(version_layout);
	main_layout->addLayout(header_row);

	main_layout->addWidget(new QLabel(bm_text("BetterMarkers.Settings.MarkerTemplates"), this));

	m_template_list = new QListWidget(this);
	main_layout->addWidget(m_template_list, 1);

	auto *button_row = new QHBoxLayout();
	auto *add_btn = new QPushButton(bm_text("BetterMarkers.Common.AddTemplate"), this);
	m_edit_button = new QPushButton(bm_text("BetterMarkers.Common.Edit"), this);
	m_delete_button = new QPushButton(bm_text("BetterMarkers.Common.Delete"), this);
	auto *close_btn = new QPushButton(bm_text("BetterMarkers.Common.Close"), this);
	button_row->addWidget(add_btn);
	button_row->addWidget(m_edit_button);
	button_row->addWidget(m_delete_button);
	button_row->addStretch(1);
	button_row->addWidget(close_btn);
	main_layout->addLayout(button_row);

	auto *export_targets_group = new QGroupBox(bm_text("BetterMarkers.Settings.ExportTargets"), this);
	auto *export_targets_layout = new QHBoxLayout(export_targets_group);
	export_targets_layout->setContentsMargins(10, 8, 10, 8);
	export_targets_layout->setSpacing(12);

	m_premiere_toggle = new QCheckBox(bm_text("BetterMarkers.Settings.ExportPremiereLabel"), export_targets_group);
	m_premiere_toggle->setToolTip(bm_text("BetterMarkers.Settings.ExportPremiereHint"));
	export_targets_layout->addWidget(m_premiere_toggle);
	export_targets_layout->addSpacing(18);

	m_resolve_toggle = new QCheckBox(bm_text("BetterMarkers.Settings.ExportResolveLabel"), export_targets_group);
	m_resolve_toggle->setToolTip(bm_text("BetterMarkers.Settings.ExportResolveHint"));
	export_targets_layout->addWidget(m_resolve_toggle);
	export_targets_layout->addSpacing(18);

	m_final_cut_toggle = new QCheckBox(bm_text("BetterMarkers.Settings.ExportFinalCutLabel"), export_targets_group);
#ifdef __APPLE__
	m_final_cut_toggle->setToolTip(bm_text("BetterMarkers.Settings.ExportFinalCutHint"));
#else
	m_final_cut_toggle->setEnabled(false);
	m_final_cut_toggle->setToolTip(bm_text("BetterMarkers.Settings.ExportFinalCutUnavailable"));
#endif
	export_targets_layout->addWidget(m_final_cut_toggle);
	export_targets_layout->addStretch(1);

	main_layout->addWidget(export_targets_group);

	main_layout->addWidget(new QLabel(bm_text("BetterMarkers.Settings.HotkeysHint"), this));

	connect(add_btn, &QPushButton::clicked, this, [this]() { add_template(); });
	connect(m_edit_button, &QPushButton::clicked, this, [this]() { edit_template(); });
	connect(m_delete_button, &QPushButton::clicked, this, [this]() { delete_template(); });
	connect(close_btn, &QPushButton::clicked, this, &QDialog::hide);
	connect(m_template_list, &QListWidget::itemSelectionChanged, this, [this]() { on_selection_changed(); });
	connect(m_premiere_toggle, &QCheckBox::checkStateChanged, this, [this]() { update_export_profile_from_ui(); });
	connect(m_resolve_toggle, &QCheckBox::checkStateChanged, this, [this]() { update_export_profile_from_ui(); });
	connect(m_final_cut_toggle, &QCheckBox::checkStateChanged, this, [this]() { update_export_profile_from_ui(); });
	connect(m_update_available_label, &QLabel::linkActivated, this, [this](const QString &) {
		if (!m_release_url.isEmpty())
			QDesktopServices::openUrl(QUrl(m_release_url));
	});

	refresh();
	on_selection_changed();
}

void SettingsDialog::set_persist_callback(PersistCallback callback)
{
	m_persist_callback = std::move(callback);
}

void SettingsDialog::set_update_availability(bool update_available, const QString &release_url)
{
	m_release_url = release_url.trimmed();
	const bool show_link = update_available && !m_release_url.isEmpty();
	if (m_update_available_label) {
		if (show_link) {
			const QString anchor = QString("<a href=\"%1\">%2</a>")
					       .arg(m_release_url.toHtmlEscaped(), bm_text("BetterMarkers.Settings.UpdateAvailable"));
			m_update_available_label->setText(anchor);
			m_update_available_label->show();
		} else {
			m_update_available_label->clear();
			m_update_available_label->hide();
		}
	}
}

void SettingsDialog::refresh()
{
	const ExportProfile profile = m_store->export_profile();
	{
		QSignalBlocker block_premiere(m_premiere_toggle);
		m_premiere_toggle->setChecked(profile.enable_premiere_xmp);
	}
	{
		QSignalBlocker block_resolve(m_resolve_toggle);
		m_resolve_toggle->setChecked(profile.enable_resolve_fcpxml);
	}
	{
		QSignalBlocker block_final_cut(m_final_cut_toggle);
		m_final_cut_toggle->setChecked(profile.enable_final_cut_fcpxml);
	}

	m_template_list->clear();
	const ScopedStoreData &global_store = m_store->for_scope(TemplateScope::Global);
	for (int i = 0; i < global_store.templates.size(); ++i) {
		const MarkerTemplate &templ = global_store.templates.at(i);
		QString text = QString("[%1").arg(scope_label(templ.scope));
		if (!templ.scope_target.trimmed().isEmpty())
			text += QString(": %1").arg(templ.scope_target);
		text += QString("] %1").arg(templ.name);
		text += QString(" - %1 (%2)").arg(templ.title, color_label_for_id(templ.color_id));

		auto *item = new QListWidgetItem(text, m_template_list);
		item->setData(Qt::UserRole, i);
	}
}

void SettingsDialog::update_export_profile_from_ui()
{
	ExportProfile &profile = m_store->export_profile();
	profile.enable_premiere_xmp = m_premiere_toggle && m_premiere_toggle->isChecked();
	profile.enable_resolve_fcpxml = m_resolve_toggle && m_resolve_toggle->isChecked();
#ifdef __APPLE__
	profile.enable_final_cut_fcpxml = m_final_cut_toggle && m_final_cut_toggle->isChecked();
#else
	profile.enable_final_cut_fcpxml = false;
#endif

	if (m_persist_callback)
		m_persist_callback();
}

void SettingsDialog::add_template()
{
	TemplateEditorDialog editor(available_profiles(), available_scene_collections(), m_store->current_profile_name(),
				    m_store->current_scene_collection_name(), this);
	MarkerTemplate templ;
	templ.scope = TemplateScope::SceneCollection;
	templ.color_id = 0;
	templ.scope_target = m_store->current_scene_collection_name();
	editor.set_template(templ);

	if (editor.exec() != QDialog::Accepted)
		return;

	MarkerTemplate created = editor.result_template();
	created.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
	m_store->for_scope(TemplateScope::Global).templates.push_back(created);

	if (m_persist_callback)
		m_persist_callback();
	refresh();
}

void SettingsDialog::edit_template()
{
	const SelectedTemplate selected = selected_template();
	if (selected.index < 0)
		return;

	ScopedStoreData &scope_store = m_store->for_scope(TemplateScope::Global);
	if (selected.index >= scope_store.templates.size())
		return;

	TemplateEditorDialog editor(available_profiles(), available_scene_collections(), m_store->current_profile_name(),
				    m_store->current_scene_collection_name(), this);
	editor.set_template(scope_store.templates.at(selected.index));

	if (editor.exec() != QDialog::Accepted)
		return;

	MarkerTemplate edited = editor.result_template();
	if (edited.id.isEmpty())
		edited.id = scope_store.templates.at(selected.index).id;
	scope_store.templates[selected.index] = edited;

	if (m_persist_callback)
		m_persist_callback();
	refresh();
}

void SettingsDialog::delete_template()
{
	const SelectedTemplate selected = selected_template();
	if (selected.index < 0)
		return;

	ScopedStoreData &scope_store = m_store->for_scope(TemplateScope::Global);
	if (selected.index >= scope_store.templates.size())
		return;

	const MarkerTemplate templ = scope_store.templates.at(selected.index);
	const auto answer = QMessageBox::question(
		this, bm_text("BetterMarkers.Settings.DeleteTitle"),
		bm_text("BetterMarkers.Settings.DeleteMessage").arg(templ.name));
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
	selected.index = item->data(Qt::UserRole).isValid() ? item->data(Qt::UserRole).toInt() : -1;
	return selected;
}

QStringList SettingsDialog::available_profiles() const
{
	QStringList profiles;
	char **names = obs_frontend_get_profiles();
	if (names) {
		for (size_t i = 0; names[i] != nullptr; ++i)
			profiles.push_back(QString::fromUtf8(names[i]));
		bfree(names);
	}
	if (!profiles.contains(m_store->current_profile_name()))
		profiles.push_back(m_store->current_profile_name());
	return profiles;
}

QStringList SettingsDialog::available_scene_collections() const
{
	QStringList collections;
	char **names = obs_frontend_get_scene_collections();
	if (names) {
		for (size_t i = 0; names[i] != nullptr; ++i)
			collections.push_back(QString::fromUtf8(names[i]));
		bfree(names);
	}
	if (!collections.contains(m_store->current_scene_collection_name()))
		collections.push_back(m_store->current_scene_collection_name());
	return collections;
}

} // namespace bm
