// red-proof: observed 2026-11-17 with `tools\cli test --no-build --only LF_Test_DownedLogRate` on the unfixed behaviour: `20 absorbed hits within a second log one line (expected 1, got 20)` and `a revive queues no delayed log calls (expected 0, got 3)`; fixed, green.

// tier: WORLD
//! A downed body that keeps taking damage logs one line per few seconds with the count of hits
//! absorbed since, not one line per hit, and a revive queues no delayed log calls (#2582).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_DownedLogRate : LF_Test
{
	override string GetName()
	{
		return "medic/downed-log-rate";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		bool wasEnabled = config.m_bDownedEnabled;
		config.m_bDownedEnabled = true;
		LF_Downed.ResetStatics();
		LF_Downed.SetTestAllEligible(true);

		IEntity entity = LF_TestCharacterFixture.Spawn(ctx);
		if (entity)
		{
			Check(ctx, entity);
			LF_TestCharacterFixture.Delete(entity);
		}

		config.m_bDownedEnabled = wasEnabled;
		LF_Downed.SetTestAllEligible(false);
		LF_Downed.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(entity.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!character || !damage)
		{
			ctx.Fail("character lacks SCR_ChimeraCharacter or the character damage manager");
			return;
		}

		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(character.LF_IsDowned(), "the lethal hit downs the character");
		ctx.Equal(0, LF_Downed.GetAbsorbedCount(), "nothing absorbed yet");

		int i;
		for (i = 0; i < 20; i++)
		{
			damage.Kill(Instigator.CreateInstigator(null));
		}

		ctx.Equal(20, LF_Downed.GetAbsorbedCount(), "every absorbed hit is still counted");
		ctx.Equal(1, LF_Downed.GetAbsorbLogLines(), "20 absorbed hits within a second log one line");

		ctx.True(LF_Downed.Clear(entity), "Clear ends the downed state");
		ctx.Equal(0, LF_Downed.GetDelayedLogCalls(), "a revive queues no delayed log calls");

		// The #427 follow-up lines are a debug switch; on, a revive queues them.
		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(character.LF_IsDowned(), "downed again");
		LF_Downed.SetWakeTrace(true);
		ctx.True(LF_Downed.Clear(entity), "Clear ends the second downed state");
		ctx.Equal(3, LF_Downed.GetDelayedLogCalls(), "with the wake trace on, a revive queues its three follow-up lines");
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(LF_Downed.LogWake);
	}
}
