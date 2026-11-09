// red-proof: observed 2026-11-08 with needs not wired to the damage manager (the PR 136 state, a
// separate survival health stat): FAIL survival/real-health "zero needs cap real health at 0.75
// (got 1)".

//------------------------------------------------------------------------------------------------
//! A spawned character's real health (SCR_CharacterDamageManagerComponent) follows needs: zero
//! hunger and thirst hold it at the cap and the character stays alive, eating and drinking gives
//! back what needs took, and needs never pull down health that vanilla healing raised once fed.
// tier: WORLD
class LF_Test_SurvivalRealHealth : LF_Test
{
	override string GetName()
	{
		return "survival/real-health";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		LF_CharacterSurvivalComponent survival = LF_CharacterSurvivalComponent.Cast(character.FindComponent(LF_CharacterSurvivalComponent));
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(character.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!survival || !damageManager)
		{
			ctx.Fail("character prefab lacks the survival component or the character damage manager");
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		survival.Init("survival-real-health");
		LF_SurvivalStats stats = survival.GetSurvivalStats();
		float cap = LF_SurvivalConfig.Get().m_fHealthCapScaled;

		ctx.True(Math.AbsFloat(damageManager.GetHealthScaled() - 1) < 0.001, string.Format("a fresh body starts at full health (got %1)", damageManager.GetHealthScaled()));

		// Starve: far more time than the drain needs, in clamped-size steps like the real tick.
		stats.SetHunger(0);
		stats.SetThirst(0);
		for (int i = 0; i < 400; i++)
		{
			survival.ApplyNeedsToHealth(5);
		}
		ctx.True(Math.AbsFloat(damageManager.GetHealthScaled() - cap) < 0.001, string.Format("zero needs cap real health at %1 (got %2)", cap, damageManager.GetHealthScaled()));
		ctx.True(damageManager.GetState() != EDamageState.DESTROYED, "zero needs never kill the character");

		// Fed: needs give back what they took.
		survival.Eat(100);
		survival.Drink(100);
		for (int j = 0; j < 400; j++)
		{
			survival.ApplyNeedsToHealth(5);
		}
		ctx.True(Math.AbsFloat(damageManager.GetHealthScaled() - 1) < 0.001, string.Format("eating and drinking give back the health needs took (got %1)", damageManager.GetHealthScaled()));
		ctx.True(survival.GetHealthDeficitScaled() < 0.001, "the deficit is cleared once given back");

		// An injury below the cap is not worsened by low needs.
		damageManager.SetHealthScaled(0.5);
		stats.SetHunger(0);
		survival.ApplyNeedsToHealth(5);
		ctx.True(Math.AbsFloat(damageManager.GetHealthScaled() - 0.5) < 0.001, string.Format("low needs never lower health already below the cap (got %1)", damageManager.GetHealthScaled()));

		LF_TestCharacterFixture.Delete(character);
	}
};
