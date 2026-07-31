// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/data/ayu_database.h"

#include "ayu/tests/ayu_test_harness.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

namespace {

using Ids = std::vector<int>;
using Texts = std::vector<std::string>;
using AyuTests::Run;

class TempDatabaseDirectory final {
public:
	enum class Tdata {
		Present,
		Missing,
	};

	explicit TempDatabaseDirectory(Tdata tdata = Tdata::Present);
	~TempDatabaseDirectory();

private:
	std::filesystem::path _previous;
	std::filesystem::path _root;

};

TempDatabaseDirectory::TempDatabaseDirectory(Tdata tdata)
: _previous(std::filesystem::current_path()) {
	static auto counter = 0;
	const auto name = "ayu_database_tests_"
		+ std::to_string(::getpid())
		+ "_"
		+ std::to_string(++counter);
	_root = std::filesystem::temp_directory_path() / name;
	std::filesystem::remove_all(_root);
	if (tdata == Tdata::Present) {
		std::filesystem::create_directories(_root / "tdata");
	} else {
		std::filesystem::create_directories(_root);
	}
	std::filesystem::current_path(_root);
}

TempDatabaseDirectory::~TempDatabaseDirectory() {
	std::filesystem::current_path(_previous);
	auto error = std::error_code();
	std::filesystem::remove_all(_root, error);
}

template <typename T>
[[nodiscard]] T MakeMessage(
		ID userId,
		ID dialogId,
		ID topicId,
		int messageId,
		const std::string &text) {
	auto result = T();
	result.userId = userId;
	result.dialogId = dialogId;
	result.peerId = dialogId;
	result.fromId = userId;
	result.topicId = topicId;
	result.messageId = messageId;
	result.date = 1700000000;
	result.entityCreateDate = 1700000100;
	result.text = text;
	return result;
}

[[nodiscard]] DeletedMessage MakeDeleted(
		ID userId,
		ID dialogId,
		ID topicId,
		int messageId,
		const std::string &text) {
	return MakeMessage<DeletedMessage>(userId, dialogId, topicId, messageId, text);
}

[[nodiscard]] EditedMessage MakeEdited(
		ID userId,
		ID dialogId,
		ID topicId,
		int messageId,
		const std::string &text) {
	return MakeMessage<EditedMessage>(userId, dialogId, topicId, messageId, text);
}

template <typename T>
[[nodiscard]] Ids IdsOf(const std::vector<T> &list) {
	auto result = Ids();
	result.reserve(list.size());
	for (const auto &entry : list) {
		result.push_back(entry.messageId);
	}
	return result;
}

template <typename T>
[[nodiscard]] Texts TextsOf(const std::vector<T> &list) {
	auto result = Texts();
	result.reserve(list.size());
	for (const auto &entry : list) {
		result.push_back(entry.text);
	}
	return result;
}

template <typename Callable>
[[nodiscard]] bool Throws(Callable &&body) {
	try {
		body();
	} catch (...) {
		return true;
	}
	return false;
}

void ClobberDatabaseFile() {
	auto file = std::ofstream("./tdata/ayudata.db", std::ios::binary | std::ios::trunc);
	file << "definitely not a sqlite database";
}

[[nodiscard]] int CountMovedAsideDatabases() {
	auto result = 0;
	for (const auto &entry : std::filesystem::directory_iterator("./tdata")) {
		const auto name = entry.path().filename().string();
		if (name.starts_with("ayudata_") && name.ends_with(".db")) {
			++result;
		}
	}
	return result;
}

void TestInitializeOnFreshDirectory() {
	const auto directory = TempDatabaseDirectory();
	CHECK(!std::filesystem::exists("./tdata/ayudata.db"));

	AyuDatabase::initialize();

	CHECK(std::filesystem::exists("./tdata/ayudata.db"));
	CHECK(!AyuDatabase::hasDeletedMessages(100, 200, 0));
	CHECK(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100).empty());
	CHECK(!AyuDatabase::hasRevisions(100, 200, 5));
	CHECK(AyuDatabase::getEditedMessages(100, 200, 5, 0, 0, 100).empty());

	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 7, "after initialize"));
	CHECK(TextsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100)) == Texts{"after initialize"});

	AyuDatabase::initialize();
	CHECK(TextsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100)) == Texts{"after initialize"});
}

