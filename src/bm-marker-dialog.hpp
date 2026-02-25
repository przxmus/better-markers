#pragma once

#include "bm-models.hpp"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;

namespace bm {

class MarkerDialog : public QDialog {
public:
	enum class Mode {
		ChooseTemplate,
		FixedTemplate,
		NoTemplate,
	};

	MarkerDialog(const QVector<MarkerTemplate> &templates, Mode mode, const QString &fixed_template_id,
		     QWidget *parent = nullptr);

	bool uses_template() const;
	QString selected_template_id() const;
	QString marker_title() const;
	QString marker_description() const;
	int marker_color_id() const;

private:
	const MarkerTemplate *selected_template() const;
	void apply_template_to_fields(const MarkerTemplate *templ);

	QVector<MarkerTemplate> m_templates;
	Mode m_mode;
	QString m_fixed_template_id;

	QComboBox *m_template_combo = nullptr;
	QLineEdit *m_title_edit = nullptr;
	QComboBox *m_color_combo = nullptr;
	QPlainTextEdit *m_description_edit = nullptr;
};

} // namespace bm
