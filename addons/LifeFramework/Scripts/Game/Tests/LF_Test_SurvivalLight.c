// red-proof: observed 2026-11-08 with LF_SurvivalHealth stubbed to the old drain-to-zero math and
// no give-back: FAIL survival/light "zero needs never push health below the cap (lowest 0, cap
// 0.75)", "needs never lower health that is already below the cap", "the give-back stops at what
// needs took".

//------------------------------------------------------------------------------------------------
//! Survival needs are light (docs/decisions.md, 2026-11-08). The cap math for the character's
//! real health: zero needs hold health at the config cap and never kill, an injury below the cap
//! is not worsened, met needs give back only what needs took, and low needs raise stamina drain
//! until the player eats and drinks. Every rate comes from LF_SurvivalConfig.
// tier: LOGIC
class LF_Test_SurvivalLight : LF_Test
{
	override string GetName()
	{
		return "survival/light";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_SurvivalConfig flagship = LF_SurvivalConfig.CreateDefault();
		float cap = flagship.m_fHealthCapScaled;
		float drain = flagship.m_fHealthDrainScaledPerSecond;

		// Zero needs for ten hours of one-second ticks: health stops at the cap and never reaches zero.
		float health = 1;
		float lowest = 1;
		for (int i = 0; i < 36000; i++)
		{
			health = LF_SurvivalHealth.ComputeCappedHealth(health, cap, drain, 1);
			if (health < lowest)
				lowest = health;
		}
		ctx.True(lowest > cap - 0.0001, string.Format("zero needs never push health below the cap (lowest %1, cap %2)", lowest, cap));
		ctx.True(Math.AbsFloat(health - cap) < 0.0001, string.Format("zero needs settle health at the cap (got %1)", health));

		// One huge step lands exactly on the cap, not past it.
		ctx.True(Math.AbsFloat(LF_SurvivalHealth.ComputeCappedHealth(1, cap, drain, 100000) - cap) < 0.0001, "a huge step stops at the cap");

		// Health already below the cap (an injury) is not reduced further by needs.
		ctx.True(Math.AbsFloat(LF_SurvivalHealth.ComputeCappedHealth(0.4, cap, drain, 600) - 0.4) < 0.0001, "needs never lower health that is already below the cap");

		// Met needs give back the deficit at the restore rate, never more than was taken, never past full.
		ctx.True(Math.AbsFloat(LF_SurvivalHealth.ComputeRestore(0.75, 0.25, 0.001, 10) - 0.01) < 0.0001, "met needs give back health at the restore rate");
		ctx.True(Math.AbsFloat(LF_SurvivalHealth.ComputeRestore(0.75, 0.25, 0.001, 100000) - 0.25) < 0.0001, "the give-back stops at what needs took");
		ctx.True(Math.AbsFloat(LF_SurvivalHealth.ComputeRestore(0.9, 0.25, 0.001, 100000) - 0.1) < 0.0001, "the give-back never goes past full health");
		ctx.True(Math.AbsFloat(LF_SurvivalHealth.ComputeRestore(0.75, 0, 0.001, 100)) < 0.0001, "no deficit, no give-back");

		// Flagship drain is slow: an hour after a full meal the player is not hungry yet.
		LF_SurvivalStats fed = LF_SurvivalStats.Create("light-fed");
		fed.UpdateStats(3600, flagship);
		ctx.False(fed.AreNeedsLow(flagship), string.Format("an hour after a full meal needs are not low (hunger %1, thirst %2)", fed.GetHunger(), fed.GetThirst()));

		// Stamina penalty applies at low needs and lifts after eating and drinking.
		LF_SurvivalStats tired = LF_SurvivalStats.Create("light-tired");
		tired.SetHunger(5);
		tired.SetThirst(5);
		ctx.True(Math.AbsFloat(tired.GetStaminaDrainMultiplier(flagship) - flagship.m_fStaminaDrainMultiplier) < 0.001, string.Format("low needs raise stamina drain (got %1)", tired.GetStaminaDrainMultiplier(flagship)));
		tired.Eat(60);
		ctx.True(tired.GetStaminaDrainMultiplier(flagship) > 1.001, "eating alone does not lift the penalty while thirst is low");
		tired.Drink(60);
		ctx.True(Math.AbsFloat(tired.GetStaminaDrainMultiplier(flagship) - 1) < 0.001, string.Format("eating and drinking lift the stamina penalty (got %1)", tired.GetStaminaDrainMultiplier(flagship)));

		// Config overrides apply: drain rates, cap, stamina multiplier, low threshold.
		LF_SurvivalConfig custom = LF_SurvivalConfig.CreateDefault();
		custom.m_fHungerDrainPerSecond = 1;
		custom.m_fThirstDrainPerSecond = 2;
		custom.m_fHealthCapScaled = 0.5;
		custom.m_fStaminaDrainMultiplier = 3;
		custom.m_fLowNeedThreshold = 99.5;

		LF_SurvivalStats tuned = LF_SurvivalStats.Create("light-tuned");
		tuned.UpdateStats(1, custom);
		ctx.True(Math.AbsFloat(tuned.GetHunger() - 99) < 0.001, string.Format("hunger drain comes from config (got %1)", tuned.GetHunger()));
		ctx.True(Math.AbsFloat(tuned.GetThirst() - 98) < 0.001, string.Format("thirst drain comes from config (got %1)", tuned.GetThirst()));
		ctx.True(tuned.AreNeedsLow(custom), "the low-needs threshold comes from config");
		ctx.True(Math.AbsFloat(tuned.GetStaminaDrainMultiplier(custom) - 3) < 0.001, "the stamina multiplier comes from config");
		ctx.True(Math.AbsFloat(LF_SurvivalHealth.ComputeCappedHealth(1, custom.m_fHealthCapScaled, drain, 100000) - 0.5) < 0.0001, "the health cap comes from config");

		// A negative delta (clock skew) changes nothing.
		LF_SurvivalStats skewed = LF_SurvivalStats.Create("light-skewed");
		skewed.SetHunger(50);
		skewed.UpdateStats(-100, flagship);
		ctx.True(Math.AbsFloat(skewed.GetHunger() - 50) < 0.001, "a negative delta is a no-op on needs");
		ctx.True(Math.AbsFloat(LF_SurvivalHealth.ComputeCappedHealth(0.9, cap, drain, -100) - 0.9) < 0.0001, "a negative delta is a no-op on health");
	}
};
