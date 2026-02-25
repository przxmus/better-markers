#pragma once

#include "bm-models.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLineEdit;
class QPlainTextEdit;

namespace bm {

class TemplateEditorDialog : public QDialog {
public:
	explicit TemplateEditorDialog(QWidget *parent = nullptr);

	void set_template(const MarkerTemplate &templ);
	MarkerTemplate result_template() const;

private:
	void validate();

	QComboBox *m_scope_combo = nullptr;
	QLineEdit *m_name_edit = nullptr;
	QLineEdit *m_title_edit = nullptr;
	QComboBox *m_color_combo = nullptr;
	QPlainTextEdit *m_description_edit = nullptr;
	QCheckBox *m_editable_title = nullptr;
	QCheckBox *m_editable_description = nullptr;
	QCheckBox *m_editable_color = nullptr;
	QDialogButtonBox *m_buttons = nullptr;
	QString m_template_id;
};

} // namespace bm
