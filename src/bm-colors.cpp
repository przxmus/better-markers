#include "bm-colors.hpp"

namespace bm {

const QVector<PremiereColorOption> &premiere_colors()
{
	static const QVector<PremiereColorOption> colors = {
		{0, "Green"},
		{1, "Red"},
		{2, "Orange"},
		{3, "Yellow"},
		{4, "White"},
		{5, "Blue"},
		{6, "Cyan"},
		{7, "Lavender"},
		{8, "Magenta"},
	};
	return colors;
}

QString color_label_for_id(int color_id)
{
	for (const PremiereColorOption &opt : premiere_colors()) {
		if (opt.id == color_id)
			return opt.label;
	}
	return "Green";
}

} // namespace bm
