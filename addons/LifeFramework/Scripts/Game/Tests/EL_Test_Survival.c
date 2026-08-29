// red-proof: change an expected value (e.g. fresh hunger 100 -> 50) and run
// the fast tier; the survival assertions fail.

class EL_Test_Survival : EL_Test
{
	override string GetName()
	{
		return "survival-stats";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_SurvivalStats stats = EL_SurvivalStats.Create("test-survival");

		ctx.True(Math.AbsFloat(stats.GetHealth() - 100) < 0.001, "fresh stats start at 100 health");
		ctx.True(Math.AbsFloat(stats.GetHunger() - 100) < 0.001, "fresh stats start at 100 hunger");
		ctx.True(Math.AbsFloat(stats.GetThirst() - 100) < 0.001, "fresh stats start at 100 thirst");

		stats.Eat(-50);
		ctx.True(Math.AbsFloat(stats.GetHunger() - 50) < 0.001, "Eat(-50) lowers hunger to 50");

		stats.Eat(200);
		ctx.True(Math.AbsFloat(stats.GetHunger() - 100) < 0.001, "hunger clamps at 100");

		stats.SetHealth(-10);
		ctx.True(Math.AbsFloat(stats.GetHealth()) < 0.001, "health clamps at 0");

		stats.Drink(-60);
		ctx.True(Math.AbsFloat(stats.GetThirst() - 40) < 0.001, "Drink(-60) lowers thirst to 40");

		stats.Drink(200);
		ctx.True(Math.AbsFloat(stats.GetThirst() - 100) < 0.001, "thirst clamps at 100");

		stats.Heal(200);
		ctx.True(Math.AbsFloat(stats.GetHealth() - 100) < 0.001, "Heal clamps at 100");

		stats.Eat(50);
		stats.SetThirst(50);
		stats.SetHealth(100);
		stats.UpdateStats(10);
		ctx.True(Math.AbsFloat(stats.GetHealth() - 100) < 0.001, "no health loss while hunger and thirst stay above 20");
		ctx.True(Math.AbsFloat(stats.GetHunger() - 49) < 0.001, "UpdateStats(10) drops hunger by 1");
		ctx.True(Math.AbsFloat(stats.GetThirst() - 48.5) < 0.001, "UpdateStats(10) drops thirst by 1.5");

		stats.SetHunger(10);
		stats.SetThirst(10);
		stats.SetHealth(100);
		stats.UpdateStats(10);
		ctx.True(Math.AbsFloat(stats.GetHealth() - 99.5) < 0.001, "hunger or thirst below 20 drains health by 0.5 per 10s");

		float thirstBefore = stats.GetThirst();
		stats.UpdateStats(0);
		ctx.True(Math.AbsFloat(stats.GetThirst() - thirstBefore) < 0.001, "UpdateStats(0) is a no-op");

		stats.UpdateStats(-10);
		ctx.True(stats.GetHunger() >= 0 && stats.GetHunger() <= 100, "stats stay in range under negative delta time");
	}
};

//------------------------------------------------------------------------------------------------
class EL_Test_SurvivalSaveData : EL_Test
{
	override string GetName()
	{
		return "survival/save-roundtrip";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_SurvivalStats stats = EL_SurvivalStats.Create("survival-save-source");
		stats.Eat(-30);

		EL_SurvivalStatsSaveData data = new EL_SurvivalStatsSaveData();
		data.ReadFrom(stats);

		EL_SurvivalStats restored = EL_SurvivalStats.Create("other-id");
		data.ApplyTo(restored);
		ctx.True(Math.AbsFloat(restored.GetHunger() - 70) < 0.001, "hunger survives the save round trip");
		ctx.EqualStr("survival-save-source", restored.GetPersistentId(), "persistent id survives the round trip");

		data.m_fHunger = 999;
		EL_SurvivalStats clamped = EL_SurvivalStats.Create("clamp-id");
		data.ApplyTo(clamped);
		ctx.True(Math.AbsFloat(clamped.GetHunger() - 100) < 0.001, "corrupt save data is re-clamped on load");

		EL_SurvivalStatsSaveData same = new EL_SurvivalStatsSaveData();
		same.ReadFrom(stats);
		EL_SurvivalStatsSaveData clone = new EL_SurvivalStatsSaveData();
		clone.ReadFrom(stats);
		ctx.True(same.Equals(clone), "equal stats compare equal in save data");
	}
};