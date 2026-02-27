#pragma once

#include <QString>
#include <Qt>

#include <cstdint>

namespace bm {

enum class SyntheticKeypressStatus : uint8_t {
	Success = 0,
	Empty,
	InvalidSequence,
	UnsupportedPlatform,
	PermissionDenied,
	UnsupportedKey,
	SystemFailure,
};

struct SyntheticKeypressResult {
	SyntheticKeypressStatus status = SyntheticKeypressStatus::Success;
	QString technical_reason;

	bool success() const
	{
		return status == SyntheticKeypressStatus::Success || status == SyntheticKeypressStatus::Empty;
	}

	bool should_warn() const { return !success(); }
};

SyntheticKeypressResult send_synthetic_keypress_portable(const QString &portable);

namespace detail {

SyntheticKeypressResult send_platform_synthetic_keypress(Qt::Key key, Qt::KeyboardModifiers modifiers);

} // namespace detail

} // namespace bm
