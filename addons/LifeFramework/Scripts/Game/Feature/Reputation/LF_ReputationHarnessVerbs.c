//------------------------------------------------------------------------------------------------
//! Session proof verbs for standing (#3084): `repgrant <FACTION> <points>` puts the caller's standing with a
//! faction at exactly that many points, and `repshow <FACTION>` logs it. The proof of a gated offer (the harbour
//! crates contract) needs a Known character without earning ten runs first. Same gate as /licence: development
//! worlds and harness builds, or an administrator. The client sends the faction and the points; the server decides.
//! The set is not an award: it does not touch the daily gain cap, so the points a run then earns are the real ones.
//------------------------------------------------------------------------------------------------
modded class LF_Reputation
{
	//------------------------------------------------------------------------------------------------
	//! Server, harness only: sets a character's points with a faction (clamped like any change) and fires
	//! the tier change so the owner's tiers reach their client.
	//! \return The points now held; 0 when the faction is unknown.
	static int HarnessSet(string playerUid, string faction, int points)
	{
		LF_ReputationFactionDef def = FactionDef(faction);
		if (!def || playerUid.IsEmpty())
			return 0;

		int before = Points(playerUid, faction);
		int after = def.Clamp(points);
		SetPoints(playerUid, faction, after);
		int tierBefore = def.TierFor(before);
		int tierAfter = def.TierFor(after);
		LF_Debug.Info(FEATURE, string.Format("%1 %2 %3 -> %4 (harness set)", playerUid, faction, before, after));
		if (tierAfter != tierBefore)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 %2 tier %3 -> %4", playerUid, faction, tierBefore, tierAfter));
			LF_EventBus.Publish(LF_OnReputationChanged.Create(playerUid, faction, tierBefore, tierAfter));
		}

		return after;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Asks the server to set (doSet true) or log (doSet false) the caller's standing. The player owns their
	//! controller, so the RPC is allowed.
	void LF_AskReputationHarness(string faction, int points, bool doSet)
	{
		if (Replication.IsServer())
			RpcAsk_LF_ReputationHarness(faction, points, doSet);
		else
			Rpc(RpcAsk_LF_ReputationHarness, faction, points, doSet);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ReputationHarness(string faction, int points, bool doSet)
	{
		int playerId = GetPlayerId();
		// Development worlds and harness builds, or an administrator (the same gate as /licence and /grid).
		bool allowed = LF_DevGate.HarnessBuild() || LF_DevGate.CheckPlayerRequest(playerId, LF_Reputation.FEATURE, "harness standing");
		if (!allowed)
			return;

		string uid = LF_Utils.GetPlayerUid(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
		if (uid.IsEmpty())
		{
			LF_Debug.Error(LF_Reputation.FEATURE, string.Format("harness standing: player %1 has no account id", playerId));
			return;
		}

		if (doSet)
			LF_Reputation.HarnessSet(uid, faction, points);

		LF_Debug.Info(LF_Reputation.FEATURE, string.Format("harness standing %1 %2 points=%3 tier=%4", uid, faction, LF_Reputation.Points(uid, faction), LF_Reputation.Tier(uid, faction)));
	}
}

//------------------------------------------------------------------------------------------------
//! Harness `repgrant <FACTION> <points>` and `repshow <FACTION>`: this client sends the request; the server's
//! dev gate decides and logs `[LFDebug:Reputation] harness standing <uid> <FACTION> points=N tier=T`.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		bool grant = cmd.Verb == "repgrant";
		if (!grant && cmd.Verb != "repshow")
			return super.Handle(cmd);

		if (cmd.Arg1.IsEmpty() || (grant && cmd.Arg2.IsEmpty()))
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: repgrant <FACTION> <points> | repshow <FACTION>");
			return true;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_controller");
			return true;
		}

		int points;
		if (grant)
			points = cmd.Arg2.ToInt();

		controller.LF_AskReputationHarness(cmd.Arg1, points, grant);
		LF_AgentHarness.Answer(cmd.Id, "ok", "asked " + cmd.Verb + " " + cmd.Arg1);
		return true;
	}
}
