#include <cstdint>

#include <gtest/gtest.h>

#include "engine/random.hpp"
#include "game_mode.hpp"
#include "levels/gendung.h"
#include "multi.h"
#include "objects.h"
#include "options.h"

namespace devilution {
namespace {

constexpr int ShrineFascinating = 9;
constexpr int ShrineSacred = 16;
constexpr int ShrineOrnate = 23;
constexpr int ShrineMurphys = 33;

bool IsCripplingShrine(int shrineType)
{
	switch (shrineType) {
	case ShrineFascinating:
	case ShrineSacred:
	case ShrineOrnate:
	case ShrineMurphys:
		return true;
	default:
		return false;
	}
}

class CripplingShrineTest : public ::testing::Test {
protected:
	static void ResetBaseState()
	{
		gbIsHellfire = false;
		gbIsMultiplayer = false;
		sgGameInitInfo.SetDisableCripplingShrines(false);
		leveltype = DTYPE_CATHEDRAL;
		GetOptions().Gameplay.disableCripplingShrines.SetValue(false);
	}

	static uint32_t RequireSeedProducingCripplingShrine()
	{
		for (uint32_t seed = 1; seed < 100000; ++seed) {
			ResetBaseState();
			SetRndSeed(seed);
			const int shrineType = FindValidShrine();
			if (IsCripplingShrine(shrineType))
				return seed;
		}
		ADD_FAILURE() << "Unable to produce a crippling shrine with deterministic seeds";
		return 1;
	}
};

} // namespace

TEST_F(CripplingShrineTest, SinglePlayerFiltersCripplingShrines)
{
	const uint32_t seed = RequireSeedProducingCripplingShrine();
	ResetBaseState();
	SetRndSeed(seed);
	const int baseline = FindValidShrine();
	ASSERT_TRUE(IsCripplingShrine(baseline));

	ResetBaseState();
	GetOptions().Gameplay.disableCripplingShrines.SetValue(true);
	SetRndSeed(seed);
	const int filtered = FindValidShrine();
	EXPECT_FALSE(IsCripplingShrine(filtered));
	EXPECT_NE(baseline, filtered);
}

TEST_F(CripplingShrineTest, MultiplayerUsesSyncedDisableFlag)
{
	const uint32_t seed = RequireSeedProducingCripplingShrine();

	ResetBaseState();
	gbIsMultiplayer = true;
	SetRndSeed(seed);
	const int baseline = FindValidShrine();
	ASSERT_TRUE(IsCripplingShrine(baseline));

	ResetBaseState();
	gbIsMultiplayer = true;
	GetOptions().Gameplay.disableCripplingShrines.SetValue(false);
	sgGameInitInfo.SetDisableCripplingShrines(true);
	SetRndSeed(seed);
	const int filtered = FindValidShrine();
	EXPECT_FALSE(IsCripplingShrine(filtered));
}

} // namespace devilution
