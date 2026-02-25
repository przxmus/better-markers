#pragma once

#include <QString>
#include <QVector>

namespace bm {

struct PremiereColorOption {
	int id;
	QString label;
};

const QVector<PremiereColorOption> &premiere_colors();
QString color_label_for_id(int color_id);

} // namespace bm
