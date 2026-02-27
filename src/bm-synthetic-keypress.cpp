#include "bm-synthetic-keypress.hpp"

#include <QKeySequence>

namespace bm {

SyntheticKeypressResult send_synthetic_keypress_portable(const QString &portable)
{
	const QString trimmed = portable.trimmed();
	if (trimmed.isEmpty())
		return {SyntheticKeypressStatus::Empty, "empty key sequence"};

	const QKeySequence sequence = QKeySequence::fromString(trimmed, QKeySequence::PortableText);
	if (sequence.isEmpty())
		return {SyntheticKeypressStatus::InvalidSequence, "sequence parsing failed"};
	if (sequence.count() != 1)
		return {SyntheticKeypressStatus::InvalidSequence, "sequence must contain exactly one key combination"};

	const QKeyCombination combination = sequence[0];
	const Qt::Key key = combination.key();
	if (key == Qt::Key_unknown)
		return {SyntheticKeypressStatus::InvalidSequence, "unknown key"};

	return detail::send_platform_synthetic_keypress(key, combination.keyboardModifiers());
}

} // namespace bm
