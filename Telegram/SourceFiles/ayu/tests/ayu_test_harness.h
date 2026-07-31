// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include <cstdio>

namespace AyuTests {

inline auto PassedCount = 0;
inline auto FailedCount = 0;

inline void Check(bool value, const char *expression, const char *file, int line) {
	if (value) {
		++PassedCount;
		std::printf("  PASS  %s\n", expression);
	} else {
		++FailedCount;
		std::printf("  FAIL  %s\n        at %s:%d\n", expression, file, line);
	}
}

inline void Run(const char *name, void (*body)()) {
	std::printf("\n== %s ==\n", name);
	body();
}

void RunEntitiesTests();
void RunTelegramHelpersTests();

} // namespace AyuTests

#define CHECK(...) ::AyuTests::Check((__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)
