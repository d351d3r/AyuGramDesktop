// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/data/entities.h"

#include "ayu/tests/ayu_test_harness.h"

namespace {

using Bytes = std::vector<char>;
using AyuTests::Run;

[[nodiscard]] Bytes BytesOf(std::initializer_list<unsigned char> values) {
	return Bytes(values.begin(), values.end());
}

[[nodiscard]] RegexFilter MakeFilter() {
	auto result = RegexFilter();
	result.id = Bytes{'i', 'd', '1', '\0'};
	result.text = "spam|scam";
	result.enabled = true;
	result.reversed = false;
	result.caseInsensitive = true;
	result.dialogId = 200;
	return result;
}

[[nodiscard]] RegexFilterGlobalExclusion MakeExclusion() {
	auto result = RegexFilterGlobalExclusion();
	result.fakeId = 1;
	result.dialogId = 200;
	result.filterId = Bytes{'i', 'd', '1', '\0'};
	return result;
}

void TestRegexFilterEqualityCoversEveryField() {
	const auto original = MakeFilter();

	CHECK(original == MakeFilter());
	CHECK(!(original != MakeFilter()));

	auto otherId = MakeFilter();
	otherId.id = Bytes{'i', 'd', '2', '\0'};
	CHECK(original != otherId);

	auto otherText = MakeFilter();
	otherText.text = "spam|scam ";
	CHECK(original != otherText);

	auto otherEnabled = MakeFilter();
	otherEnabled.enabled = false;
	CHECK(original != otherEnabled);

	auto otherReversed = MakeFilter();
	otherReversed.reversed = true;
	CHECK(original != otherReversed);

	auto otherCaseInsensitive = MakeFilter();
	otherCaseInsensitive.caseInsensitive = false;
	CHECK(original != otherCaseInsensitive);

	auto otherDialogId = MakeFilter();
	otherDialogId.dialogId = 201;
	CHECK(original != otherDialogId);
}

void TestRegexFilterEqualityOnAbsentDialogId() {
	auto global = MakeFilter();
	global.dialogId = std::nullopt;

	auto perDialog = MakeFilter();
	perDialog.dialogId = 200;

	CHECK(global != perDialog);
	CHECK(perDialog != global);

	auto otherGlobal = MakeFilter();
	otherGlobal.dialogId = std::nullopt;
	CHECK(global == otherGlobal);

	otherGlobal.text = "other";
	CHECK(global != otherGlobal);
}

void TestRegexFilterEqualityComparesWholeId() {
	auto first = MakeFilter();
	auto second = MakeFilter();

	first.id = Bytes{'a', '\0', 'b'};

	second.id = Bytes{'a', '\0', 'c'};
	CHECK(first != second);

	second.id = Bytes{'a'};
	CHECK(first != second);

	second.id = Bytes{'a', '\0', 'b'};
	CHECK(first == second);
}

void TestRegexFilterToJson() {
	const auto json = MakeFilter().toJson();

	CHECK(json.size() == 6);
	CHECK(json.serialized("id") == "69643100");
	CHECK(json.serialized("text") == "spam|scam");
	CHECK(json.serialized("enabled") == "true");
	CHECK(json.serialized("reversed") == "false");
	CHECK(json.serialized("caseInsensitive") == "true");
	CHECK(json.serialized("dialogId") == "200");
}

void TestRegexFilterToJsonBooleansFollowTheirFields() {
	auto filter = MakeFilter();
	filter.enabled = false;
	filter.reversed = true;
	filter.caseInsensitive = false;
	const auto json = filter.toJson();

	CHECK(json.serialized("enabled") == "false");
	CHECK(json.serialized("reversed") == "true");
	CHECK(json.serialized("caseInsensitive") == "false");
}

void TestRegexFilterToJsonKeepsWholeDialogId() {
	auto filter = MakeFilter();
	filter.dialogId = -1001234567890LL;
	CHECK(filter.toJson().serialized("dialogId") == "-1001234567890");

	filter.dialogId = 0;
	CHECK(filter.toJson().contains("dialogId"));
	CHECK(filter.toJson().serialized("dialogId") == "0");
}

void TestRegexFilterToJsonOmitsAbsentDialogId() {
	auto filter = MakeFilter();
	filter.dialogId = std::nullopt;
	const auto json = filter.toJson();

	CHECK(json.size() == 5);
	CHECK(!json.contains("dialogId"));
	CHECK(json.contains("id"));
	CHECK(json.contains("text"));
	CHECK(json.contains("enabled"));
	CHECK(json.contains("reversed"));
	CHECK(json.contains("caseInsensitive"));
}

void TestRegexFilterToJsonWritesTheIdAsAUuid() {
	auto filter = MakeFilter();
	filter.id = BytesOf({
		0xde, 0xad, 0xbe, 0xef,
		0xfe, 0xed, 0x4a, 0xce,
		0xba, 0xbe, 0xca, 0xfe,
		0xd0, 0x0d, 0xf0, 0x0d,
	});

	CHECK(filter.toJson().serialized("id") == "deadbeef-feed-4ace-babe-cafed00df00d");
}

void TestRegexFilterToJsonKeepsEveryIdByte() {
	auto filter = MakeFilter();
	filter.id = BytesOf({
		0x00, 0x11, 0x22, 0x33,
		0x44, 0x55, 0x46, 0x77,
		0x88, 0x99, 0xaa, 0xbb,
		0xcc, 0xdd, 0xee, 0xff,
	});
	CHECK(filter.toJson().serialized("id") == "00112233-4455-4677-8899-aabbccddeeff");

	filter.id = BytesOf({0x00, 0x01});
	CHECK(filter.toJson().serialized("id") == "0001");

	filter.id = BytesOf({0x61, 0x00, 0x62});
	CHECK(filter.toJson().serialized("id") == "610062");

	filter.id = Bytes();
	CHECK(filter.toJson().serialized("id") == "");
	CHECK(filter.toJson().contains("id"));
}

void TestGlobalExclusionEqualityIgnoresFakeId() {
	const auto original = MakeExclusion();

	auto otherFakeId = MakeExclusion();
	otherFakeId.fakeId = 999;
	CHECK(original == otherFakeId);
	CHECK(!(original != otherFakeId));

	auto otherDialogId = MakeExclusion();
	otherDialogId.dialogId = 201;
	CHECK(original != otherDialogId);

	auto otherFilterId = MakeExclusion();
	otherFilterId.filterId = Bytes{'i', 'd', '2', '\0'};
	CHECK(original != otherFilterId);

	auto emptyFilterId = MakeExclusion();
	emptyFilterId.filterId = Bytes();
	CHECK(original != emptyFilterId);
}

} // namespace

void AyuTests::RunEntitiesTests() {
	Run("RegexFilter equality compares every field", TestRegexFilterEqualityCoversEveryField);
	Run("RegexFilter equality on an absent dialogId", TestRegexFilterEqualityOnAbsentDialogId);
	Run("RegexFilter equality compares ids byte by byte", TestRegexFilterEqualityComparesWholeId);
	Run("RegexFilter::toJson writes every field", TestRegexFilterToJson);
	Run("RegexFilter::toJson booleans follow their fields", TestRegexFilterToJsonBooleansFollowTheirFields);
	Run("RegexFilter::toJson keeps the whole dialogId", TestRegexFilterToJsonKeepsWholeDialogId);
	Run("RegexFilter::toJson omits an absent dialogId", TestRegexFilterToJsonOmitsAbsentDialogId);
	Run("RegexFilter::toJson writes the id as a UUID", TestRegexFilterToJsonWritesTheIdAsAUuid);
	Run("RegexFilter::toJson keeps every id byte", TestRegexFilterToJsonKeepsEveryIdByte);
	Run("RegexFilterGlobalExclusion equality ignores fakeId", TestGlobalExclusionEqualityIgnoresFakeId);
}
