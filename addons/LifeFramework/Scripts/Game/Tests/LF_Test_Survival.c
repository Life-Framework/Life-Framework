// red-proof: change an expected value (e.g. fresh hunger 100 -> 50) and run
// the fast tier; the survival assertions fail.

// tier: LOGIC
class LF_Test_Survival : LF_Test
{
	override string GetName()
	{
		return "survival-stats";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_SurvivalStats stats = LF_SurvivalStats.Create("test-survival");

		ctx.True(Math.AbsFloat(stats.GetHunger() - 100) < 0.001, "fresh stats start at 100 hunger");
		ctx.True(Math.AbsFloat(stats.GetThirst() - 100) < 0.001, "fresh stats start at 100 thirst");

		stats.Eat(-50);
		ctx.True(Math.AbsFloat(stats.GetHunger() - 50) < 0.001, "Eat(-50) lowers hunger to 50");

		stats.Eat(200);
		ctx.True(Math.AbsFloat(stats.GetHunger() - 100) < 0.001, "hunger clamps at 100");

		stats.Drink(-60);
		ctx.True(Math.AbsFloat(stats.GetThirst() - 40) < 0.001, "Drink(-60) lowers thirst to 40");

		stats.Drink(200);
		ctx.True(Math.AbsFloat(stats.GetThirst() - 100) < 0.001, "thirst clamps at 100");

		// Explicit rates so the arithmetic below does not move with the flagship config.
		LF_SurvivalConfig rates = LF_SurvivalConfig.CreateDefault();
		rates.m_fHungerDrainPerSecond = 0.1;
		rates.m_fThirstDrainPerSecond = 0.15;
		stats.SetHunger(50);
		stats.SetThirst(50);
		stats.UpdateStats(10, rates);
		ctx.True(Math.AbsFloat(stats.GetHunger() - 49) < 0.001, "UpdateStats(10) drops hunger by 1");
		ctx.True(Math.AbsFloat(stats.GetThirst() - 48.5) < 0.001, "UpdateStats(10) drops thirst by 1.5");

		stats.SetHunger(10);
		stats.SetThirst(10);
		stats.UpdateStats(10, rates);

		float thirstBefore = stats.GetThirst();
		stats.UpdateStats(0, rates);
		ctx.True(Math.AbsFloat(stats.GetThirst() - thirstBefore) < 0.001, "UpdateStats(0) is a no-op");

		stats.UpdateStats(-10, rates);
		ctx.True(stats.GetHunger() >= 0 && stats.GetHunger() <= 100, "stats stay in range under negative delta time");
	}
};

//------------------------------------------------------------------------------------------------
//! Save-data round trip through the persisted record.
// tier: LOGIC
class LF_Test_SurvivalSaveData : LF_Test
{
	override string GetName()
	{
		return "survival/save-roundtrip";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_SurvivalStats stats = LF_SurvivalStats.Create("survival-save-source");
		stats.Eat(-30);

		// Serialize: copy the live values into the persisted record.
		LF_SurvivalStatsRecord record = LF_SurvivalStatsRecord.Create(
			stats.GetPersistentId(), stats.GetHunger(), stats.GetThirst(), LF_SurvivalStatsRecord.RETIRED_HEALTH);

		// Deserialize: apply the record back through the public setters.
		LF_SurvivalStats restored = LF_SurvivalStats.Create("other-id");
		restored.SetPersistentId(record.m_sPersistentId);
		restored.SetHunger(record.m_fHunger);
		restored.SetThirst(record.m_fThirst);

		ctx.True(Math.AbsFloat(restored.GetHunger() - 70) < 0.001, "hunger survives the save round trip");
		ctx.EqualStr("survival-save-source", restored.GetPersistentId(), "persistent id survives the round trip");

		// Corrupt save data is re-clamped by the public setter on load.
		LF_SurvivalStatsRecord corrupt = LF_SurvivalStatsRecord.Create("clamp-id", 999, 100, 100);
		LF_SurvivalStats clamped = LF_SurvivalStats.Create("clamp-id");
		clamped.SetHunger(corrupt.m_fHunger);
		ctx.True(Math.AbsFloat(clamped.GetHunger() - 100) < 0.001, "corrupt save data is re-clamped on load");
	}
};
