// Harness verb `levelupcard grant <JOB> <xp>|state` (client side, #4404). `grant` asks the server for a
// debug XP grant to the caller, so the real path runs: AddXp, the level event, the Owner RPC, the card.
// `state` answers `card=<shown|none> last=<JOB level>` so a proof waits for the card. Same gate as
// `playermenu seed`: development worlds and harness builds, or an administrator.

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "levelupcard")
			return super.Handle(cmd);

		if (cmd.Arg1 == "grant")
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (!controller)
			{
				LF_AgentHarness.Answer(cmd.Id, "error", "no player controller");
				return true;
			}

			controller.LF_AskLevelUpCardGrant(cmd.Arg2, cmd.Arg3.ToInt());
			LF_AgentHarness.Answer(cmd.Id, "ok", "asked for " + cmd.Arg3 + " xp in " + cmd.Arg2);
			return true;
		}

		if (cmd.Arg1 == "state")
		{
			LF_AgentHarness.Answer(cmd.Id, "ok", "card=" + LF_LevelUpCardView.State() + " last=" + LF_LevelUpCardView.Last());
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "error", "levelupcard grant <JOB> <xp>|state");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_AskLevelUpCardGrant(string job, int xp)
	{
		if (Replication.IsServer())
			RpcAsk_LF_LevelUpCardGrant(job, xp);
		else
			Rpc(RpcAsk_LF_LevelUpCardGrant, job, xp);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_LevelUpCardGrant(string job, int xp)
	{
		int playerId = GetPlayerId();
		bool allowed = LF_DevGate.HarnessBuild() || LF_DevGate.CheckPlayerRequest(playerId, LF_LevelUpCard.FEATURE, "harness level-up card grant");
		if (!allowed)
			return;

		string uid = LF_Utils.GetPlayerUID(playerId);
		if (uid.IsEmpty() || job.IsEmpty() || xp <= 0)
		{
			LF_Debug.Error(LF_LevelUpCard.FEATURE, string.Format("harness grant refused: player %1 job '%2' xp %3", playerId, job, xp));
			return;
		}

		LF_JobLevels.AddXp(uid, job, xp, "harness_levelup_card");
	}
}
