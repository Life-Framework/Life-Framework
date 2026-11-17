//------------------------------------------------------------------------------------------------
//! A knockout: a timed trip into the downed state that deals no damage and ends on its own (the
//! taser's stun, LF_Stun; later a punch). The server owns the decision and the timer; the body
//! falls, lies limp and gets up through vanilla's own machinery:
//! - the downed flag (LF_Downed) holds the character in vanilla unconsciousness (INCAPACITATED:
//!   no movement, weapons or items) and the engine replicates that life state, late joiners
//!   included;
//! - the knockout flag (SCR_ChimeraCharacter.m_bLF_Stunned, an RplProp) makes the machine that
//!   owns the character start vanilla's ragdoll (CharacterControllerComponent.Ragdoll, which only
//!   the owner may call; the engine replicates the ragdoll to proxies) and hold it with
//!   RefreshRagdoll for as long as the flag is set. Vanilla unconsciousness only ragdolls from a
//!   damage hit's effectors, and a knockout deals no damage, so without this the body went
//!   INCAPACITATED standing and woke standing (#1857);
//! - when the flag clears, the owner releases the ragdoll (RefreshRagdoll(0): it blends into the
//!   prone pose), keeps the character unconscious for SETTLE_MS while it settles, then lets
//!   vanilla's wake-up animation stand it up from the ground.
//! A knockout never starts on a dead, medically downed or otherwise unconscious character, and
//! its end never wakes one (ShouldBeUnconscious still asks vanilla and the downed flag).
//------------------------------------------------------------------------------------------------
class LF_Knockout
{
	static const string FEATURE = "Medic";

	//! RefreshRagdoll's "endless" hold, from the engine's own doc; the flag's clear ends it.
	// lf-fixed: an engine idiom, not tuning: the release comes from the replicated flag
	static const float HOLD_SECONDS = 3600;

	//! Time the released ragdoll gets to blend into the prone pose before the wake-up animation.
	// lf-fixed: an animation blend time, not gameplay tuning; the knockout length is the caller's
	static const int SETTLE_MS = 1500;

