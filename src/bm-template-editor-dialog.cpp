#include "bm-template-editor-dialog.hpp"

#include "bm-colors.hpp"
#include "bm-localization.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace bm {

TemplateEditorDialog::TemplateEditorDialog(const QStringList &profiles, const QStringList &scene_collections,
					   const QString &current_profile, const QString &current_scene_collection,
					   QWidget *parent)
	: QDialog(parent),
	  m_profiles(profiles),
	  m_scene_collections(scene_collections),
	  m_current_profile(current_profile),
	  m_current_scene_collection(current_scene_collection)
{
	setWindowTitle(bm_text("BetterMarkers.TemplateEditor.Title"));
	setModal(true);

	m_scope_combo = new QComboBox(this);
	m_scope_combo->addItem(bm_text("BetterMarkers.Scope.SceneCollection"),
			       static_cast<int>(TemplateScope::SceneCollection));
	m_scope_combo->addItem(bm_text("BetterMarkers.Scope.Profile"), static_cast<int>(TemplateScope::Profile));
	m_scope_combo->addItem(bm_text("BetterMarkers.Scope.Global"), static_cast<int>(TemplateScope::Global));
	m_scope_target_combo = new QComboBox(this);

	m_name_edit = new QLineEdit(this);
	m_name_edit->setPlaceholderText(bm_text("BetterMarkers.TemplateEditor.TemplateNamePlaceholder"));
	m_title_edit = new QLineEdit(this);
	m_color_combo = new QComboBox(this);
	for (const PremiereColorOption &color : premiere_colors())
		m_color_combo->addItem(color.label, color.id);

	m_description_edit = new QPlainTextEdit(this);
	m_description_edit->setPlaceholderText(bm_text("BetterMarkers.TemplateEditor.DescriptionPlaceholder"));
	m_description_edit->setFixedHeight(100);

	m_editable_title = new QCheckBox(bm_text("BetterMarkers.TemplateEditor.EditableTitle"), this);
	m_editable_description = new QCheckBox(bm_text("BetterMarkers.TemplateEditor.EditableDescription"), this);
	m_editable_color = new QCheckBox(bm_text("BetterMarkers.TemplateEditor.EditableColor"), this);

	auto *main_layout = new QVBoxLayout(this);
	auto *form_layout = new QFormLayout();
	form_layout->addRow(bm_text("BetterMarkers.TemplateEditor.Scope"), m_scope_combo);
	form_layout->addRow(bm_text("BetterMarkers.TemplateEditor.ScopeTarget"), m_scope_target_combo);
	form_layout->addRow(bm_text("BetterMarkers.TemplateEditor.TemplateName"), m_name_edit);

	auto *title_row = new QWidget(this);
	auto *title_layout = new QHBoxLayout(title_row);
	title_layout->setContentsMargins(0, 0, 0, 0);
	title_layout->addWidget(m_title_edit, 2);
	title_layout->addWidget(new QLabel(bm_text("BetterMarkers.Common.Color"), this));
	title_layout->addWidget(m_color_combo, 1);
	form_layout->addRow(bm_text("BetterMarkers.TemplateEditor.MarkerTitle"), title_row);

	form_layout->addRow(bm_text("BetterMarkers.TemplateEditor.Description"), m_description_edit);
	main_layout->addLayout(form_layout);
	main_layout->addWidget(m_editable_title);
	main_layout->addWidget(m_editable_description);
	main_layout->addWidget(m_editable_color);

	m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
	main_layout->addWidget(m_buttons);

	connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_name_edit, &QLineEdit::textChanged, this, [this]() { validate(); });
	connect(m_scope_combo, &QComboBox::currentIndexChanged, this, [this](int index) {
		Q_UNUSED(index);
		refresh_scope_target_visibility();
	});

	refresh_scope_target_visibility();
	validate();
}

void TemplateEditorDialog::set_template(const MarkerTemplate &templ)
{
	m_template_id = templ.id;
	m_scope_combo->setCurrentIndex(m_scope_combo->findData(static_cast<int>(templ.scope)));
	m_name_edit->setText(templ.name);
	m_title_edit->setText(templ.title);
	m_color_combo->setCurrentIndex(m_color_combo->findData(templ.color_id));
	m_description_edit->setPlainText(templ.description);
	m_editable_title->setChecked(templ.editable_title);
	m_editable_description->setChecked(templ.editable_description);
	m_editable_color->setChecked(templ.editable_color);
	refresh_scope_target_visibility();

	const QString target = templ.scope_target.trimmed();
	if (!target.isEmpty()) {
		const int target_index = m_scope_target_combo->findData(target);
		if (target_index >= 0)
			m_scope_target_combo->setCurrentIndex(target_index);
	}
	validate();
}

MarkerTemplate TemplateEditorDialog::result_template() const
{
	MarkerTemplate templ;
	templ.id = m_template_id;
	templ.scope = static_cast<TemplateScope>(m_scope_combo->currentData().toInt());
	templ.scope_target = m_scope_target_combo->isVisible() ? m_scope_target_combo->currentData().toString()
							       : QString();
	templ.name = m_name_edit->text().trimmed();
	templ.title = m_title_edit->text();
	templ.color_id = m_color_combo->currentData().toInt(0);
	templ.description = m_description_edit->toPlainText();
	templ.editable_title = m_editable_title->isChecked();
	templ.editable_description = m_editable_description->isChecked();
	templ.editable_color = m_editable_color->isChecked();
	return templ;
}

void TemplateEditorDialog::validate()
{
	QPushButton *ok_button = m_buttons->button(QDialogButtonBox::Ok);
	if (ok_button)
		ok_button->setEnabled(!m_name_edit->text().trimmed().isEmpty());
}

void TemplateEditorDialog::refresh_scope_target_visibility()
{
	m_scope_target_combo->clear();

	const TemplateScope scope = static_cast<TemplateScope>(m_scope_combo->currentData().toInt());
	if (scope == TemplateScope::Profile) {
		for (const QString &profile : m_profiles)
			m_scope_target_combo->addItem(profile, profile);
		if (m_scope_target_combo->count() == 0 && !m_current_profile.isEmpty())
			m_scope_target_combo->addItem(m_current_profile, m_current_profile);
		const int current_index = m_scope_target_combo->findData(m_current_profile);
		if (current_index >= 0)
			m_scope_target_combo->setCurrentIndex(current_index);
		m_scope_target_combo->setVisible(true);
		return;
	}

	if (scope == TemplateScope::SceneCollection) {
		for (const QString &collection : m_scene_collections)
			m_scope_target_combo->addItem(collection, collection);
		if (m_scope_target_combo->count() == 0 && !m_current_scene_collection.isEmpty())
			m_scope_target_combo->addItem(m_current_scene_collection, m_current_scene_collection);
		const int current_index = m_scope_target_combo->findData(m_current_scene_collection);
		if (current_index >= 0)
			m_scope_target_combo->setCurrentIndex(current_index);
		m_scope_target_combo->setVisible(true);
		return;
	}

	m_scope_target_combo->setVisible(false);
}

} // namespace bm