void TestInitializeMovesCorruptedDatabaseAside() {
	const auto directory = TempDatabaseDirectory();
	{
		auto file = std::ofstream("./tdata/ayudata.db", std::ios::binary);
		file << "definitely not a sqlite database";
	}
	CHECK(CountMovedAsideDatabases() == 0);

	AyuDatabase::initialize();

	CHECK(CountMovedAsideDatabases() == 1);
	CHECK(!AyuDatabase::hasDeletedMessages(100, 200, 0));

	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 1, "after recovery"));
	CHECK(TextsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100)) == Texts{"after recovery"});
}

void TestDeletedMessageRoundtrip() {
	const auto directory = TempDatabaseDirectory();
	AyuDatabase::initialize();

	CHECK(!AyuDatabase::hasDeletedMessages(100, 200, 0));

	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 42, "recalled text"));

	CHECK(AyuDatabase::hasDeletedMessages(100, 200, 0));

	const auto messages = AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100);
	CHECK(messages.size() == 1);
	if (messages.size() != 1) {
		return;
	}
	CHECK(messages[0].messageId == 42);
	CHECK(messages[0].text == "recalled text");
	CHECK(messages[0].userId == 100);
	CHECK(messages[0].dialogId == 200);
	CHECK(messages[0].peerId == 200);
	CHECK(messages[0].fromId == 100);
	CHECK(messages[0].date == 1700000000);
	CHECK(messages[0].entityCreateDate == 1700000100);
	CHECK(messages[0].fakeId > 0);
}

void TestDeletedMessagesScoping() {
	const auto directory = TempDatabaseDirectory();
	AyuDatabase::initialize();

	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 1, "dialog 200"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 201, 0, 2, "dialog 201"));
	AyuDatabase::addDeletedMessage(MakeDeleted(101, 200, 0, 3, "other account"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 300, 10, 4, "topic 10"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 300, 20, 5, "topic 20"));

	CHECK(TextsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100)) == Texts{"dialog 200"});
	CHECK(TextsOf(AyuDatabase::getDeletedMessages(100, 201, 0, 0, 0, 100)) == Texts{"dialog 201"});
	CHECK(TextsOf(AyuDatabase::getDeletedMessages(101, 200, 0, 0, 0, 100)) == Texts{"other account"});
	CHECK(TextsOf(AyuDatabase::getDeletedMessages(100, 300, 10, 0, 0, 100)) == Texts{"topic 10"});
	CHECK(TextsOf(AyuDatabase::getDeletedMessages(100, 300, 20, 0, 0, 100)) == Texts{"topic 20"});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 300, 0, 0, 0, 100)) == Ids{5, 4});
	CHECK(AyuDatabase::getDeletedMessages(100, 300, 30, 0, 0, 100).empty());
	CHECK(AyuDatabase::getDeletedMessages(100, 999, 0, 0, 0, 100).empty());
	CHECK(AyuDatabase::getDeletedMessages(999, 200, 0, 0, 0, 100).empty());

	CHECK(AyuDatabase::hasDeletedMessages(100, 300, 10));
	CHECK(AyuDatabase::hasDeletedMessages(100, 300, 20));
	CHECK(AyuDatabase::hasDeletedMessages(100, 300, 0));
	CHECK(!AyuDatabase::hasDeletedMessages(100, 300, 30));
	CHECK(!AyuDatabase::hasDeletedMessages(100, 999, 0));
	CHECK(!AyuDatabase::hasDeletedMessages(999, 200, 0));
}

