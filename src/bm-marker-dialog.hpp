#pragma once

#include "bm-models.hpp"

#include <QDialog>
#include <QList>

class QComboBox;
class QEvent;
class QLineEdit;
class QObject;
class QPlainTextEdit;
class QShowEvent;
class QWidget;

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
	void prepare_for_immediate_input(bool aggressive_focus);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void showEvent(QShowEvent *event) override;

private:
	const MarkerTemplate *selected_template() const;
	void apply_template_to_fields(const MarkerTemplate *templ);
	void focus_primary_input();
	bool focus_next_or_accept(QWidget *current_widget);
	QList<QWidget *> active_focus_order() const;

	QVector<MarkerTemplate> m_templates;
	Mode m_mode;
	QString m_fixed_template_id;
	bool m_request_immediate_focus = false;
	bool m_aggressive_focus = false;

	QComboBox *m_template_combo = nullptr;
	QLineEdit *m_title_edit = nullptr;
	QComboBox *m_color_combo = nullptr;
	QPlainTextEdit *m_description_edit = nullptr;
};

} // namespace bm
