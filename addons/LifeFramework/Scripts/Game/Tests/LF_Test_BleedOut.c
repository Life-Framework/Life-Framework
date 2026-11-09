// red-proof: observed 2026-11-09 with `tools\cli test --no-build --only LF_Test_BleedOut`: LF_BleedOut.IsExpired using `left < 0` failed `running out of time kills for real` and `one expiry (expected 1, got 0)`; restored, green.

// tier: WORLD
//! A downed character bleeds out through LF_BleedOut: giving up is refused before the delay and
//! accepted after it, running out of time kills for real and exactly once, a revive stops the
//! clock, and the replicated copy of the time left moves in steps. Test characters have no
//! player behind them, so the test opts them in as eligible.
class LF_Test_BleedOut : LF_Test
{
	override string GetName()
	{
		return "medic/bleed-out";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		bool wasEnabled = config.m_bDownedEnabled;
		float wasBleed = config.m_fBleedOutSeconds;
		float wasDelay = config.m_fGiveUpDelaySeconds;
		config.m_bDownedEnabled = true;
		config.m_fBleedOutSeconds = 100;
		config.m_fGiveUpDelaySeconds = 30;
		LF_Downed.ResetStatics();
		LF_Downed.SetTestAllEligible(true);
		LF_BleedOut.ResetStatics();

		RunExpiry(ctx);
		RunGiveUp(ctx);
		RunRevive(ctx);

		config.m_bDownedEnabled = wasEnabled;
		config.m_fBleedOutSeconds = wasBleed;
		config.m_fGiveUpDelaySeconds = wasDelay;
		LF_Downed.ResetStatics();
		LF_BleedOut.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_ChimeraCharacter SpawnDowned(LF_TestContext ctx, out IEntity outEntity)
	{
		outEntity = LF_TestCharacterFixture.Spawn(ctx);
		if (!outEntity)
			return null;

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(outEntity.FindComponent(SCR_CharacterDamageManagerComponent));
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(outEntity);
		if (!damage || !character)
		{
			ctx.Fail("character lacks SCR_ChimeraCharacter or the character damage manager");
			LF_TestCharacterFixture.Delete(outEntity);
			return null;
		}

		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(character.LF_IsDowned(), "the lethal hit downs the character");
		return character;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunExpiry(LF_TestContext ctx)
	{
		IEntity entity;
		SCR_ChimeraCharacter character = SpawnDowned(ctx, entity);
		if (!character)
			return;

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(entity.FindComponent(SCR_CharacterDamageManagerComponent));
		ctx.EqualFloat(100, character.LF_GetBleedLeft(), 0.01, "the clock starts at the configured seconds");
		ctx.Equal(100, character.LF_GetBleedSecondsLeft(), "the replicated copy starts there too");

		// Throttle: three seconds of bleeding do not move the replicated copy.
		LF_BleedOut.Advance(3);
		ctx.EqualFloat(97, character.LF_GetBleedLeft(), 0.01, "the server clock counts down");
		ctx.Equal(100, character.LF_GetBleedSecondsLeft(), "the replicated copy is not bumped for 3 seconds");
		LF_BleedOut.Advance(3);
		ctx.Equal(94, character.LF_GetBleedSecondsLeft(), "the replicated copy catches up once the drift passes the step");
		ctx.True(character.LF_IsDowned(), "still downed while time is left");

		LF_BleedOut.Advance(1000);
		ctx.True(damage.GetState() == EDamageState.DESTROYED, "running out of time kills for real");
		ctx.False(character.LF_IsDowned(), "the downed flag is gone with the death");
		ctx.Equal(-1, character.LF_GetBleedSecondsLeft(), "the replicated clock is cleared");
		ctx.Equal(1, LF_BleedOut.GetExpiredCount(), "one expiry");

		LF_BleedOut.Advance(1000);
		ctx.Equal(1, LF_BleedOut.GetExpiredCount(), "a dead character does not expire again");

		LF_TestCharacterFixture.Delete(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunGiveUp(LF_TestContext ctx)
	{
		IEntity entity;
		SCR_ChimeraCharacter character = SpawnDowned(ctx, entity);
		if (!character)
			return;

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(entity.FindComponent(SCR_CharacterDamageManagerComponent));
		ctx.False(LF_BleedOut.GiveUp(entity), "give-up is refused right after going down");
		ctx.True(character.LF_IsDowned(), "a refused give-up leaves the player downed");
		ctx.True(damage.GetState() != EDamageState.DESTROYED, "a refused give-up does not kill");

		LF_BleedOut.Advance(31);
		ctx.True(LF_BleedOut.GiveUp(entity), "give-up is accepted after the delay");
		ctx.True(damage.GetState() == EDamageState.DESTROYED, "giving up kills for real");
		ctx.Equal(1, LF_BleedOut.GetGaveUpCount(), "one give-up");
		ctx.False(LF_BleedOut.GiveUp(entity), "a dead character cannot give up again");

		LF_TestCharacterFixture.Delete(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRevive(LF_TestContext ctx)
	{
		IEntity entity;
		SCR_ChimeraCharacter character = SpawnDowned(ctx, entity);
		if (!character)
			return;

		LF_BleedOut.Advance(10);
		ctx.True(LF_Downed.Clear(entity), "Clear ends the downed state");
		ctx.Equal(-1, character.LF_GetBleedSecondsLeft(), "a revived player has no clock");
		int expiredBefore = LF_BleedOut.GetExpiredCount();
		LF_BleedOut.Advance(1000);
		ctx.Equal(expiredBefore, LF_BleedOut.GetExpiredCount(), "a revived player does not bleed out");

		LF_TestCharacterFixture.Delete(entity);
	}
}
