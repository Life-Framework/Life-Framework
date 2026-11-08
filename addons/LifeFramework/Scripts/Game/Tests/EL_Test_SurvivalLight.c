// red-proof: observed 2026-11-08 against the old hard-coded UpdateStats (0.1/0.15 drain, health
// -0.05/s below 20 to zero, no stamina hook): FAIL survival/light "zero needs never push health
// below the cap (lowest 0, cap 75)", "needs never lower health that is already below the cap (got
// 10)", "hunger drain comes from config (got 99.9)", "low needs raise stamina drain (got 1)".

//------------------------------------------------------------------------------------------------
//! Survival needs are light (docs/decisions.md, 2026-11-08): zero hunger and thirst hold health
//! at the config cap and never kill, low needs raise stamina drain until the player eats and
//! drinks, and every rate comes from LF_SurvivalConfig.
// tier: LOGIC
class EL_Test_SurvivalLight : EL_Test
{
	override string GetName()
	{
		return "survival/light";
	}

	override void Run(EL_TestContext ctx)
	{
		LF_SurvivalConfig flagship = new LF_SurvivalConfig();

		// Zero needs for ten hours of one-second ticks: health stops at the cap and never reaches zero.
		EL_SurvivalStats starving = EL_SurvivalStats.Create("light-starving");
		starving.SetHunger(0);
		starving.SetThirst(0);
		float lowestHealth = 100;
		for (int i = 0; i < 36000; i++)
		{
			starving.UpdateStats(1, flagship);
			if (starving.GetHealth() < lowestHealth)
				lowestHealth = starving.GetHealth();
		}
		ctx.True(lowestHealth > flagship.m_fHealthCap - 0.001, string.Format("zero needs never push health below the cap (lowest %1, cap %2)", lowestHealth, flagship.m_fHealthCap));
		ctx.True(Math.AbsFloat(starving.GetHealth() - flagship.m_fHealthCap) < 0.001, string.Format("zero needs settle health at the cap (got %1)", starving.GetHealth()));

		// Health already below the cap (an injury) is not reduced further by needs.
		EL_SurvivalStats injured = EL_SurvivalStats.Create("light-injured");
		injured.SetHunger(0);
		injured.SetThirst(0);
		injured.SetHealth(40);
		injured.UpdateStats(600, flagship);
		ctx.True(Math.AbsFloat(injured.GetHealth() - 40) < 0.001, string.Format("needs never lower health that is already below the cap (got %1)", injured.GetHealth()));

		// Flagship drain is slow: an hour after a full meal the player is not hungry yet.
		EL_SurvivalStats fed = EL_SurvivalStats.Create("light-fed");
		fed.UpdateStats(3600, flagship);
		ctx.False(fed.AreNeedsLow(flagship), string.Format("an hour after a full meal needs are not low (hunger %1, thirst %2)", fed.GetHunger(), fed.GetThirst()));

		// Stamina penalty applies at low needs and lifts after eating and drinking.
		EL_SurvivalStats tired = EL_SurvivalStats.Create("light-tired");
		tired.SetHunger(5);
		tired.SetThirst(5);
		ctx.True(Math.AbsFloat(tired.GetStaminaDrainMultiplier(flagship) - flagship.m_fStaminaDrainMultiplier) < 0.001, string.Format("low needs raise stamina drain (got %1)", tired.GetStaminaDrainMultiplier(flagship)));
		tired.Eat(60);
		ctx.True(tired.GetStaminaDrainMultiplier(flagship) > 1.001, "eating alone does not lift the penalty while thirst is low");
		tired.Drink(60);
		ctx.True(Math.AbsFloat(tired.GetStaminaDrainMultiplier(flagship) - 1) < 0.001, string.Format("eating and drinking lift the stamina penalty (got %1)", tired.GetStaminaDrainMultiplier(flagship)));

		// Health recovers to full once needs are met again.
		EL_SurvivalStats recovering = EL_SurvivalStats.Create("light-recovering");
		recovering.SetHealth(flagship.m_fHealthCap);
		recovering.UpdateStats(100, flagship);
		ctx.True(Math.AbsFloat(recovering.GetHealth() - (flagship.m_fHealthCap + 100 * flagship.m_fHealthRegenPerSecond)) < 0.001, string.Format("met needs regenerate health (got %1)", recovering.GetHealth()));

		// Config overrides apply: drain rates, cap, stamina multiplier, low threshold.
		LF_SurvivalConfig custom = new LF_SurvivalConfig();
		custom.m_fHungerDrainPerSecond = 1;
		custom.m_fThirstDrainPerSecond = 2;
		custom.m_fHealthCap = 50;
		custom.m_fHealthDrainPerSecond = 1;
		custom.m_fStaminaDrainMultiplier = 3;
		custom.m_fLowNeedThreshold = 99.5;

		EL_SurvivalStats tuned = EL_SurvivalStats.Create("light-tuned");
		tuned.UpdateStats(1, custom);
		ctx.True(Math.AbsFloat(tuned.GetHunger() - 99) < 0.001, string.Format("hunger drain comes from config (got %1)", tuned.GetHunger()));
		ctx.True(Math.AbsFloat(tuned.GetThirst() - 98) < 0.001, string.Format("thirst drain comes from config (got %1)", tuned.GetThirst()));
		ctx.True(tuned.AreNeedsLow(custom), "the low-needs threshold comes from config");
		ctx.True(Math.AbsFloat(tuned.GetStaminaDrainMultiplier(custom) - 3) < 0.001, "the stamina multiplier comes from config");
		for (int j = 0; j < 200; j++)
		{
			tuned.UpdateStats(1, custom);
		}
		ctx.True(Math.AbsFloat(tuned.GetHealth() - 50) < 0.001, string.Format("the health cap comes from config (got %1)", tuned.GetHealth()));

		// A negative delta (clock skew) changes nothing.
		EL_SurvivalStats skewed = EL_SurvivalStats.Create("light-skewed");
		skewed.SetHunger(50);
		skewed.SetHealth(80);
		skewed.UpdateStats(-100, flagship);
		ctx.True(Math.AbsFloat(skewed.GetHunger() - 50) < 0.001 && Math.AbsFloat(skewed.GetHealth() - 80) < 0.001, "a negative delta is a no-op");
	}
};
