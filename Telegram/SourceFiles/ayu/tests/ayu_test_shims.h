// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <QJsonObject>
#include <QString>
#include <string>
#include <vector>

template <typename T>
using Fn = std::function<T>;

using int64 = std::int64_t;
using uint64 = std::uint64_t;

class QFile final {
public:
	[[nodiscard]] static bool exists(const QString &path);
	static bool rename(const QString &from, const QString &to);

};

namespace tr {

struct now_t {
};

inline constexpr auto now = now_t();

[[nodiscard]] QString ayu_OneViewTTL(now_t);
[[nodiscard]] QString ayu_OnePlayTTL(now_t);

} // namespace tr

namespace AyuTestShim {

void WriteLog(const QString &message);

} // namespace AyuTestShim

#define LOG(message) ::AyuTestShim::WriteLog(QString message)

inline bool QFile::exists(const QString &path) {
	auto error = std::error_code();
	return std::filesystem::exists(path.toStdString(), error);
}

inline bool QFile::rename(const QString &from, const QString &to) {
	auto error = std::error_code();
	std::filesystem::rename(from.toStdString(), to.toStdString(), error);
	return !error;
}

inline QString tr::ayu_OneViewTTL(now_t) {
	return QString("<ayu_OneViewTTL>");
}

inline QString tr::ayu_OnePlayTTL(now_t) {
	return QString("<ayu_OnePlayTTL>");
}

inline void AyuTestShim::WriteLog(const QString &message) {
	if (std::getenv("AYU_TEST_LOG")) {
		std::fprintf(stderr, "[db] %s\n", message.toStdString().c_str());
	}
}
