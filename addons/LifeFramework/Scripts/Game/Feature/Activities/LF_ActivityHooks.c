// LF_ActivityHooks.c - where the activities meet the quest system and the game mode, as modded
// classes so the quest files keep their own paths: an accepted activity step moves the waypoint and
// shows its note, the guide refresh after a turn-in or a spawn includes the activity mark, the world
// start registers the activity quests, and a leaver's guide state goes with them.

//------------------------------------------------------------------------------------------------
modded class LF_QuestTrackerComponent
{
	//------------------------------------------------------------------------------------------------
	override bool AcceptQuest(string questId, bool chained = false, bool quiet = false)
	{
		bool accepted = super.AcceptQuest(questId, chained, quiet);
		if (accepted && Replication.IsServer() && questId.StartsWith(LF_ActivityTable.QUEST_PREFIX))
			LF_ActivityGuide.OnStepStarted(GetOwner(), questId, !chained);

		return accepted;
	}

	//------------------------------------------------------------------------------------------------
	//! The arrival guide first; a running activity step then takes the one waypoint (the player chose it
	//! last). Returns the quest id guided to.
	override string RefreshGuide()
	{
		string guided = super.RefreshGuide();
		if (!Replication.IsServer() || !GetOwner())
			return guided;

		LF_ActivityTable.EnsureRegistered();
		string activity = LF_ActivityGuide.Refresh(GetOwner());
		if (!activity.IsEmpty())
			return activity;

		return guided;
	}
}

//------------------------------------------------------------------------------------------------
//! "postal.run_complete" when the postal van run is handed in and paid (#4088), so the postal starter's last
//! step finishes. The postal files keep their own path.
modded class LF_PostalRun
{
	static const string SIGNAL_DONE = "postal.run_complete";

	//------------------------------------------------------------------------------------------------
	override static LF_EPostalResult HandIn(IEntity user, string uid, IEntity office, out int paid)
	{
		LF_EPostalResult result = super.HandIn(user, uid, office, paid);
		if (result == LF_EPostalResult.OK && user && Replication.IsServer() && LF_QuestManager.GetInstance())
			LF_QuestManager.GetInstance().ReportSignal(user, SIGNAL_DONE);

		return result;
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer())
			LF_ActivityTable.EnsureRegistered();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (Replication.IsServer())
			LF_ActivityGuide.Forget(LF_Utils.GetPlayerUID(playerId));

		super.OnPlayerDisconnected(playerId, cause, timeout);
	}
}
