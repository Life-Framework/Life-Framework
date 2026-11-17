//------------------------------------------------------------------------------------------------
//! Rejoining inside the window puts the player back in their logout body (#1975, after #1973).
//!
//! The body (LF_LogoutBody) stands in the world for its window. When its player comes back, the
//! spawn (LF_SpawnLogic.CreateCharacter) asks TakeBack first:
//! - jail or hospital (LF_DisconnectRules.RejoinFor, #1972) wins: the body ends and the normal spawn
//!   restores the snapshot, which already holds what the body still carried;
//! - otherwise a live body is handed back as it is: wherever it lies, cuffed, downed or robbed;
//! - no body (the window closed, or it was killed) leaves the normal rejoin rule (#1778).
//! No dupes: the player gets the body OR the snapshot, never both. A taken-back body keeps its own
//! inventory and nothing is applied to it; an ended body is captured once more and deleted before
//! the snapshot goes on the new character.
//!
//! A body killed while its player was away takes the ordinary death path (LF_SpawnLogic.OnAccountDied_S):
//! the snapshot drops (the loot is on the corpse) and the rejoin wakes in hospital.
//------------------------------------------------------------------------------------------------
class LF_LogoutBodyRejoin
{
	static const string FEATURE = "LogoutBody";

	//------------------------------------------------------------------------------------------------
	//! Pure rule: does the rejoin take the body back? Jail and hospital come first.
	static bool Applies(bool hasLiveBody, bool serving, bool pendingWake)
	{
		return hasLiveBody && !serving && !pendingWake;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the spawn: uid's logout body when the rejoin takes it back, else null. When the
	//! jail or the hospital wins, the body ends here (captured, deleted) so the normal spawn restores
	//! exactly what it held.
	static IEntity TakeBack(string uid)
	{
		if (!Replication.IsServer() || uid.IsEmpty() || !LF_LogoutBody.HasBody(uid))
			return null;

		bool serving = LF_Jail.IsServing(uid);
		bool pendingWake = LF_HospitalRespawn.HasPendingWake(uid);
		if (!Applies(true, serving, pendingWake))
		{
			LF_Debug.Info(FEATURE, string.Format("%1 rejoins inside the window but %2 comes first: the body ends", uid, Rule(serving)));
			LF_LogoutBody.Release(uid, "rejoined to " + Rule(serving));
			return null;
		}

		IEntity body = LF_LogoutBody.Detach(uid);
		if (!body)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 rejoins: the body is gone or dead, normal rejoin", uid));
			return null;
		}

		// The logout knockout's job is done; a real downed state (a wound) stays.
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(body);
		if (character && character.LF_IsStunned())
			EndLogoutKnockout(character);

		LF_Debug.Info(FEATURE, string.Format("%1 rejoins inside the window: takes back the body at %2", uid, body.GetOrigin()));
		return body;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, which owns the body until the spawn hands it over: end the logout knockout and get the
	//! body up (#2156). The leaving client started and held the knockout ragdoll (LF_Knockout.Sync runs
	//! on the owner, and the player still owned the body when LF_LogoutBody laid it down), so the
	//! server had no hold of its own and a plain End released nothing: the body stayed ragdolled and
	//! unconscious. Taking the hold over first makes End release it, settle and wake through vanilla.
	static void EndLogoutKnockout(notnull SCR_ChimeraCharacter character)
	{
		character.LF_SetKnockoutRagdoll(true, false);
		LF_Knockout.End(character);
		// Not the owner (Sync did nothing): drop the borrowed hold; the owner wakes it on control.
		if (character.LF_IsKnockoutRagdollHeld())
			character.LF_SetKnockoutRagdoll(false, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure rule for the machine that just took control of a character: wake it when it lies
	//! unconscious or ragdolled with nothing left that keeps it down. A wound or a knockout (keptDown)
	//! or vanilla itself (wantsUnconscious) keeps it down.
	static bool ShouldWakeOnControl(bool dead, bool keptDown, bool wantsUnconscious, bool incapacitated, bool ragdolled)
	{
		if (dead || keptDown || wantsUnconscious)
			return false;

		return incapacitated || ragdolled;
	}

	//------------------------------------------------------------------------------------------------
	//! Owner, a moment after it took control. A client owner re-reads consciousness only on a damage
	//! state or downed-flag change (LF_OnDownedReplicated), and a taken-back body brings neither: the
	//! flag cleared while the server still owned it, so the new owner kept the body it inherited
	//! INCAPACITATED. Vanilla's own path: release the ragdoll, then UpdateConsciousness.
	static bool WakeOnControl(SCR_ChimeraCharacter character)
	{
		if (!character)
			return false;

		RplComponent rpl = RplComponent.Cast(character.FindComponent(RplComponent));
		if (rpl && !rpl.IsOwner())
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(character.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!controller || !damageManager)
			return false;

		bool ragdolled;
		CharacterAnimationComponent animation = character.GetAnimationComponent();
		if (animation)
			ragdolled = animation.IsRagdollActive();

		bool dead = controller.GetLifeState() == ECharacterLifeState.DEAD;
		bool keptDown = character.LF_IsDowned() || character.LF_HasStunFlag() || character.LF_IsKnockoutRagdollHeld() || character.LF_IsKnockoutSettling();
		bool incapacitated = controller.GetLifeState() == ECharacterLifeState.INCAPACITATED;
		if (!ShouldWakeOnControl(dead, keptDown, damageManager.LF_WantsUnconscious(), incapacitated, ragdolled))
			return false;

		if (ragdolled)
			controller.RefreshRagdoll(0);

		damageManager.UpdateConsciousness();
		// Vanilla wakes a character lying prone; the rejoiner stands once the wake-up has played.
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(StandUp, STAND_UP_MS, false, character, STAND_UP_TRIES);

		LF_Debug.Info(FEATURE, string.Format("%1 took control of a lying body (ragdoll %2, incapacitated %3): it gets up", LF_Utils.GetPlayerUID(character), ragdolled, incapacitated));
		return true;
	}

	//! Time the vanilla wake-up gets before the stand, and the retry step while it still plays (the
	//! controller drops a stance change asked for during the wake-up animation).
	// lf-fixed: animation settle delays, not gameplay tuning
	static const int STAND_UP_MS = 2000;
	static const int STAND_UP_TRIES = 6;

	//------------------------------------------------------------------------------------------------
	//! Owner: the woken body stands up from prone, unless something put it down again meanwhile.
	static void StandUp(SCR_ChimeraCharacter character, int triesLeft)
	{
		if (!character || character.LF_IsDowned() || character.LF_HasStunFlag())
			return;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.GetLifeState() == ECharacterLifeState.DEAD)
			return;

		if (controller.GetLifeState() == ECharacterLifeState.ALIVE && controller.GetStance() != ECharacterStance.PRONE)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 is up from the logout body (stance %2)", LF_Utils.GetPlayerUID(character), typename.EnumToString(ECharacterStance, controller.GetStance())));
			return;
		}

		if (triesLeft <= 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("%1 woke from the logout body but stays %2 (life %3)", LF_Utils.GetPlayerUID(character), typename.EnumToString(ECharacterStance, controller.GetStance()), typename.EnumToString(ECharacterLifeState, controller.GetLifeState())));
			return;
		}

		if (controller.GetLifeState() == ECharacterLifeState.ALIVE)
			controller.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOERECTED);

