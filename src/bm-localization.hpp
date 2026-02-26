#pragma once

#include <obs-module.h>

#include <QString>

namespace bm {

inline QString bm_text(const char *key)
{
	const char *localized = obs_module_text(key);
	if (!localized || !*localized)
		return QString::fromUtf8(key);
	return QString::fromUtf8(localized);
}

} // namespace bm
