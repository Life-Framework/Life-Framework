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
			LF_Knockout.End(character);

		LF_Debug.Info(FEATURE, string.Format("%1 rejoins inside the window: takes back the body at %2", uid, body.GetOrigin()));
		return body;
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
		Watch(body, false);
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
