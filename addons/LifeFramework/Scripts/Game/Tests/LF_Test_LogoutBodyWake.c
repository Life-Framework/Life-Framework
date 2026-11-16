// red-proof: RED-OK 2026-11-17 `tools\cli test --no-build --red LF_Test_LogoutBodyWake` (hook takes the body back the pre-#2156 way, End without the hold): "the take-back releases the logout ragdoll (expected 1, got 0)" and "the released body settles before it gets up" failed; green with `--only`.
// tier: WORLD
//! Acceptance of #2156: a rejoiner who takes back their logout body gets up. The logout knockout
//! was started and held by the leaving client (it still owned the body), so the server has no hold
//! of its own; the take-back must still release the ragdoll, let it settle and wake the body
//! through vanilla. A body that was downed for real before the logout stays down. The new owner's
//! wake on control (WakeOnControl) is a pure rule here; the two-client rejoin is a session proof.
class LF_Test_LogoutBodyWake : LF_Test
{
	protected static const string UID = "logout-wake-uid";
	protected static const string CHARACTER_ID = "logout-wake-character";

	override string GetName()
	{
		return "rejoin/logout-body-wake";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_LogoutBodyRejoin.ShouldWakeOnControl(false, false, false, true, false), "an unconscious body nothing keeps down wakes for its new owner");
		ctx.True(LF_LogoutBodyRejoin.ShouldWakeOnControl(false, false, false, false, true), "a ragdolled body nothing keeps down is released");
		ctx.False(LF_LogoutBodyRejoin.ShouldWakeOnControl(false, false, false, false, false), "a standing body is left alone");
		ctx.False(LF_LogoutBodyRejoin.ShouldWakeOnControl(false, true, false, true, true), "a downed or knocked-out body stays down");
		ctx.False(LF_LogoutBodyRejoin.ShouldWakeOnControl(false, false, true, true, false), "a body vanilla keeps unconscious stays down");
		ctx.False(LF_LogoutBodyRejoin.ShouldWakeOnControl(true, false, false, true, true), "a dead body stays dead");

		LF_MedicConfig config = LF_MedicConfig.Get();
		bool wasEnabled = config.m_bDownedEnabled;
		config.m_bDownedEnabled = true;
		LF_Downed.SetTestAllEligible(true);
		LF_LogoutBody.ResetStatics();
		LF_HospitalRespawn.Reset();

		RunLogoutKnockout(ctx);
		RunDownedBeforeLogout(ctx);

		LF_LogoutBody.ResetStatics();
		LF_StorageSnapshot.Clear(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		config.m_bDownedEnabled = wasEnabled;
		LF_Downed.SetTestAllEligible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLogoutKnockout(LF_TestContext ctx)
	{
		SCR_ChimeraCharacter character;
		SCR_CharacterDamageManagerComponent damage;
		if (!SpawnPair(ctx, character, damage))
			return;

		ctx.True(LF_Knockout.Begin(character, 300, "", "logout"), "the logout lays the body down");
		// The leaving client held the ragdoll; the server, owner from here on, has no hold of its own.
		character.LF_SetKnockoutRagdoll(false, false);
		LF_LogoutBody.KeepBody(UID, CHARACTER_ID, character, LF_LogoutBody.Now() + 300);
		int releases = LF_Knockout.GetRagdollReleases();

		IEntity taken;
		if (!RedProof())
		{
			taken = LF_LogoutBodyRejoin.TakeBack(UID);
		}
		else
		{
			// The take-back before #2156: the knockout ends with no ragdoll release.
			taken = LF_LogoutBody.Detach(UID);
			LF_Knockout.End(character);
		}

		ctx.True(taken == character, "the rejoin takes the same body back");
		ctx.False(LF_Stun.IsStunned(character), "the logout knockout is over");
		ctx.Equal(releases + 1, LF_Knockout.GetRagdollReleases(), "the take-back releases the logout ragdoll");
		ctx.True(character.LF_IsKnockoutSettling(), "the released body settles before it gets up");

		LF_Knockout.Settle(character);
		ctx.False(character.LF_IsKnockoutSettling(), "the body has settled");
		ctx.False(damage.LF_WantsUnconscious(), "the taken-back body is conscious: vanilla stands it up");

		LF_TestCharacterFixture.Delete(character);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDownedBeforeLogout(LF_TestContext ctx)
	{
		SCR_ChimeraCharacter character;
		SCR_CharacterDamageManagerComponent damage;
		if (!SpawnPair(ctx, character, damage))
			return;

		damage.Kill(Instigator.CreateInstigator(null));
		ctx.True(character.LF_IsDowned(), "the body was downed before the logout");
		LF_LogoutBody.KeepBody(UID, CHARACTER_ID, character, LF_LogoutBody.Now() + 600);
		int releases = LF_Knockout.GetRagdollReleases();

		IEntity taken = LF_LogoutBodyRejoin.TakeBack(UID);
		ctx.True(taken == character, "the rejoin takes the downed body back");
		ctx.True(character.LF_IsDowned(), "a body downed for real stays downed");
		ctx.True(damage.LF_WantsUnconscious(), "a body downed for real stays unconscious");
		ctx.False(character.LF_IsKnockoutRagdollHeld() || character.LF_IsKnockoutSettling(), "no knockout hold is left on a real down");
		ctx.Equal(releases, LF_Knockout.GetRagdollReleases(), "no knockout ragdoll is released on a real down");
		ctx.False(LF_LogoutBodyRejoin.WakeOnControl(character), "its new owner does not wake it either");

		LF_Downed.Finish(character, Instigator.CreateInstigator(null));
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
