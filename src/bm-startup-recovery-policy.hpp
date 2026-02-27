#pragma once

#include "bm-marker-data.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

namespace bm {

enum class StartupRecoveryAction {
	RetryOnce,
	DropMissingMedia,
	DropUnsupportedMedia,
	DropMissingSidecar,
};

struct StartupRecoveryDecision {
	StartupRecoveryAction action = StartupRecoveryAction::RetryOnce;
	QString sidecar_path;
};

inline constexpr int startup_recovery_retry_attempts()
{
	return 1;
}

inline StartupRecoveryDecision decide_startup_recovery(const QString &media_path)
{
	if (!QFile::exists(media_path))
		return {StartupRecoveryAction::DropMissingMedia, {}};

	if (!is_mp4_or_mov_path(media_path))
		return {StartupRecoveryAction::DropUnsupportedMedia, {}};

	const QFileInfo info(media_path);
	const QString sidecar_path = info.dir().filePath(info.completeBaseName() + ".xmp");
	if (!QFile::exists(sidecar_path))
		return {StartupRecoveryAction::DropMissingSidecar, sidecar_path};

	return {StartupRecoveryAction::RetryOnce, sidecar_path};
}

inline const char *startup_recovery_action_name(StartupRecoveryAction action)
{
	switch (action) {
	case StartupRecoveryAction::RetryOnce:
		return "retry_once";
	case StartupRecoveryAction::DropMissingMedia:
		return "drop_missing_media";
	case StartupRecoveryAction::DropUnsupportedMedia:
		return "drop_unsupported_media";
	case StartupRecoveryAction::DropMissingSidecar:
		return "drop_missing_sidecar";
	default:
		return "unknown";
	}
}

} // namespace bm
