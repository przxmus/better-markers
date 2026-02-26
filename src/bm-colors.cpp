#include "bm-colors.hpp"
#include "bm-localization.hpp"

namespace bm {

const QVector<PremiereColorOption> &premiere_colors()
{
	static const QVector<PremiereColorOption> colors = {
		{0, bm_text("BetterMarkers.Color.Green")},   {1, bm_text("BetterMarkers.Color.Red")},
		{2, bm_text("BetterMarkers.Color.Orange")},  {3, bm_text("BetterMarkers.Color.Yellow")},
		{4, bm_text("BetterMarkers.Color.White")},   {5, bm_text("BetterMarkers.Color.Blue")},
		{6, bm_text("BetterMarkers.Color.Cyan")},    {7, bm_text("BetterMarkers.Color.Lavender")},
		{8, bm_text("BetterMarkers.Color.Magenta")},
	};
	return colors;
}

QString color_label_for_id(int color_id)
{
	for (const PremiereColorOption &opt : premiere_colors()) {
		if (opt.id == color_id)
			return opt.label;
	}
	return bm_text("BetterMarkers.Color.Green");
}

} // namespace bm
