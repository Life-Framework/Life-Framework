// red-proof: RED-OK 2026-11-15 `toolsi test --no-build --red LF_Test_Knockout` (hook skips Begin): "the character is knocked out", "the owning machine holds the body in ragdoll", "one ragdoll was released" failed; green without the hook.
// tier: WORLD
//! A knockout (#1857, the taser's down state): the body is ragdolled on the machine that owns it
//! and held down for the knockout's time, the wake releases the ragdoll and keeps the character
//! unconscious while it settles, then vanilla wakes it. A dead, a medically downed and a vanilla
//! unconscious character are refused and left exactly as they were; ending a knockout never
//! revives anyone. Test characters are server-owned and have no player, so the test opts them in.
class LF_Test_Knockout : LF_Test
{
	override string GetName()
	{
		return "medic/knockout";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(1, LF_Knockout.DecideRagdoll(true, false), "a knockout starts the ragdoll");
		ctx.Equal(0, LF_Knockout.DecideRagdoll(true, true), "a held ragdoll is not started twice");
		ctx.Equal(-1, LF_Knockout.DecideRagdoll(false, true), "the end of a knockout releases the ragdoll");
		ctx.Equal(0, LF_Knockout.DecideRagdoll(false, false), "no knockout, no ragdoll");
		ctx.True(LF_Knockout.Allows(false, false, false, true), "a living, conscious character can be knocked out");
		ctx.False(LF_Knockout.Allows(true, false, false, true), "a dead character cannot be knocked out");
		ctx.False(LF_Knockout.Allows(false, true, false, true), "a downed character cannot be knocked out");
		ctx.False(LF_Knockout.Allows(false, false, true, true), "an unconscious character cannot be knocked out");
		ctx.False(LF_Knockout.Allows(false, false, false, false), "no knockout where unconsciousness is not permitted");

		LF_MedicConfig config = LF_MedicConfig.Get();
		bool wasEnabled = config.m_bDownedEnabled;
		config.m_bDownedEnabled = true;
		LF_Downed.SetTestAllEligible(true);
		LF_Knockout.ResetStatics();

		RunKnockout(ctx);
		RunDead(ctx);
		RunMedicallyDowned(ctx);
		RunVanillaUnconscious(ctx);

		config.m_bDownedEnabled = wasEnabled;
		LF_Downed.SetTestAllEligible(false);
		LF_Knockout.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunKnockout(LF_TestContext ctx)
	{
		SCR_ChimeraCharacter character;
		SCR_CharacterDamageManagerComponent damage;
		if (!SpawnPair(ctx, character, damage))
			return;

		float seconds = 7;
		if (!RedProof())
			LF_Knockout.Begin(character, seconds, "", "test");

		ctx.True(LF_Stun.IsStunned(character), "the character is knocked out");
		ctx.True(damage.LF_WantsUnconscious(), "a knocked-out character is held unconscious");
		ctx.InRange(LF_Knockout.GetRemainingSeconds(character), seconds - 0.5, seconds, "the knockout lasts its configured time");
		ctx.True(character.LF_IsKnockoutRagdollHeld(), "the owning machine holds the body in ragdoll");
		ctx.Equal(1, LF_Knockout.GetRagdollStarts(), "one ragdoll was started");

		LF_Knockout.Wake(character, character.LF_GetStunToken());
		ctx.False(LF_Stun.IsStunned(character), "the timed wake ends the knockout");
		ctx.False(character.LF_IsKnockoutRagdollHeld(), "the wake releases the ragdoll");
		ctx.Equal(1, LF_Knockout.GetRagdollReleases(), "one ragdoll was released");
		ctx.True(character.LF_IsKnockoutSettling(), "the released body settles before the wake-up");
		ctx.True(damage.LF_WantsUnconscious(), "a settling body is still unconscious, so it does not snap upright");

		LF_Knockout.Settle(character);
		ctx.False(character.LF_IsKnockoutSettling(), "the body has settled");
		ctx.False(damage.LF_WantsUnconscious(), "the settled character wakes through vanilla");
		ctx.Equal(0, LF_Knockout.GetRemainingSeconds(character), "no knockout time left");

		LF_TestCharacterFixture.Delete(character);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDead(LF_TestContext ctx)
	{
		SCR_ChimeraCharacter character;
		SCR_CharacterDamageManagerComponent damage;
		if (!SpawnPair(ctx, character, damage))
			return;

		LF_Downed.Finish(character, Instigator.CreateInstigator(null));
		ctx.True(damage.GetState() == EDamageState.DESTROYED, "the character is dead");
		int starts = LF_Knockout.GetRagdollStarts();
		ctx.False(LF_Knockout.Begin(character, 5, "", "test"), "a dead character is not knocked out");
		ctx.False(character.LF_IsDowned(), "the dead character is not put in the downed state");
		ctx.False(LF_Knockout.End(character), "ending a knockout on a dead character does nothing");
		ctx.True(damage.GetState() == EDamageState.DESTROYED, "the dead character stays dead");
		ctx.Equal(starts, LF_Knockout.GetRagdollStarts(), "no knockout ragdoll on a dead body");

		LF_TestCharacterFixture.Delete(character);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunMedicallyDowned(LF_TestContext ctx)
	{
		SCR_ChimeraCharacter character;
		SCR_CharacterDamageManagerComponent damage;
		if (!SpawnPair(ctx, character, damage))
			return;

		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(character.LF_IsDowned(), "lethal damage downs the character");
		float health = damage.GetDefaultHitZone().GetHealthScaled();
		ctx.False(LF_Knockout.Begin(character, 5, "", "test"), "a medically downed character is not knocked out");
		ctx.False(LF_Stun.IsStunned(character), "the medical down is not turned into a knockout");
		ctx.False(LF_Knockout.End(character), "ending a knockout does not revive a medically downed character");
		ctx.True(character.LF_IsDowned(), "the character is still downed");
		ctx.True(damage.LF_WantsUnconscious(), "the character is still unconscious");
		ctx.True(character.LF_GetBleedSecondsLeft() > 0, "the bleed-out timer still runs");
		ctx.EqualFloat(health, damage.GetDefaultHitZone().GetHealthScaled(), 0.001, "the downed character's health is untouched");
		ctx.False(character.LF_IsKnockoutRagdollHeld(), "no knockout ragdoll on a medical down");

		LF_Downed.Finish(character, Instigator.CreateInstigator(null));
		LF_TestCharacterFixture.Delete(character);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunVanillaUnconscious(LF_TestContext ctx)
	{
		SCR_ChimeraCharacter character;
		SCR_CharacterDamageManagerComponent damage;
		if (!SpawnPair(ctx, character, damage))
			return;

		damage.ForceUnconsciousness(0);
		ctx.True(damage.LF_WantsUnconscious(), "vanilla resilience damage knocks the character out");
		ctx.False(LF_Knockout.Begin(character, 5, "", "test"), "a vanilla-unconscious character is not knocked out");
		ctx.False(character.LF_IsDowned(), "the vanilla unconsciousness is not turned into a downed state");
		ctx.True(damage.LF_WantsUnconscious(), "the character stays unconscious");

		LF_TestCharacterFixture.Delete(character);
	}

	//------------------------------------------------------------------------------------------------
	protected bool SpawnPair(LF_TestContext ctx, out SCR_ChimeraCharacter character, out SCR_CharacterDamageManagerComponent damage)
	{
		IEntity entity = LF_TestCharacterFixture.Spawn(ctx);
		if (!entity)
			return false;

		character = SCR_ChimeraCharacter.Cast(entity);
		damage = SCR_CharacterDamageManagerComponent.Cast(entity.FindComponent(SCR_CharacterDamageManagerComponent));
		if (character && damage)
			return true;

		ctx.Fail("character lacks SCR_ChimeraCharacter or the character damage manager");
		LF_TestCharacterFixture.Delete(entity);
		return false;
	}
}
