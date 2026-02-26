#include "bm-marker-dialog.hpp"

#include "bm-colors.hpp"
#include "bm-localization.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
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

MarkerDialog::MarkerDialog(const QVector<MarkerTemplate> &templates, Mode mode, const QString &fixed_template_id,
			   QWidget *parent)
	: QDialog(parent), m_templates(templates), m_mode(mode), m_fixed_template_id(fixed_template_id)
{
	setModal(true);
	setWindowTitle(bm_text("BetterMarkers.AddMarkerButton"));
	setMinimumWidth(560);

	auto *layout = new QVBoxLayout(this);
	auto *form = new QFormLayout();

	if (m_mode == Mode::ChooseTemplate) {
		m_template_combo = new QComboBox(this);
		m_template_combo->addItem(bm_text("BetterMarkers.MarkerDialog.NoTemplate"), QString());
		for (const MarkerTemplate &templ : m_templates) {
			QString scope = scope_label(templ.scope);
			if (!templ.scope_target.trimmed().isEmpty())
				scope += QString(": %1").arg(templ.scope_target);
			const QString label = QString("[%1] %2").arg(scope, templ.name);
			m_template_combo->addItem(label, templ.id);
		}
		form->addRow(bm_text("BetterMarkers.MarkerDialog.Template"), m_template_combo);
	}

	auto *title_row = new QWidget(this);
	auto *title_layout = new QHBoxLayout(title_row);
	title_layout->setContentsMargins(0, 0, 0, 0);
	m_title_edit = new QLineEdit(this);
	m_color_combo = new QComboBox(this);
	for (const PremiereColorOption &color : premiere_colors())
		m_color_combo->addItem(color.label, color.id);
	title_layout->addWidget(m_title_edit, 3);
	title_layout->addWidget(new QLabel(bm_text("BetterMarkers.Common.Color"), this));
	title_layout->addWidget(m_color_combo, 2);
	form->addRow(bm_text("BetterMarkers.MarkerDialog.MarkerTitle"), title_row);

	m_description_edit = new QPlainTextEdit(this);
	m_description_edit->setFixedHeight(110);
	form->addRow(bm_text("BetterMarkers.MarkerDialog.Description"), m_description_edit);
	layout->addLayout(form);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	layout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	if (m_template_combo) {
		connect(m_template_combo, &QComboBox::currentIndexChanged, this,
			[this](int) { apply_template_to_fields(selected_template()); });
		m_template_combo->setCurrentIndex(0);
	}

	if (m_mode == Mode::FixedTemplate)
		apply_template_to_fields(selected_template());
	else if (m_mode == Mode::NoTemplate)
		apply_template_to_fields(nullptr);
}

bool MarkerDialog::uses_template() const
{
	return selected_template() != nullptr;
}

QString MarkerDialog::selected_template_id() const
{
	const MarkerTemplate *templ = selected_template();
	return templ ? templ->id : QString();
}

QString MarkerDialog::marker_title() const
{
	return m_title_edit->text();
}

QString MarkerDialog::marker_description() const
{
	return m_description_edit->toPlainText();
}

int MarkerDialog::marker_color_id() const
{
	return m_color_combo->currentData().toInt(0);
}

const MarkerTemplate *MarkerDialog::selected_template() const
{
	QString template_id;
	if (m_mode == Mode::ChooseTemplate) {
		if (!m_template_combo)
			return nullptr;
		template_id = m_template_combo->currentData().toString();
		if (template_id.isEmpty())
			return nullptr;
	} else if (m_mode == Mode::FixedTemplate) {
		template_id = m_fixed_template_id;
	} else {
		return nullptr;
	}

	for (const MarkerTemplate &templ : m_templates) {
		if (templ.id == template_id)
			return &templ;
	}
	return nullptr;
}

void MarkerDialog::apply_template_to_fields(const MarkerTemplate *templ)
{
	if (!templ) {
		m_title_edit->clear();
		m_description_edit->clear();
		m_color_combo->setCurrentIndex(m_color_combo->findData(0));
		m_title_edit->setEnabled(true);
		m_description_edit->setEnabled(true);
		m_color_combo->setEnabled(true);
		return;
	}

	m_title_edit->setText(templ->title);
	m_description_edit->setPlainText(templ->description);
	m_color_combo->setCurrentIndex(m_color_combo->findData(templ->color_id));

	m_title_edit->setEnabled(templ->editable_title);
	m_description_edit->setEnabled(templ->editable_description);
	m_color_combo->setEnabled(templ->editable_color);
}

} // namespace bm
