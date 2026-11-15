//------------------------------------------------------------------------------------------------
//! Per-session player facts, server only. For now: which account uids have spawned since the
//! world started, and the spawn event published for each spawn (LF_OnPlayerSpawnedEvent).
class LF_Session
{
	static const string FEATURE = "Session";

	protected static ref set<string> s_mSeenUids;

	//------------------------------------------------------------------------------------------------
	//! Called from LF_SessionHooks on every player spawn. Records the uid and publishes the event.
	//! \return The published event, or null when this is not the server or the player id is invalid.
	static LF_OnPlayerSpawnedEvent OnPlayerSpawned(int playerId, string accountUid)
	{
		if (!Replication.IsServer())
			return null;

		if (playerId <= 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("spawn event skipped: invalid player id %1", playerId));
			return null;
		}

		if (!s_mSeenUids)
			s_mSeenUids = new set<string>();

		// An empty uid (identity not resolved yet) cannot be told apart from another empty one, so
		// it is never recorded and always counts as first; subscribers key on the player id then.
		bool first = true;
		if (accountUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("player %1 spawned without an account uid yet", playerId));
		}
		else
		{
			first = s_mSeenUids.Find(accountUid) < 0;
			if (first)
				s_mSeenUids.Insert(accountUid);
		}

		LF_OnPlayerSpawnedEvent spawned = LF_OnPlayerSpawnedEvent.Create(playerId, accountUid, first);
		LF_Debug.Info(FEATURE, string.Format("player %1 spawned (uid %2, first this session: %3)", playerId, accountUid, first));
		LF_EventBus.Publish(spawned);
		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mSeenUids = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of this file's statics (Core/LF_StaticReset.c runs every feature's hook).
modded class LF_StaticResetHooks
{
	override void Run(notnull array<string> done, bool apply)
	{
		super.Run(done, apply);
		done.Insert("LF_Session");
		if (apply)
			LF_Session.ResetStatics();
	}
}
