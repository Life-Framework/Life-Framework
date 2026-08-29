// red-proof: change an expected value (e.g. level 1 next-xp 100 -> 50) or
// break the CheckLevelUp cascade, then run the fast tier.

class EL_Test_LevelThresholds : EL_Test
{
	override string GetName()
	{
		return "level/threshold-math";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_TestLevelProbe probe = new EL_TestLevelProbe();
		probe.SetLevelForTest(1);
		ctx.EqualFloat(100, probe.GetExperienceForNextLevel(), 0.001, "level 1 requires 100 xp");

		probe.SetLevelForTest(3);
		ctx.EqualFloat(300, probe.GetExperienceForNextLevel(), 0.001, "level 3 requires 300 xp");

		probe.SetLevelForTest(10);
		ctx.EqualFloat(1000, probe.GetExperienceForNextLevel(), 0.001, "level 10 requires 1000 xp");
	}
};

//------------------------------------------------------------------------------------------------
class EL_Test_LevelBonuses : EL_Test
{
	override string GetName()
	{
		return "level/bonus-formulas";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_TestLevelProbe probe = new EL_TestLevelProbe();
		probe.SetLevelForTest(1);
		ctx.EqualFloat(1.0, probe.GetGatheringSpeedMultiplier(), 0.001, "level 1 gathering speed multiplier is 1.0");
		ctx.EqualFloat(1.0, probe.GetGatheringAmountBonus(), 0.001, "level 1 gathering amount bonus is 1.0");
		ctx.EqualFloat(1.0, probe.GetSaleBonus(), 0.001, "level 1 sale bonus is 1.0");

		probe.SetLevelForTest(3);
		ctx.EqualFloat(1.2, probe.GetGatheringSpeedMultiplier(), 0.001, "level 3 gathering speed multiplier is 1.2");
		ctx.EqualFloat(1.2, probe.GetGatheringAmountBonus(), 0.001, "level 3 gathering amount bonus is 1.2");
		ctx.EqualFloat(1.1, probe.GetSaleBonus(), 0.001, "level 3 sale bonus is 1.1");
	}
};

//------------------------------------------------------------------------------------------------
//! Level-up cascade math through CheckLevelUp's invariant: after any level-up
//! the player's xp is always below the next threshold, skill points and total
//! earned each rise by the number of levels gained.
class EL_Test_LevelUpCascade : EL_Test
{
	override string GetName()
	{
		return "level/levelup-cascade";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_TestLevelProbe probe = new EL_TestLevelProbe();
		probe.SetLevelForTest(1);
		probe.SetSkillPointsForTest(0);
		probe.SetTotalSkillPointsEarnedForTest(0);
		probe.SetExperienceForTest(250);

		probe.RunCheckLevelUp();

		ctx.Equal(3, probe.GetLevel(), "250 xp from level 1 cascades to level 3");
		ctx.True(probe.GetExperience() < probe.GetExperienceForNextLevel(), "xp drained below the next threshold after cascade");
		ctx.Equal(2, probe.GetSkillPoints(), "two levels gained grant two skill points");
		ctx.Equal(2, probe.GetTotalSkillPointsEarned(), "two levels gained record two total earned");
	}
};