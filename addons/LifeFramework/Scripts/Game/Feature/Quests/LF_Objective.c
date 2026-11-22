// LF_Objective.c - the pinned objective line under the money (LF_BetaHud, review A14). The guides that put the
// player's one waypoint up (LF_ArrivalGuide, LF_ActivityGuide) send the step's title beside it, with the same
// id; the client keeps the line until that guide clears it, so it survives looking away, a toast fading and
// reaching the waypoint. A rejoin or a respawn asks the server to send the waypoint, the line and the step's
// note again once the client controls its body (RpcAsk_LF_GuideResume, review B7).

//------------------------------------------------------------------------------------------------
//! Client: the line the HUD shows. One line at a time; a guide clears only its own.
class LF_Objective
{
	static const string FEATURE = "Objective";

	protected static string s_sSource;
	protected static string s_sTitleKey;
	protected static int s_iChanges;

	//------------------------------------------------------------------------------------------------
	//! Puts `titleKey` up for `source` (a waypoint id). An empty title clears that source's line.
	//! \return True when the line changed (the HUD flashes it).
	static bool Set(string source, string titleKey)
	{
		if (source.IsEmpty())
			return false;

		if (titleKey.IsEmpty())
			return Clear(source);

		if (source == s_sSource && titleKey == s_sTitleKey)
			return false;

		s_sSource = source;
		s_sTitleKey = titleKey;
		s_iChanges++;
		LF_Debug.Info(FEATURE, string.Format("objective from %1: %2", source, titleKey));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when `source` held the line and it is gone.
	static bool Clear(string source)
	{
		if (source.IsEmpty() || source != s_sSource)
			return false;

		s_sSource = string.Empty;
		s_sTitleKey = string.Empty;
		s_iChanges++;
		LF_Debug.Info(FEATURE, "objective cleared by " + source);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static string TitleKey()
	{
		return s_sTitleKey;
	}

	//------------------------------------------------------------------------------------------------
	static string Source()
	{
		return s_sSource;
	}

	//------------------------------------------------------------------------------------------------
	//! Grows by one on every change: the HUD compares it to the last one it drew.
	static int Changes()
	{
		return s_iChanges;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sSource = string.Empty;
		s_sTitleKey = string.Empty;
		s_iChanges = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Objective's statics (LF_StaticReset finds it by reflection).
class LF_Objective_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_Objective";
	}

	override void Apply()
	{
		LF_Objective.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The game sink sends the line on the player's own channel, as it sends the waypoint.
modded class LF_JobWaypointRplSink
{
	//------------------------------------------------------------------------------------------------
	override void Objective(string uid, string id, string titleKey)
	{
		LF_MapShareComponent channel = Channel(uid);
		if (channel)
			channel.ServerObjective(id, titleKey);
	}
}

//------------------------------------------------------------------------------------------------
//! The objective leg of the owner channel.
modded class LF_MapShareComponent
{
	//------------------------------------------------------------------------------------------------
	//! Server: put the line up on this player's client (an empty title takes that id's line down).
	void ServerObjective(string id, string titleKey)
	{
		if (!Replication.IsServer())
			return;

		if (IsLocalOwner())
			RpcDo_LF_Objective(id, titleKey);
		else
			Rpc(RpcDo_LF_Objective, id, titleKey);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_Objective(string id, string titleKey)
	{
		LF_Objective.Set(id, titleKey);
	}
}

//------------------------------------------------------------------------------------------------
//! The resume ask: the client asks once it controls a body, so the answer never lands before its HUD.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		if (!to || GetGame().GetPlayerController() != this)
			return;

		Rpc(RpcAsk_LF_GuideResume);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the waypoint, the objective line and the running step's note for this player's body.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_GuideResume()
	{
		int playerId = GetPlayerId();
		if (!LF_RateLimit.Allow(playerId, LF_ERequestKind.QUEST, "guide resume", false))
			return;

		LF_ObjectiveResume.Resume(GetControlledEntity());
	}
}

//------------------------------------------------------------------------------------------------
//! Server side of the resume ask.
class LF_ObjectiveResume
{
	//------------------------------------------------------------------------------------------------
	//! Puts the body's current step back on its player's screen and map, and shows a running activity
	//! step's note again. \return The quest id guided to, or empty.
	static string Resume(IEntity body)
	{
		if (!body || !Replication.IsServer() || !LF_Utils.IsAliveState(body))
			return string.Empty;

		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(body.FindComponent(LF_QuestTrackerComponent));
		if (!tracker)
			return string.Empty;

		string guided = tracker.RefreshGuide();
		bool reminded = LF_ActivityGuide.Remind(body);
		LF_Debug.Info(LF_Objective.FEATURE, string.Format("resume for %1: guided to '%2', note shown %3", LF_Utils.GetPlayerUID(body), guided, reminded));
		return guided;
	}
}
