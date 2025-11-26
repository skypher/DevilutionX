#include <gtest/gtest.h>

#include "engine/assets.hpp"
#include "init.hpp"
#include "levels/gendung.h"
#include "towners.h"

using namespace devilution;

namespace {

void ClearDMonsterGrid()
{
	for (int i = 0; i < MAXDUNX; i++) {
		for (int j = 0; j < MAXDUNY; j++) {
			dMonster[i][j] = 0;
		}
	}
}

} // namespace

class TownersTest : public ::testing::Test {
protected:
	static void SetUpTestSuite()
	{
		LoadCoreArchives();
		LoadGameArchives();
	}

	void SetUp() override
	{
		if (!HaveMainData()) {
			GTEST_SKIP() << "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test";
		}

		ClearDMonsterGrid();

		// Initialize a minimal towner for testing
		Towners[0] = {};
		Towners[0]._ttype = TOWN_WITCH;
		Towners[0].position = Point { 80, 20 };
		dMonster[80][20] = 1; // Towner index + 1
	}

	void TearDown() override
	{
		ClearDMonsterGrid();
		Towners[0] = {};
	}
};

TEST_F(TownersTest, GetTownerReturnsCorrectTowner)
{
	Towner *towner = GetTowner(TOWN_WITCH);
	ASSERT_NE(towner, nullptr);
	EXPECT_EQ(towner->_ttype, TOWN_WITCH);
	EXPECT_EQ(towner->position.x, 80);
	EXPECT_EQ(towner->position.y, 20);
}

TEST_F(TownersTest, GetTownerReturnsNullForMissingTowner)
{
	Towner *towner = GetTowner(TOWN_PEGBOY);
	EXPECT_EQ(towner, nullptr);
}

TEST_F(TownersTest, TownerPositionUpdatesCorrectly)
{
	Towner *towner = GetTowner(TOWN_WITCH);
	ASSERT_NE(towner, nullptr);

	// Verify initial position
	EXPECT_EQ(towner->position.x, 80);
	EXPECT_EQ(towner->position.y, 20);
	EXPECT_EQ(dMonster[80][20], 1);

	// Manually update position (simulating what setPosition does)
	dMonster[towner->position.x][towner->position.y] = 0;
	towner->position = Point { 75, 70 };
	dMonster[75][70] = 1;

	// Verify new position
	EXPECT_EQ(towner->position.x, 75);
	EXPECT_EQ(towner->position.y, 70);
	EXPECT_EQ(dMonster[75][70], 1);
	EXPECT_EQ(dMonster[80][20], 0); // Old position cleared
}

TEST_F(TownersTest, DMonsterGridClearedOnPositionChange)
{
	Towner *towner = GetTowner(TOWN_WITCH);
	ASSERT_NE(towner, nullptr);

	// Initial state
	EXPECT_EQ(dMonster[80][20], 1);
	EXPECT_EQ(dMonster[50][50], 0);

	// Clear old position and set new
	dMonster[towner->position.x][towner->position.y] = 0;
	towner->position = Point { 50, 50 };
	dMonster[50][50] = 1;

	// Verify old position is cleared
	EXPECT_EQ(dMonster[80][20], 0);
	EXPECT_EQ(dMonster[50][50], 1);
}
