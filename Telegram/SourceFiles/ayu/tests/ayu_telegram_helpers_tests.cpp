// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/data/entities.h"

#include "ayu/tests/ayu_test_harness.h"

ID getUserIdFromPackId(uint64 id);
int getScheduleTime(int64 sumSize);
QString getDCName(int dc);
QString formatTTL(int time, bool isDoc);

namespace {

using AyuTests::Run;

constexpr auto kMegabyte = int64(1024 * 1024);

void TestPackIdOwnerIsTheHighWord() {
	CHECK(getUserIdFromPackId(0) == 0);
	CHECK(getUserIdFromPackId(0x0000000100000000ULL) == 1);
	CHECK(getUserIdFromPackId(0x0000002A00000000ULL) == 42);
	CHECK(getUserIdFromPackId(0x000000000000FFFFULL) == 0);
	CHECK(getUserIdFromPackId(0x000000050000FFFFULL) == 5);
}

void TestPackIdMarkerByteSetsTheHighBit() {
	CHECK(getUserIdFromPackId(0x00000001003F0000ULL) == 0x80000001LL);
	CHECK(getUserIdFromPackId(0x00000001003E0000ULL) == 1);
	CHECK(getUserIdFromPackId(0x0000000100400000ULL) == 1);
	CHECK(getUserIdFromPackId(0x000000000000003FULL) == 0);
}

void TestPackIdMarkerByteOnlySets() {
	CHECK(getUserIdFromPackId(0x80000000003F0000ULL) == 0x80000000LL);
	CHECK(getUserIdFromPackId(0x8000000000000000ULL) == 0x80000000LL);
}

void TestPackIdSecondMarkerAddsTheThirtyThirdBit() {
	CHECK(getUserIdFromPackId(0x0000000201000000ULL) == 0x100000002LL);
	CHECK(getUserIdFromPackId(0x00000002FF000000ULL) == 0x100000002LL);
	CHECK(getUserIdFromPackId(0x0000000200000000ULL) == 2);
	CHECK(getUserIdFromPackId(0x00000003013F0000ULL) == 0x180000003LL);
}

void TestScheduleTimeHasAFloor() {
	CHECK(getScheduleTime(0) == 19);
	CHECK(getScheduleTime(-1) == 19);
	CHECK(getScheduleTime(1) == 19);
	CHECK(getScheduleTime(kMegabyte) == 19);
	CHECK(getScheduleTime(8 * kMegabyte) == 19);
}

void TestScheduleTimeGrowsWithSize() {
	CHECK(getScheduleTime(9 * kMegabyte) == 20);
	CHECK(getScheduleTime(32 * kMegabyte) == 36);
	CHECK(getScheduleTime(1024 * kMegabyte) == 730);
	CHECK(getScheduleTime(9 * kMegabyte) < getScheduleTime(32 * kMegabyte));
	CHECK(getScheduleTime(32 * kMegabyte) < getScheduleTime(1024 * kMegabyte));
}

void TestScheduleTimeRoundsPartialSecondsUp() {
	CHECK(getScheduleTime(kMegabyte * 25 / 2) == 22);
	CHECK(getScheduleTime(kMegabyte * 41 / 2) == 28);
	CHECK(getScheduleTime(9 * kMegabyte - 1) == 20);
	CHECK(getScheduleTime(8 * kMegabyte + 1) == 19);
}

void TestScheduleTimeFloorHandsOverToTheCeiling() {
	CHECK(getScheduleTime(8987793) == 19);
	CHECK(getScheduleTime(8987794) == 19);
	CHECK(getScheduleTime(8987795) == 20);
	CHECK(getScheduleTime(8987796) == 20);
}

void TestDataCentreNames() {
	CHECK(getDCName(1) == "DC1, Miami FL, USA");
	CHECK(getDCName(2) == "DC2, Amsterdam, NL");
	CHECK(getDCName(3) == "DC3, Miami FL, USA");
	CHECK(getDCName(4) == "DC4, Amsterdam, NL");
	CHECK(getDCName(5) == "DC5, Singapore, SG");
}

void TestUnknownDataCentres() {
	CHECK(getDCName(0) == "DC_UNKNOWN");
	CHECK(getDCName(-1) == "DC_UNKNOWN");
	CHECK(getDCName(6) == "DC6, UNKNOWN");
	CHECK(getDCName(100) == "DC100, UNKNOWN");
}

void TestTimeToLiveFormatting() {
	CHECK(formatTTL(0, false) == "0s");
	CHECK(formatTTL(1, false) == "1s");
	CHECK(formatTTL(60, false) == "60s");
	CHECK(formatTTL(86400, true) == "86400s");
	CHECK(formatTTL(-1, false) == "-1s");
}

void TestTimeToLiveSentinelIsOneViewOnly() {
	CHECK(formatTTL(0x7FFFFFFF, false) == "<ayu_OneViewTTL>");
	CHECK(formatTTL(0x7FFFFFFF, true) == "<ayu_OnePlayTTL>");
	CHECK(formatTTL(0x7FFFFFFE, false) == "2147483646s");
	CHECK(formatTTL(0x7FFFFFFE, true) == "2147483646s");
}

} // namespace

void AyuTests::RunTelegramHelpersTests() {
	Run("sticker pack id carries its owner in the high word", TestPackIdOwnerIsTheHighWord);
	Run("sticker pack id marker byte 0x3f sets bit 31", TestPackIdMarkerByteSetsTheHighBit);
	Run("sticker pack id marker byte 0x3f only sets bit 31", TestPackIdMarkerByteOnlySets);
	Run("sticker pack id third byte adds bit 32", TestPackIdSecondMarkerAddsTheThirtyThirdBit);
	Run("upload schedule never goes below its floor", TestScheduleTimeHasAFloor);
	Run("upload schedule grows with the upload size", TestScheduleTimeGrowsWithSize);
	Run("upload schedule rounds partial seconds up", TestScheduleTimeRoundsPartialSecondsUp);
	Run("upload schedule floor hands over to the ceiling", TestScheduleTimeFloorHandsOverToTheCeiling);
	Run("data centre names", TestDataCentreNames);
	Run("unknown data centres", TestUnknownDataCentres);
	Run("time to live formatting", TestTimeToLiveFormatting);
	Run("time to live sentinel means view once", TestTimeToLiveSentinelIsOneViewOnly);
}
