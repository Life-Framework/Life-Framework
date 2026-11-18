// red-proof: 2026-11-18 (#3359): on main (corpse tick still running) `--only LF_Test_SurvivalCorpseNoDrain` failed `the shared hunger drops by one delta, not two`.

//------------------------------------------------------------------------------------------------
//! A respawned character shares one stats object with its corpse (same character id). The corpse's
//! tick must stop at death, or the shared hunger and thirst drain twice as fast until cleanup.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SurvivalCorpseNoDrain : LF_Test
{
	override string GetName()
	{
		return "survival/corpse-no-drain";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity bodyA = LF_TestCharacterFixture.Spawn(ctx);
		if (!bodyA)
			return;

		IEntity bodyB = LF_TestCharacterFixture.Spawn(ctx);
		if (!bodyB)
		{
			LF_TestCharacterFixture.Delete(bodyA);
			return;
		}

		LF_CharacterSurvivalComponent survivalA = LF_CharacterSurvivalComponent.Cast(bodyA.FindComponent(LF_CharacterSurvivalComponent));
		LF_CharacterSurvivalComponent survivalB = LF_CharacterSurvivalComponent.Cast(bodyB.FindComponent(LF_CharacterSurvivalComponent));
		SCR_CharacterDamageManagerComponent damageA = SCR_CharacterDamageManagerComponent.Cast(bodyA.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!survivalA || !survivalB || !damageA)
		{
			ctx.Fail("character prefab lacks the survival component or the character damage manager");
			LF_TestCharacterFixture.Delete(bodyA);
			LF_TestCharacterFixture.Delete(bodyB);
			return;
		}

		string id = "survival-corpse-no-drain";
		survivalA.Init(id);
		damageA.Kill(Instigator.CreateInstigator(null));
		ctx.True(damageA.IsDestroyed(), "the first body is dead");

		survivalB.Init(id);
		LF_SurvivalStats stats = survivalB.GetSurvivalStats();
		ctx.True(stats == survivalA.GetSurvivalStats(), "both bodies share one stats object");
		if (!stats)
		{
			LF_TestCharacterFixture.Delete(bodyA);
			LF_TestCharacterFixture.Delete(bodyB);
			return;
		}

		LF_SurvivalConfig config = LF_SurvivalConfig.Get();
		float deltaSeconds = Math.Min(10, config.m_fMaxTickDeltaSeconds);
		stats.SetHunger(100);
		stats.SetThirst(100);

		// Both bodies were last ticked at the same world time; tick each at that time plus one delta.
		float nowMs = GetGame().GetWorld().GetWorldTime() + deltaSeconds * 1000;
		survivalA.TickAt(nowMs);
		survivalB.TickAt(nowMs);

		ctx.EqualFloat(100 - deltaSeconds * config.m_fHungerDrainPerSecond, stats.GetHunger(), 0.01, "the shared hunger drops by one delta, not two");
		ctx.EqualFloat(100 - deltaSeconds * config.m_fThirstDrainPerSecond, stats.GetThirst(), 0.01, "the shared thirst drops by one delta, not two");

		LF_TestCharacterFixture.Delete(bodyA);
		LF_TestCharacterFixture.Delete(bodyB);
	}
};