	protected static int s_iCount;
	protected static int s_iRagdollStarts;
	protected static int s_iRagdollReleases;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iCount = 0;
		s_iRagdollStarts = 0;
		s_iRagdollReleases = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure rule for the owner's ragdoll: 1 start and hold, -1 release, 0 nothing.
	static int DecideRagdoll(bool knockedOut, bool held)
	{
		if (knockedOut && !held)
			return 1;

		if (!knockedOut && held)
			return -1;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure rule: a knockout may start only on a living, conscious character that is not downed.
	static bool Allows(bool dead, bool downed, bool unconscious, bool unconsciousnessPermitted)
	{
		return !dead && !downed && !unconscious && unconsciousnessPermitted;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: may this character be knocked out now? Logs nothing.
	static bool CanKnockOut(notnull SCR_ChimeraCharacter victim)
	{
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(victim.FindComponent(SCR_CharacterDamageManagerComponent));
		CharacterControllerComponent controller = victim.GetCharacterController();
		if (!damageManager || !controller)
			return false;

		bool dead = damageManager.GetState() == EDamageState.DESTROYED || controller.GetLifeState() == ECharacterLifeState.DEAD;
		bool unconscious = controller.GetLifeState() == ECharacterLifeState.INCAPACITATED || damageManager.LF_WantsUnconscious();
		if (!Allows(dead, victim.LF_IsDowned(), unconscious, damageManager.GetPermitUnconsciousness()))
			return false;

		return LF_Downed.Enabled() && LF_Downed.IsEligible(victim);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: knock a character out for `seconds`. \return False (and logged) when refused.
	static bool Begin(notnull SCR_ChimeraCharacter victim, float seconds, string byUid, string reason)
	{
		if (!Replication.IsServer())
			return false;

		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(victim.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damageManager || seconds <= 0 || !CanKnockOut(victim))
		{
			LF_Debug.Info(FEATURE, string.Format("%1 knockout of %2 refused (downed %3, seconds %4)", reason, Uid(victim), victim.LF_IsDowned(), seconds));
			return false;
		}

		int token = victim.LF_BeginStun(seconds);
		damageManager.UpdateConsciousness();
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(Wake, seconds * 1000, false, victim, token);

		s_iCount++;
		LF_Debug.Info(FEATURE, string.Format("%1: player %2 is knocked out for %3 s (by %4)", reason, Uid(victim), seconds, byUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the knockout's time is over. Does nothing when that knockout already ended (a revive,
	//! a death) or a later one replaced it.
	static void Wake(SCR_ChimeraCharacter victim, int token)
	{
		if (!victim || !victim.LF_IsStunned() || victim.LF_GetStunToken() != token)
			return;

		End(victim);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: end a knockout now. The character wakes with the health it had, unless something
	//! else (a wound, vanilla resilience) still keeps it unconscious.
	static bool End(notnull SCR_ChimeraCharacter victim)
	{
		if (!Replication.IsServer() || !victim.LF_IsStunned())
			return false;

		victim.LF_SetDowned(false, "");
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(victim.FindComponent(SCR_CharacterDamageManagerComponent));
		if (damageManager)
			damageManager.UpdateConsciousness();

		LF_Debug.Info(FEATURE, string.Format("player %1 is no longer knocked out", Uid(victim)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: seconds left on a character's knockout; 0 when there is none.
	static float GetRemainingSeconds(notnull SCR_ChimeraCharacter victim)
	{
		if (!victim.LF_IsStunned() || !GetGame().GetWorld())
			return 0;

		float left = victim.LF_GetStunEndsAt() - GetGame().GetWorld().GetWorldTime() / 1000;
		return Math.Max(0, left);
	}

	//------------------------------------------------------------------------------------------------
	//! Bring the ragdoll in line with the knockout flag on the machine that owns the character (the
	//! owning client from the replication callback; the server for a server-owned body). Proxies do
	//! nothing: the engine replicates the owner's ragdoll to them.
	static void Sync(notnull SCR_ChimeraCharacter character)
	{
		RplComponent rpl = RplComponent.Cast(character.FindComponent(RplComponent));
		if (rpl && !rpl.IsOwner())
			return;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller)
			return;

		// Both flags, not the stun flag alone: the two props of one update reach a client through two
		// callbacks in either order, and the wake must see the release before it sees "not downed".
		int decision = DecideRagdoll(character.LF_IsStunned(), character.LF_IsKnockoutRagdollHeld());
		if (decision > 0)
		{
			character.LF_SetKnockoutRagdoll(true, false);
			controller.Ragdoll();
			controller.RefreshRagdoll(HOLD_SECONDS);
			s_iRagdollStarts++;
			LF_Debug.Info(FEATURE, string.Format("knockout ragdoll on %1 starts", Uid(character)));
			return;
		}

		if (decision < 0)
		{
			character.LF_SetKnockoutRagdoll(false, true);
			controller.RefreshRagdoll(0);
			s_iRagdollReleases++;
			if (GetGame().GetCallqueue())
				GetGame().GetCallqueue().CallLater(Settle, SETTLE_MS, false, character);

			LF_Debug.Info(FEATURE, string.Format("knockout ragdoll on %1 released; wake-up in %2 ms", Uid(character), SETTLE_MS));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Owner: the released body has settled; vanilla decides whether it wakes (the wake-up animation).
	static void Settle(SCR_ChimeraCharacter character)
	{
		if (!character || !character.LF_IsKnockoutSettling())
			return;

		character.LF_SetKnockoutRagdoll(false, false);
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(character.FindComponent(SCR_CharacterDamageManagerComponent));
		CharacterControllerComponent controller = character.GetCharacterController();
		if (!damageManager || !controller || controller.GetLifeState() == ECharacterLifeState.DEAD)
			return;

		damageManager.UpdateConsciousness();
		LF_Debug.Info(FEATURE, string.Format("knockout of %1 settled: wants unconscious %2", Uid(character), damageManager.LF_WantsUnconscious()));
	}

	//------------------------------------------------------------------------------------------------
	static int GetCount()
	{
		return s_iCount;
	}

	//------------------------------------------------------------------------------------------------
	static int GetRagdollStarts()
	{
		return s_iRagdollStarts;
	}

	//------------------------------------------------------------------------------------------------
	static int GetRagdollReleases()
	{
		return s_iRagdollReleases;
	}

	//------------------------------------------------------------------------------------------------
	//! Who to name in the log: the player's uid, the owner of a logged-out body, else "npc".
	static string Uid(IEntity character)
	{
		string uid = LF_Utils.GetPlayerUID(character);
		if (uid.IsEmpty())
		{
			string owner = LF_LogoutBody.UidOfBody(character);
			if (!owner.IsEmpty())
				return owner + " (logged out)";

			return "npc";
		}

		return uid;
	}
}
