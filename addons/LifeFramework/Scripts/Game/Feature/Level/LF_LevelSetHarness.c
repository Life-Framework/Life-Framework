// Harness verb `levelset <JOB> <level>` / `levelset <JOB> xp <n>` (#4773). Sets a character's trade
// level or total XP directly: the daily XP cap (LF_JobLevels.CapToday) is skipped and the granted XP is
// never booked against it, so a playtest can stand at MINER 10 (the labour-office perk pick) without
// 5 days of capped play. The level event still fires, so the level-up card and the replicated level
// refresh like a real level change.
//
// A harness build only: the verb and its RPC are compiled out without -scrDefine LF_HARNESS, so a real
// server has neither. The two statics below stay in every build because the suite (built without the
// define) tests them; `HarnessSetLevel` refuses outside a harness build and nothing in a non-harness
// build calls either.

//------------------------------------------------------------------------------------------------
modded class LF_JobLevels
{
	//------------------------------------------------------------------------------------------------
	//! Server: sets one job's XP to `xp` (up or down), whatever the daily cap says, and without
	//! counting it toward the cap. Publishes LF_OnJobLevelChanged when the level moves.
	//! Callers go through HarnessSetXp, which checks for a harness build; the suite calls this one.
	//! \return The XP now held; -1 when refused (not the server, empty uid, no track, negative xp).
	static int SetXpUnchecked(string playerUid, string job, int xp)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return -1;

		if (playerUid.IsEmpty() || xp < 0)
		{
			LF_Debug.Error(FEATURE, string.Format("harness set refused: uid '%1' job '%2' xp %3", playerUid, job, xp));
			return -1;
		}

		LF_JobTrackDef track = Track(job);
		if (!track)
		{
			LF_Debug.Warn(FEATURE, string.Format("harness set skipped: job '%1' has no track in the jobs config", job));
			return -1;
		}

		int before = XpOf(playerUid, job);
		int levelBefore = track.LevelFor(before);
		SetXp(playerUid, job, xp);
		int levelAfter = track.LevelFor(xp);
		LF_Debug.Info(FEATURE, string.Format("harness set %1 xp %2 -> %3 for %4 (level %5 -> %6, daily cap skipped)", job, before, xp, playerUid, levelBefore, levelAfter));
		if (levelAfter != levelBefore)
			LF_EventBus.Publish(LF_OnJobLevelChanged.Create(playerUid, job, levelBefore, levelAfter));

		return xp;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sets one job to exactly `level` (the XP that reaches it, `LF_JobTrackDef.XpForLevel`).
	//! \return The XP now held; -1 when refused or the job's table stops below `level`.
	static int SetLevelUnchecked(string playerUid, string job, int level)
	{
		LF_JobTrackDef track = Track(job);
		if (!track)
		{
			LF_Debug.Warn(FEATURE, string.Format("harness set skipped: job '%1' has no track in the jobs config", job));
			return -1;
		}

		int xp = track.XpForLevel(level);
		if (xp < 0)
		{
			LF_Debug.Error(FEATURE, string.Format("harness set refused: job '%1' has no level %2 (the table stops lower)", job, level));
			return -1;
		}

		return SetXpUnchecked(playerUid, job, xp);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The XP now held, or -1 when refused. Refuses outside a harness build.
	static int HarnessSetXp(string playerUid, string job, int xp)
	{
		if (!LF_DevGate.HarnessBuild())
		{
			LF_Debug.Warn(FEATURE, "harness set refused: not a LF_HARNESS build");
			return -1;
		}

		return SetXpUnchecked(playerUid, job, xp);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The XP now held, or -1 when refused. Refuses outside a harness build.
	static int HarnessSetLevel(string playerUid, string job, int level)
	{
		if (!LF_DevGate.HarnessBuild())
		{
			LF_Debug.Warn(FEATURE, "harness set refused: not a LF_HARNESS build");
			return -1;
		}

		return SetLevelUnchecked(playerUid, job, level);
	}
}

#ifdef LF_HARNESS
//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "levelset")
			return super.Handle(cmd);

		string job = cmd.Arg1;
		bool byXp = cmd.Arg2 == "xp";
		string number = cmd.Arg2;
		if (byXp)
			number = cmd.Arg3;

		if (job.IsEmpty() || number.IsEmpty())
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "levelset <JOB> <level> | levelset <JOB> xp <n>");
			return true;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "no player controller");
			return true;
		}

		controller.LF_AskLevelSet(job, byXp, number.ToInt());
		string what = "level " + number;
		if (byXp)
			what = number + " xp";

		LF_AgentHarness.Answer(cmd.Id, "ok", "asked to set " + job + " to " + what + " (daily cap skipped)");
		return true;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_AskLevelSet(string job, bool byXp, int number)
	{
		if (Replication.IsServer())
			RpcAsk_LF_LevelSet(job, byXp, number);
		else
			Rpc(RpcAsk_LF_LevelSet, job, byXp, number);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_LevelSet(string job, bool byXp, int number)
	{
		// Compiled only in a LF_HARNESS build, where this check is open; the dev gate stays the one door.
		if (!LF_DevGate.CheckHarnessRequest(GetControlledEntity(), "level set"))
			return;

		string uid = LF_Utils.GetPlayerUID(GetPlayerId());
		if (byXp)
			LF_JobLevels.HarnessSetXp(uid, job, number);
		else
			LF_JobLevels.HarnessSetLevel(uid, job, number);
	}
}
#endif
