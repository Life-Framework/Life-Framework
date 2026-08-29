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

		float thirstBefore = stats.GetThirst();
		stats.UpdateStats(10);
		ctx.True(stats.GetThirst() < thirstBefore, "UpdateStats reduces thirst over time");
		ctx.True(stats.GetHunger() < 100, "UpdateStats reduces hunger over time");
	}
};