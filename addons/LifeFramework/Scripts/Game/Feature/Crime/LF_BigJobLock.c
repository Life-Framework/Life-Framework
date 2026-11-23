//------------------------------------------------------------------------------------------------
//! What LF_BigJobLock.TryBegin answers.
enum LF_EBigJobLock
{
	OK,
	//! Another big job holds the server-wide lock.
	BUSY,
	//! This job's own cooldown has not run out.
	COOLDOWN,
	//! An empty job or holder key.
	BAD_INPUT
}

//------------------------------------------------------------------------------------------------
//! One job's cooldown in the "bigjob" save section (#4193). Seconds left, never world time (it
//! restarts at 0 on a boot). Field order is the binary save order; append only.
class LF_BigJobCooldownRow : Managed
{
	static const int ROW_VERSION = 1;

	//! Row format, written first. 1 is this shape.
	int m_iVersion;
	string m_sJobKey;
	int m_iSecondsLeft;

	//------------------------------------------------------------------------------------------------
	static LF_BigJobCooldownRow Create(string jobKey, int secondsLeft)
	{
		LF_BigJobCooldownRow row = new LF_BigJobCooldownRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sJobKey = jobKey;
		row.m_iSecondsLeft = secondsLeft;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The server-wide big-job lock over robbery rungs 4 to 6 (pharmacy, bank vault, reserve depot) and,
//! in 0.4, the prison break (plan 0.3.0 Crime A6, decision N17). Server only.
//!
//! One big job runs on the server at a time: TryBegin takes the lock, Finish releases it and starts
//! that job's own cooldown, during which the same job is refused. A lock never released (the crew
//! logged off, the heist entity was deleted) expires MAX_RUN_SECONDS after it began and the job
//! goes into its cooldown, so the server is never locked for good.
//!
//! Across a restart (the "bigjob" save section) the cooldowns keep their seconds left. A job that
//! was running at the save comes back as that job in its full cooldown: the heist itself did not
//! survive the restart, and a restart must never reset a cooldown.
//!
//! Callers pass the cooldown from their pack config; COOLDOWN_* are the plan's defaults.
class LF_BigJobLock
{
	static const string FEATURE = "BigJob";
	//! Rung 4, hospital pharmacy: 2 h.
	static const int COOLDOWN_PHARMACY = 7200; // lf-fixed: plan default only; callers pass the pack-config cooldown
	//! Rung 5, bank vault: 3 h.
	static const int COOLDOWN_BANK = 10800; // lf-fixed: plan default only; callers pass the pack-config cooldown
	//! Rung 6, reserve depot (0.4): 6 h.
	static const int COOLDOWN_RESERVE = 21600; // lf-fixed: plan default only; callers pass the pack-config cooldown
	//! A lock older than this is treated as a job that ended without Finish.
	static const int MAX_RUN_SECONDS = 2700; // lf-fixed: a safety net for a lost Finish, longer than any heist run

	//! The running job's key, empty when the lock is free.
	protected static string s_sRunningJob;
	//! Who holds the lock (the crew leader's identity key), for the log and for Finish.
	protected static string s_sHolder;
	protected static int s_iRunningCooldown;
	protected static int s_iBeganAt;
	//! Job key -> the world second its cooldown ends.
	protected static ref map<string, int> s_mCooldownEnds;
	//! Tests only: a fixed clock instead of world time; negative uses the world.
	protected static int s_iClockOverride = -1;

	//------------------------------------------------------------------------------------------------
	protected static map<string, int> Ends()
	{
		if (!s_mCooldownEnds)
			s_mCooldownEnds = new map<string, int>();

		return s_mCooldownEnds;
	}

	//------------------------------------------------------------------------------------------------
	//! World seconds now (0 with no world), or the test clock.
	static int Now()
	{
		if (s_iClockOverride >= 0)
			return s_iClockOverride;

		if (!GetGame())
			return 0;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		return world.GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: fix the clock at seconds; a negative value goes back to world time.
	static void SetClockForTests(int seconds)
	{
		s_iClockOverride = seconds;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_sRunningJob = string.Empty;
		s_sHolder = string.Empty;
		s_iRunningCooldown = 0;
		s_iBeganAt = 0;
		s_mCooldownEnds = null;
		s_iClockOverride = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! A lock older than MAX_RUN_SECONDS becomes its job's cooldown.
	protected static void ExpireStale(int now)
	{
		if (s_sRunningJob.IsEmpty())
			return;

		if (now - s_iBeganAt < MAX_RUN_SECONDS)
			return;

		LF_Debug.Warn(FEATURE, string.Format("big job %1 (holder %2) never finished; the lock expired after %3 s and its cooldown starts", s_sRunningJob, s_sHolder, MAX_RUN_SECONDS));
		Release(now);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Release(int now)
	{
		Ends().Set(s_sRunningJob, now + LF_Utils.MaxInt(0, s_iRunningCooldown));
		s_sRunningJob = string.Empty;
		s_sHolder = string.Empty;
		s_iRunningCooldown = 0;
		s_iBeganAt = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Asks to start a big job. Server only.
	//! \param jobKey Stable job id ("pharmacy", "bank"; the same key every boot).
	//! \param holderKey The starting player's identity key.
	//! \param cooldownSeconds The job's cooldown from its config, started by Finish.
	//! \param now World seconds (Now()).
	static LF_EBigJobLock TryBegin(string jobKey, string holderKey, int cooldownSeconds, int now)
	{
		if (jobKey.IsEmpty() || holderKey.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "big job refused: empty job or holder key");
			return LF_EBigJobLock.BAD_INPUT;
		}

		ExpireStale(now);
		if (!s_sRunningJob.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("big job %1 for %2 refused: %3 is running (holder %4)", jobKey, holderKey, s_sRunningJob, s_sHolder));
			return LF_EBigJobLock.BUSY;
		}

		int left = CooldownLeft(jobKey, now);
		if (left > 0)
		{
			LF_Debug.Info(FEATURE, string.Format("big job %1 for %2 refused: cooldown, %3 s left", jobKey, holderKey, left));
			return LF_EBigJobLock.COOLDOWN;
		}

		s_sRunningJob = jobKey;
		s_sHolder = holderKey;
		s_iRunningCooldown = cooldownSeconds;
		s_iBeganAt = now;
		Ends().Remove(jobKey);
		LF_Debug.Info(FEATURE, string.Format("big job %1 started by %2; the server-wide lock is taken", jobKey, holderKey));
		return LF_EBigJobLock.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The job ended (paid out, failed, or abandoned): releases the lock and starts the job's cooldown.
	//! \return False when jobKey is not the running job (nothing changes).
	static bool Finish(string jobKey, int now)
	{
		if (jobKey.IsEmpty() || jobKey != s_sRunningJob)
		{
			LF_Debug.Warn(FEATURE, string.Format("finish of big job %1 ignored: the running job is '%2'", jobKey, s_sRunningJob));
			return false;
		}

		LF_Debug.Info(FEATURE, string.Format("big job %1 ended; the lock is free and its %2 s cooldown starts", jobKey, s_iRunningCooldown));
		Release(now);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The running job's key, or empty.
	static string RunningJob(int now)
	{
		ExpireStale(now);
		return s_sRunningJob;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Seconds until jobKey may start again, 0 when it may.
	static int CooldownLeft(string jobKey, int now)
	{
		int ends;
		if (!Ends().Find(jobKey, ends))
			return 0;

		return LF_Utils.MaxInt(0, ends - now);
	}

	//------------------------------------------------------------------------------------------------
	//! The string-table key for a refusal, empty for OK.
	static string RefusalKey(LF_EBigJobLock result)
	{
		if (result == LF_EBigJobLock.BUSY)
			return "#LF-BigJob_Busy";

		if (result == LF_EBigJobLock.COOLDOWN)
			return "#LF-BigJob_Cooldown";

		if (result == LF_EBigJobLock.BAD_INPUT)
			return "#LF-BigJob_Unavailable";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The refusal in words for the player, with the minutes left on a cooldown. Empty for OK.
	static string RefusalText(LF_EBigJobLock result, string jobKey, int now)
	{
		string key = RefusalKey(result);
		if (key.IsEmpty())
			return string.Empty;

		int minutes = (CooldownLeft(jobKey, now) + 59) / 60;
		return LF_Terms.Text(key, minutes.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: every cooldown still running at now, and the running job as its full cooldown.
	static array<ref LF_BigJobCooldownRow> ExportAll(int now)
	{
		ExpireStale(now);
		array<ref LF_BigJobCooldownRow> rows = new array<ref LF_BigJobCooldownRow>();
		foreach (string key, int ends : Ends())
		{
			if (key == s_sRunningJob)
				continue;

			int left = ends - now;
			if (left > 0)
				rows.Insert(LF_BigJobCooldownRow.Create(key, left));
		}

		if (!s_sRunningJob.IsEmpty())
			rows.Insert(LF_BigJobCooldownRow.Create(s_sRunningJob, LF_Utils.MaxInt(0, s_iRunningCooldown)));

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the saved cooldowns replace the live ones. After a boot nothing runs, so the
	//! lock is free; a re-apply to a live session keeps the job that is running now (its row, the
	//! running job saved as a cooldown, is skipped). Idempotent.
	//! \return How many cooldowns were applied.
	static int ApplyAll(notnull array<ref LF_BigJobCooldownRow> rows, int now)
	{
		ExpireStale(now);
		s_mCooldownEnds = new map<string, int>();
		int applied;
		foreach (LF_BigJobCooldownRow row : rows)
		{
			if (!row || row.m_sJobKey.IsEmpty() || row.m_iSecondsLeft <= 0)
				continue;

			if (row.m_sJobKey == s_sRunningJob)
				continue;

			s_mCooldownEnds.Set(row.m_sJobKey, now + row.m_iSecondsLeft);
			applied++;
		}

		return applied;
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "bigjob" (#4193): the big-job cooldowns as seconds left; a job running at the save
//! is saved as its full cooldown. OnAbsent keeps the default no-op: an older save has no cooldowns.
//------------------------------------------------------------------------------------------------
class LF_BigJobLockSavePayload : LF_SavePayload
{
	static const string NAME = "bigjob";

	//------------------------------------------------------------------------------------------------
	override string Name()
	{
		return NAME;
	}

	//------------------------------------------------------------------------------------------------
	override int Version()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_BigJobCooldownRow> rows = LF_BigJobLock.ExportAll(LF_BigJobLock.Now());
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 big-job cooldown row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_BigJobCooldownRow> rows = new array<ref LF_BigJobCooldownRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_BigJobLock.FEATURE, "big-job cooldowns failed to read; the live lock stays as it was");
			return false;
		}

		int applied = LF_BigJobLock.ApplyAll(rows, LF_BigJobLock.Now());
		LF_Debug.Info("Persistence", string.Format("applied %1 big-job cooldown row(s)", applied));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BigJobLock's statics (LF_StaticReset finds it by reflection).
class LF_BigJobLock_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BigJobLock";
	}

	override void Apply()
	{
		LF_BigJobLock.ResetStatics();
	}
}
