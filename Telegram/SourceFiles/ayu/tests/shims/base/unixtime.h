// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include <ctime>

namespace base::unixtime {

[[nodiscard]] inline int now() {
	return static_cast<int>(std::time(nullptr));
}

} // namespace base::unixtime