		GetGame().GetCallqueue().CallLater(StandUp, 1000, false, character, triesLeft - 1);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Rule(bool serving)
	{
		if (serving)
			return "jail";

		return "hospital";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character died. A logout body takes the ordinary death path for its account.
	static bool OnKilled(IEntity entity)
	{
		if (!Replication.IsServer() || !entity)
			return false;

		string uid = LF_LogoutBody.UidOfBody(entity);
		if (uid.IsEmpty())
			return false;

		LF_Debug.Info(FEATURE, string.Format("logout body of %1 killed while away: ordinary death", uid));
		LF_SpawnLogic.OnAccountDied_S(uid, entity);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_LogoutBody
{
	//------------------------------------------------------------------------------------------------
	//! The player is back (identity known). The body now waits for the spawn, which takes it back or
	//! ends it (LF_LogoutBodyRejoin.TakeBack). A player who already controls another character (a
	//! spawn path that skipped the spawn logic) cannot have both: the body ends.
	override static void OnPlayerReturned(int playerId)
	{
		if (!Replication.IsServer())
			return;

		string uid = LF_Utils.GetPlayerUID(playerId);
		if (uid.IsEmpty())
			return;

		if (!s_mUidByPlayer)
			s_mUidByPlayer = new map<int, string>();

		s_mUidByPlayer.Set(playerId, uid);
		if (!s_mBodies || !s_mBodies.Contains(uid))
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		IEntity controlled;
		if (playerManager)
			controlled = playerManager.GetPlayerControlledEntity(playerId);

		LF_LogoutBodyRecord record = s_mBodies.Get(uid);
		if (controlled && (!record || controlled.GetID() != record.m_BodyId))
			Release(uid, "rejoined in another body");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stop tracking uid's body and hand it over as it is. The snapshot is captured one last
	//! time (it is the save until the player's own saves take over).
	//! \return The live body, or null (none, gone, or dead: a dead one is left to the corpse cleanup).
	static IEntity Detach(string uid)
	{
		if (!s_mBodies || !s_mBodies.Contains(uid))
			return null;

		LF_LogoutBodyRecord record = s_mBodies.Get(uid);
		if (!record)
		{
			s_mBodies.Remove(uid);
			return null;
		}

		IEntity body = Resolve(record);
		if (!body || record.m_bDead || !LF_StorageSnapshot.IsAlive(body))
		{
			Release(uid, "rejoined");
			return null;
		}

		Recapture(record);
		s_mBodies.Remove(uid);
		Watch(body, false, uid);
		// The body is the player's character again: clear the replicated Logged out flag, or every client
		// (a late joiner too) keeps tracking it and a later release of control would show the tag.
		LF_LogoutBodyTag.Mark(body, false);
		LF_Debug.Info(FEATURE, string.Format("logout body of %1 taken back by its player", uid));
		return body;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The account uid whose logout body this is, or an empty string.
	static string UidOfBody(IEntity entity)
	{
		if (!entity || !s_mBodies)
			return string.Empty;

		EntityID id = entity.GetID();
		foreach (string uid, LF_LogoutBodyRecord record : s_mBodies)
		{
			if (record && record.m_BodyId == id)
				return uid;
		}

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! A logout body has no player, so the base game's player death never fires for it.
modded class SCR_BaseGameMode
{
	//------------------------------------------------------------------------------------------------
	protected override void OnControllableDestroyed(IEntity entity, IEntity killerEntity, notnull Instigator instigator)
	{
		if (IsMaster())
			LF_LogoutBodyRejoin.OnKilled(entity);

		super.OnControllableDestroyed(entity, killerEntity, instigator);
	}
}

//------------------------------------------------------------------------------------------------
//! The machine that takes control of a character gets it up when nothing keeps it down (#2156).
modded class SCR_CharacterControllerComponent
{
	//! Lets the server's ragdoll release settle (LF_Knockout.SETTLE_MS) and ownership arrive first.
	// lf-fixed: a replication and animation settle delay, not gameplay tuning
	protected static const int LF_WAKE_ON_CONTROL_MS = 2500;

	//------------------------------------------------------------------------------------------------
	override void OnControlledByPlayer(IEntity owner, bool controlled)
	{
		super.OnControlledByPlayer(owner, controlled);
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(owner);
		if (controlled && character && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(LF_LogoutBodyRejoin.WakeOnControl, LF_WAKE_ON_CONTROL_MS, false, character);
	}
}