void TestDeletedMessagesRange() {
	const auto directory = TempDatabaseDirectory();
	AyuDatabase::initialize();

	for (auto messageId = 1; messageId <= 10; ++messageId) {
		AyuDatabase::addDeletedMessage(
			MakeDeleted(100, 200, 0, messageId, "m" + std::to_string(messageId)));
	}

	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100))
		== Ids{10, 9, 8, 7, 6, 5, 4, 3, 2, 1});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 7, 0, 100)) == Ids{10, 9, 8});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 4, 100)) == Ids{3, 2, 1});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 3, 7, 100)) == Ids{6, 5, 4});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 3)) == Ids{10, 9, 8});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 3, 0, 2)) == Ids{10, 9});
	CHECK(AyuDatabase::getDeletedMessages(100, 200, 0, 10, 0, 100).empty());
	CHECK(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 1, 100).empty());

	const auto firstPage = AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 4);
	CHECK(IdsOf(firstPage) == Ids{10, 9, 8, 7});
	const auto secondPage = AyuDatabase::getDeletedMessages(100, 200, 0, 0, firstPage.back().messageId, 4);
	CHECK(IdsOf(secondPage) == Ids{6, 5, 4, 3});
	const auto thirdPage = AyuDatabase::getDeletedMessages(100, 200, 0, 0, secondPage.back().messageId, 4);
	CHECK(IdsOf(thirdPage) == Ids{2, 1});
	CHECK(AyuDatabase::getDeletedMessages(100, 200, 0, 0, thirdPage.back().messageId, 4).empty());
}

void TestClearDeletedMessages() {
	const auto directory = TempDatabaseDirectory();
	AyuDatabase::initialize();

	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 10, 1, "dialog 200 topic 10"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 20, 2, "dialog 200 topic 20"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 201, 0, 3, "dialog 201"));
	AyuDatabase::addDeletedMessage(MakeDeleted(101, 200, 10, 4, "other account"));

	AyuDatabase::clearDeletedMessages(100, 200, 10);

	CHECK(!AyuDatabase::hasDeletedMessages(100, 200, 10));
	CHECK(TextsOf(AyuDatabase::getDeletedMessages(100, 200, 20, 0, 0, 100))
		== Texts{"dialog 200 topic 20"});
	CHECK(TextsOf(AyuDatabase::getDeletedMessages(100, 201, 0, 0, 0, 100)) == Texts{"dialog 201"});
	CHECK(TextsOf(AyuDatabase::getDeletedMessages(101, 200, 10, 0, 0, 100)) == Texts{"other account"});

	AyuDatabase::clearDeletedMessages(100, 200, 0);

	CHECK(!AyuDatabase::hasDeletedMessages(100, 200, 0));
	CHECK(TextsOf(AyuDatabase::getDeletedMessages(100, 201, 0, 0, 0, 100)) == Texts{"dialog 201"});
	CHECK(TextsOf(AyuDatabase::getDeletedMessages(101, 200, 10, 0, 0, 100)) == Texts{"other account"});
}

void TestDeletedMessagesSearch() {
	const auto directory = TempDatabaseDirectory();
	AyuDatabase::initialize();

	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 1, "hello world"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 2, "goodbye world"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 3, "hello there"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 4, "discount 100% today"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 5, "discount 1005 today"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 6, "snake_case name"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 7, "snakeXcase name"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 8, "path C:\\temp here"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 201, 0, 9, "hello from another dialog"));

	CHECK(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100, "").size() == 8);
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100, "hello")) == Ids{3, 1});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100, "world")) == Ids{2, 1});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100, "HELLO")) == Ids{3, 1});
	CHECK(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100, "nothing here").empty());
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 201, 0, 0, 0, 100, "hello")) == Ids{9});

	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100, "100%")) == Ids{4});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100, "snake_case")) == Ids{6});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100, "C:\\")) == Ids{8});

	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 2, 0, 100, "hello")) == Ids{3});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 3, 100, "hello")) == Ids{1});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 1, "hello")) == Ids{3});
}

void TestSearchIsTopicScoped() {
	const auto directory = TempDatabaseDirectory();
	AyuDatabase::initialize();

	AyuDatabase::addDeletedMessage(MakeDeleted(100, 300, 10, 1, "shared word one"));
	AyuDatabase::addDeletedMessage(MakeDeleted(100, 300, 20, 2, "shared word two"));

	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 300, 10, 0, 0, 100, "shared")) == Ids{1});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 300, 20, 0, 0, 100, "shared")) == Ids{2});
	CHECK(IdsOf(AyuDatabase::getDeletedMessages(100, 300, 0, 0, 0, 100, "shared")) == Ids{2, 1});
}

