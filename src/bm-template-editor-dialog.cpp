#include "bm-template-editor-dialog.hpp"

#include "bm-colors.hpp"

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

TemplateEditorDialog::TemplateEditorDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Template Editor");
	setModal(true);

	m_scope_combo = new QComboBox(this);
	m_scope_combo->addItem("Scene Collection", static_cast<int>(TemplateScope::SceneCollection));
	m_scope_combo->addItem("Profile", static_cast<int>(TemplateScope::Profile));
	m_scope_combo->addItem("Global", static_cast<int>(TemplateScope::Global));

	m_name_edit = new QLineEdit(this);
	m_name_edit->setPlaceholderText("Template name");
	m_title_edit = new QLineEdit(this);
	m_color_combo = new QComboBox(this);
	for (const PremiereColorOption &color : premiere_colors())
		m_color_combo->addItem(color.label, color.id);

	m_description_edit = new QPlainTextEdit(this);
	m_description_edit->setPlaceholderText("Description");
	m_description_edit->setFixedHeight(100);

	m_editable_title = new QCheckBox("Title can be edited while adding marker", this);
	m_editable_description = new QCheckBox("Description can be edited while adding marker", this);
	m_editable_color = new QCheckBox("Color can be edited while adding marker", this);

	auto *main_layout = new QVBoxLayout(this);
	auto *form_layout = new QFormLayout();
	form_layout->addRow("Scope", m_scope_combo);
	form_layout->addRow("Template Name", m_name_edit);

	auto *title_row = new QWidget(this);
	auto *title_layout = new QHBoxLayout(title_row);
	title_layout->setContentsMargins(0, 0, 0, 0);
	title_layout->addWidget(m_title_edit, 2);
	title_layout->addWidget(new QLabel("Color", this));
	title_layout->addWidget(m_color_combo, 1);
	form_layout->addRow("Marker Title", title_row);

	form_layout->addRow("Description", m_description_edit);
	main_layout->addLayout(form_layout);
	main_layout->addWidget(m_editable_title);
	main_layout->addWidget(m_editable_description);
	main_layout->addWidget(m_editable_color);

	m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
	main_layout->addWidget(m_buttons);

	connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_name_edit, &QLineEdit::textChanged, this, [this]() { validate(); });

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
	validate();
}

MarkerTemplate TemplateEditorDialog::result_template() const
{
	MarkerTemplate templ;
	templ.id = m_template_id;
	templ.scope = static_cast<TemplateScope>(m_scope_combo->currentData().toInt());
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

} // namespace bm
