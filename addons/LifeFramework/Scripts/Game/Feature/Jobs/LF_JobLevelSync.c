//------------------------------------------------------------------------------------------------
//! Keeps the DELIVERY level a client reads (LF_PlayerJobComponent's replicated job level, for the
//! sign-up offer) in step with LF_JobLevels, whatever raised it: a run, a quest, a debug grant
//! (#749). Subscribes to LF_OnJobLevelChanged on the server and refreshes every live body of that
//! character. Other jobs' levels reach no client.
class LF_JobLevelSync : LF_EventSubscriber
{
	protected static ref LF_JobLevelSync s_Subscriber;

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes once (game start).
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.SubscriberCount(LF_OnJobLevelChanged) > 0)
			return true;

		s_Subscriber = new LF_JobLevelSync();
		return LF_EventBus.Subscribe(LF_OnJobLevelChanged, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnJobLevelChanged changed = LF_OnJobLevelChanged.Cast(busEvent);
		if (!changed || changed.m_sJob != LF_JobLevels.JOB_DELIVERY)
			return true;

		int bodies = RefreshBodies(changed.m_sPlayerUid);
		LF_Debug.Log("Jobs", string.Format("delivery level %1 -> %2 for %3 pulled onto %4 live body(ies)", changed.m_iLevelBefore, changed.m_iLevel, changed.m_sPlayerUid, bodies));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Live bodies of the character whose job component took the DELIVERY level.
	static int RefreshBodies(string playerUid)
	{
		if (playerUid.IsEmpty() || !GetGame())
			return 0;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return 0;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		int bodies = 0;
		foreach (int playerId : playerIds)
		{
			if (LF_Utils.GetPlayerUID(playerId) != playerUid)
				continue;

			LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(players.GetPlayerControlledEntity(playerId));
			if (!jobs)
				continue;

			jobs.PullCareerFor(playerUid);
			bodies++;
		}

		return bodies;
	}
}
