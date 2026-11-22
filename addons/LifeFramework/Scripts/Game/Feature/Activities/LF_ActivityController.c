//------------------------------------------------------------------------------------------------
//! The wire of the "What to do next" tab. Two asks on the player's own controller, both answered for
//! GetPlayerId() only, never for a player the client names:
//!  - RpcAsk_LF_Activities (no argument): the list (LF_ActivityData) for this player.
//!  - RpcAsk_LF_ActivityGo (an activity id): "Start" on an available activity, "Guide me" on a running
//!    one. The server checks everything again (LF_ActivityRules.StartRefusal); the client's state is
//!    never trusted. It answers with the outcome and a fresh list.
//! Both count in the QUEST bucket of LF_RateLimit; the client also throttles itself.
modded class SCR_PlayerController
{
	static const int LF_ACTIVITY_ASK_MS = 1000; // lf-fixed: client throttle under LF_RateLimit's QUEST bucket
	static const int LF_ACTIVITY_ID_MAX = 64; // lf-fixed: longer than any activity id; a longer ask is refused unread

	protected int m_iLF_ActivitiesAskedAt = -100000;
	protected int m_iLF_ActivityGoAt = -100000;

	//------------------------------------------------------------------------------------------------
	//! Client: ask for this player's list. \return false when throttled.
	bool LF_AskActivities()
	{
		int now = System.GetTickCount();
		if (now - m_iLF_ActivitiesAskedAt < LF_ACTIVITY_ASK_MS)
			return false;

		m_iLF_ActivitiesAskedAt = now;
		Rpc(RpcAsk_LF_Activities);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: "Start" or "Guide me" on one activity. \return false when throttled.
	bool LF_AskActivityGo(string activityId)
	{
		int now = System.GetTickCount();
		if (now - m_iLF_ActivityGoAt < LF_ACTIVITY_ASK_MS)
		{
			LF_Debug.Info(LF_ActivityTable.FEATURE, "activity ask skipped: pressed under a second ago");
			return false;
		}

		m_iLF_ActivityGoAt = now;
		Rpc(RpcAsk_LF_ActivityGo, activityId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the body of the asking player, alive. Null (logged) otherwise.
	protected IEntity LF_ActivityBody(string verb)
	{
		int playerId = GetPlayerId();
		if (playerId <= 0)
		{
			LF_Debug.Warn(LF_ActivityTable.FEATURE, verb + " refused: the controller has no player");
			return null;
		}

		IEntity body = GetControlledEntity();
		if (!body || !LF_Utils.IsAliveState(body))
		{
			LF_Debug.Info(LF_ActivityTable.FEATURE, string.Format("%1 refused for player %2: no living body", verb, playerId));
			return null;
		}

		return body;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the list of the body's player.
	protected string LF_ActivityWire(IEntity body)
	{
		LF_ActivityConfig config = LF_ActivityTable.Get();
		if (!config)
		{
			LF_ActivityData empty = new LF_ActivityData();
			return empty.Encode();
		}

		LF_ActivityTable.EnsureRegistered();
		return LF_ActivityData.Build(config.m_aActivities, new LF_ActivityLiveFacts(body), body.GetOrigin(), true).Encode();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_Activities()
	{
		if (!LF_RateLimit.Allow(GetPlayerId(), LF_ERequestKind.QUEST, "activities list", false))
			return;

		IEntity body = LF_ActivityBody("activities list");
		if (!body)
			return;

		string wire = LF_ActivityWire(body);
		LF_Debug.Info(LF_ActivityTable.FEATURE, string.Format("activities sent to player %1: %2 chars", GetPlayerId(), wire.Length()));
		Rpc(RpcDo_LF_Activities, wire, string.Empty, string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ActivityGo(string activityId)
	{
		if (!LF_RateLimit.Allow(GetPlayerId(), LF_ERequestKind.QUEST, "activity start", true))
			return;

		if (activityId.IsEmpty() || activityId.Length() > LF_ACTIVITY_ID_MAX)
		{
			LF_Debug.Warn(LF_ActivityTable.FEATURE, string.Format("activity ask refused for player %1: a bad id of %2 chars", GetPlayerId(), activityId.Length()));
			return;
		}

		IEntity body = LF_ActivityBody("activity start");
		if (!body)
			return;

		string outcome = LF_ActivityGuide.Start(body, activityId);
		// "Guide me" on a running activity: the waypoint goes back to its step.
		if (outcome == "running" && LF_ActivityGuide.Focus(body, activityId))
			outcome = "guided";
		else if (outcome.IsEmpty())
			outcome = "started";

		Rpc(RpcDo_LF_Activities, LF_ActivityWire(body), activityId, outcome);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the list, and the outcome of a press when this answers one.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_Activities(string wire, string activityId, string outcome)
	{
		LF_ActivityPage.OnData(wire, activityId, outcome);
	}
}