void TestEditedMessagesHistory() {
	const auto directory = TempDatabaseDirectory();
	AyuDatabase::initialize();

	CHECK(!AyuDatabase::hasRevisions(100, 200, 5));
	CHECK(AyuDatabase::getEditedMessages(100, 200, 5, 0, 0, 100).empty());

	AyuDatabase::addEditedMessage(MakeEdited(100, 200, 0, 5, "first"));
	AyuDatabase::addEditedMessage(MakeEdited(100, 200, 0, 5, "second"));
	AyuDatabase::addEditedMessage(MakeEdited(100, 200, 0, 5, "third"));
	AyuDatabase::addEditedMessage(MakeEdited(100, 200, 0, 6, "other message"));
	AyuDatabase::addEditedMessage(MakeEdited(100, 201, 0, 5, "other dialog"));
	AyuDatabase::addEditedMessage(MakeEdited(101, 200, 0, 5, "other account"));

	CHECK(AyuDatabase::hasRevisions(100, 200, 5));
	CHECK(AyuDatabase::hasRevisions(100, 200, 6));
	CHECK(!AyuDatabase::hasRevisions(100, 200, 7));
	CHECK(!AyuDatabase::hasRevisions(100, 999, 5));
	CHECK(!AyuDatabase::hasRevisions(999, 200, 5));

	const auto revisions = AyuDatabase::getEditedMessages(100, 200, 5, 0, 0, 100);
	CHECK(TextsOf(revisions) == Texts{"third", "second", "first"});
	CHECK(TextsOf(AyuDatabase::getEditedMessages(100, 200, 6, 0, 0, 100)) == Texts{"other message"});
	CHECK(TextsOf(AyuDatabase::getEditedMessages(100, 201, 5, 0, 0, 100)) == Texts{"other dialog"});
	CHECK(TextsOf(AyuDatabase::getEditedMessages(101, 200, 5, 0, 0, 100)) == Texts{"other account"});
	CHECK(TextsOf(AyuDatabase::getEditedMessages(100, 200, 5, 0, 0, 2)) == Texts{"third", "second"});

	CHECK(revisions.size() == 3);
	if (revisions.size() != 3) {
		return;
	}
	CHECK(revisions[0].fakeId > revisions[1].fakeId);
	CHECK(revisions[1].fakeId > revisions[2].fakeId);

	const auto middle = revisions[1].fakeId;
	CHECK(TextsOf(AyuDatabase::getEditedMessages(100, 200, 5, middle, 0, 100)) == Texts{"third"});
	CHECK(TextsOf(AyuDatabase::getEditedMessages(100, 200, 5, 0, middle, 100)) == Texts{"first"});
	CHECK(AyuDatabase::getEditedMessages(100, 200, 5, middle, middle, 100).empty());
}

void TestEditedAndDeletedStayIndependent() {
	const auto directory = TempDatabaseDirectory();
	AyuDatabase::initialize();

	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 5, "deleted body"));
	AyuDatabase::addEditedMessage(MakeEdited(100, 200, 0, 5, "edited body"));

	CHECK(TextsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100)) == Texts{"deleted body"});
	CHECK(TextsOf(AyuDatabase::getEditedMessages(100, 200, 5, 0, 0, 100)) == Texts{"edited body"});

	AyuDatabase::clearDeletedMessages(100, 200, 0);

	CHECK(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100).empty());
	CHECK(AyuDatabase::hasRevisions(100, 200, 5));
	CHECK(TextsOf(AyuDatabase::getEditedMessages(100, 200, 5, 0, 0, 100)) == Texts{"edited body"});
}

void TestFreshDirectoryPerTest() {
	const auto directory = TempDatabaseDirectory();
	CHECK(!std::filesystem::exists("./tdata/ayudata.db"));

	AyuDatabase::initialize();

	CHECK(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100).empty());
	CHECK(AyuDatabase::getEditedMessages(100, 200, 5, 0, 0, 100).empty());
	CHECK(!AyuDatabase::hasDeletedMessages(100, 200, 0));
	CHECK(!AyuDatabase::hasRevisions(100, 200, 5));
}

