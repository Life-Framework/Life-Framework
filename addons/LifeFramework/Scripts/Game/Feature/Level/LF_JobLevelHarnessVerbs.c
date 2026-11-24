//------------------------------------------------------------------------------------------------
//! Session proof verbs for job levels (#3244): `joblevel <JOB> <level>` puts the caller's job at exactly the XP
//! that level costs, and `jobshow <JOB>` logs the job's XP and level. A gated machine (the iron smelter needs
//! SMELTING 8) is proved from a worker below the gate and one at it, and the daily XP cap (2000 for SMELTING)
//! is under the 3000 XP level 8 costs, so no one earns that in a day of play. Same gate as `repgrant` and
//! /licence: development worlds and harness builds, or an administrator. The client sends the job and the
//! level; the server decides. The set is not an award: no `LF_OnJobXpAwarded`, no daily cap booked, so the XP
//! a run then earns is the real one.
//------------------------------------------------------------------------------------------------
modded class LF_JobLevels
{
	//------------------------------------------------------------------------------------------------
	//! Server, harness only: sets a character's XP in one job to what \p level costs (level 1 is 0 XP) and
	//! fires the level change so what shows a level refreshes.
	//! \return The level now held; 0 when the job has no track, the uid is empty, or the track stops below
	//! \p level (nothing changes then).
	static int HarnessSetLevel(string playerUid, string job, int level)
	{
		LF_JobTrackDef track = Track(job);
		if (!track || playerUid.IsEmpty() || level < 1)
			return 0;

		int needed = track.XpForLevel(level);
		if (needed < 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("harness level refused: %1 has no level %2", job, level));
			return 0;
		}

		int before = XpOf(playerUid, job);
		int levelBefore = track.LevelFor(before);
		SetXp(playerUid, job, needed);
		int levelAfter = track.LevelFor(needed);
		LF_Debug.Info(FEATURE, string.Format("%1 %2 xp %3 -> %4 (harness set, level %5 -> %6)", playerUid, job, before, needed, levelBefore, levelAfter));
		if (levelAfter != levelBefore)
			LF_EventBus.Publish(LF_OnJobLevelChanged.Create(playerUid, job, levelBefore, levelAfter));

		return levelAfter;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Asks the server to set (doSet true) or log (doSet false) the caller's level in a job. The player owns
	//! their controller, so the RPC is allowed.
	void LF_AskJobLevelHarness(string job, int level, bool doSet)
	{
		if (Replication.IsServer())
			RpcAsk_LF_JobLevelHarness(job, level, doSet);
		else
			Rpc(RpcAsk_LF_JobLevelHarness, job, level, doSet);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_JobLevelHarness(string job, int level, bool doSet)
	{
		int playerId = GetPlayerId();
		bool allowed = LF_DevGate.HarnessBuild() || LF_DevGate.CheckPlayerRequest(playerId, LF_JobLevels.FEATURE, "harness job level");
		if (!allowed)
			return;

		string uid = LF_Utils.GetPlayerUID(playerId);
		if (uid.IsEmpty())
		{
			LF_Debug.Error(LF_JobLevels.FEATURE, string.Format("harness job level: player %1 has no account id", playerId));
			return;
		}

		if (doSet)
			LF_JobLevels.HarnessSetLevel(uid, job, level);

		LF_Debug.Info(LF_JobLevels.FEATURE, string.Format("harness job %1 %2 xp=%3 level=%4", uid, job, LF_JobLevels.XpOf(uid, job), LF_JobLevels.LevelOf(uid, job)));
	}
}

//------------------------------------------------------------------------------------------------
//! Harness `joblevel <JOB> <level>` and `jobshow <JOB>`: this client sends the request; the server's dev gate
//! decides and logs `[LFDebug:JobLevels] harness job <uid> <JOB> xp=N level=L`.
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		bool doSet = cmd.Verb == "joblevel";
		if (!doSet && cmd.Verb != "jobshow")
			return super.Handle(cmd);

		if (cmd.Arg1.IsEmpty() || (doSet && cmd.Arg2.IsEmpty()))
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: joblevel <JOB> <level> | jobshow <JOB>");
			return true;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no_controller");
			return true;
		}

		int level;
		if (doSet)
			level = cmd.Arg2.ToInt();

		controller.LF_AskJobLevelHarness(cmd.Arg1, level, doSet);
		LF_AgentHarness.Answer(cmd.Id, "ok", "asked " + cmd.Verb + " " + cmd.Arg1);
		return true;
	}
}