void TestReadsSurviveACorruptedDatabase() {
	const auto directory = TempDatabaseDirectory();
	AyuDatabase::initialize();

	AyuDatabase::addDeletedMessage(MakeDeleted(100, 200, 0, 7, "before corruption"));
	AyuDatabase::addEditedMessage(MakeEdited(100, 200, 0, 7, "before corruption"));
	CHECK(TextsOf(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100)) == Texts{"before corruption"});
	CHECK(TextsOf(AyuDatabase::getEditedMessages(100, 200, 7, 0, 0, 100)) == Texts{"before corruption"});

	ClobberDatabaseFile();

	CHECK(!Throws([] { AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100); }));
	CHECK(!Throws([] { AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100, "before"); }));
	CHECK(!Throws([] { AyuDatabase::getEditedMessages(100, 200, 7, 0, 0, 100); }));

	CHECK(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100).empty());
	CHECK(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100, "before").empty());
	CHECK(AyuDatabase::getEditedMessages(100, 200, 7, 0, 0, 100).empty());
	CHECK(!AyuDatabase::hasDeletedMessages(100, 200, 0));
	CHECK(!AyuDatabase::hasRevisions(100, 200, 7));
}

void TestInitializeSurvivesAnUnusableDirectory() {
	const auto directory = TempDatabaseDirectory(TempDatabaseDirectory::Tdata::Missing);
	CHECK(!std::filesystem::exists("./tdata"));

	CHECK(!Throws([] { AyuDatabase::initialize(); }));
	CHECK(!std::filesystem::exists("./tdata/ayudata.db"));

	CHECK(!Throws([] { AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100); }));
	CHECK(!Throws([] { AyuDatabase::getEditedMessages(100, 200, 5, 0, 0, 100); }));

	CHECK(AyuDatabase::getDeletedMessages(100, 200, 0, 0, 0, 100).empty());
	CHECK(AyuDatabase::getEditedMessages(100, 200, 5, 0, 0, 100).empty());
	CHECK(!AyuDatabase::hasDeletedMessages(100, 200, 0));
	CHECK(!AyuDatabase::hasRevisions(100, 200, 5));
}

} // namespace

int main() {
	Run("initialize on a fresh directory", TestInitializeOnFreshDirectory);
	Run("initialize moves a corrupted database aside", TestInitializeMovesCorruptedDatabaseAside);
	Run("deleted message roundtrip", TestDeletedMessageRoundtrip);
	Run("deleted messages are scoped by account, dialog and topic", TestDeletedMessagesScoping);
	Run("deleted messages minId / maxId / totalLimit", TestDeletedMessagesRange);
	Run("clearDeletedMessages targets one dialog and topic", TestClearDeletedMessages);
	Run("deleted messages search query", TestDeletedMessagesSearch);
	Run("search query respects topic scope", TestSearchIsTopicScoped);
	Run("edit history revisions", TestEditedMessagesHistory);
	Run("edit history and deleted messages are independent", TestEditedAndDeletedStayIndependent);
	Run("every test runs against a fresh database", TestFreshDirectoryPerTest);
	Run("reads degrade instead of throwing on a corrupt database", TestReadsSurviveACorruptedDatabase);

	// Keep last. sqlite_orm's connection_holder raises its retain count before
	// calling sqlite3_open and throws without lowering it again when the open
	// fails, so the count never returns to zero and every later call reuses the
	// handle left behind by that failed open. One directory the database cannot
	// be created in therefore poisons the storage for the rest of the process.
	Run("initialize survives a directory it cannot use", TestInitializeSurvivesAnUnusableDirectory);

	AyuTests::RunEntitiesTests();
	AyuTests::RunTelegramHelpersTests();

	std::printf("\n%d passed, %d failed\n", AyuTests::PassedCount, AyuTests::FailedCount);
	return AyuTests::FailedCount ? 1 : 0;
}
