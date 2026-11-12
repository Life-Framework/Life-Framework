//------------------------------------------------------------------------------------------------
//! Per-job XP and levels (#749, decision D12): one XP track per job replaces the single player
//! level. Keyed by the character record key (LF_Utils.GetPlayerUID) and a job id from the "jobs"
//! config (LF_JobsConfig.m_aTracks: CIVILIAN, DELIVERY, FARMER, MINER, ...). Server-only state.
//!
//! LEVELS. The store holds total XP per job; the level is derived from the job's thresholds on
//! every read (LF_JobTrackDef.LevelFor), so a rebalanced table reaches existing characters.
//! A job the config does not list reads as level 1 and earns nothing.
//!
//! EARNING. Only server code adds XP, through AddXp (a direct amount, e.g. a quest's reward) or
//! Award (an action id whose XP, job and cooldown are config). A client never sends XP.
//! - Cooldown: an action's m_fCooldownSeconds per character; an award inside it earns nothing.
//! - Daily cap: a track's m_iDailyXpCap per character and job per day of server time (24 h since
//!   boot); the part over the cap is dropped. Both clocks are session state and are not saved.
//!
//! GATES. Readers ask LevelOf(uid, job): delivery sign-up (DELIVERY), licence purchase (the gate's
//! job), chain steps and quests (their m_sLevelJob), lockpicking (CIVILIAN).
//!
//! PERSISTENCE. The "joblevels" payload (save version 28): one LF_JobXpRow per (character, job).
//! ApplyAll replaces the store. A save from before it has no rows: the "progression" payload
//! seeds the store from its single level and delivery career (SeedFromLegacy), so a version 7-27
//! save keeps its progress (the single level onto CIVILIAN, the career onto DELIVERY).
//------------------------------------------------------------------------------------------------
class LF_JobLevels
{
	static const string FEATURE = "JobLevels";

	static const string JOB_CIVILIAN = "CIVILIAN";
	static const string JOB_DELIVERY = "DELIVERY";
	static const string JOB_FARMER = "FARMER";

	static const string ACTION_DELIVERY_RUN = "delivery_run";
	static const string ACTION_FRUIT_PICK = "fruit_pick";
	static const string ACTION_GATHER = "gather";

	//! Seconds in one day of the daily cap.
	static const float DAY_SECONDS = 86400;	// lf-fixed: the length of a day, the cap itself is config

	//! The pre-#749 curves, used only to convert a save's single level and delivery career to XP
	//! and back. The single level cost level x 100 to the next with no top (LF_PlayerLevelComponent).
	//! The career cost the jobs config's m_iJobXpFirstLevel to level 2 and m_fJobXpLevelGrowth times
	//! the step before for each later one (shipped 500 and 2), read at migration time.
	static const int LEGACY_LEVEL_STEP = 100;	// lf-fixed: a shipped save format, not a tunable
	//! Highest single level the conversion reads: 50 L (L - 1) stays far inside an int (#749 review).
	static const int LEGACY_MAX_SINGLE_LEVEL = 1000;	// lf-fixed: int overflow guard, no save reaches it
	//! Highest career level the conversion reads: the growing career curve leaves an int past it.
	static const int LEGACY_MAX_LEVEL = 20;	// lf-fixed: overflow guard on the growing career curve
	//! Largest total XP a legacy conversion returns.
	static const int LEGACY_MAX_XP = 2000000000;	// lf-fixed: int overflow guard

	//! uid -> job -> total XP.
	protected static ref map<string, ref map<string, int>> s_mXp;
	//! uid|action -> world time (s) of the last award. Session-only.
	protected static ref map<string, float> s_mLastAward;
	//! uid|job -> XP earned on day s_mCapDay[key]. Session-only.
	protected static ref map<string, int> s_mEarnedToday;
	protected static ref map<string, int> s_mCapDay;

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset). The next world's save load fills the store again.
	static void ResetStatics()
	{
		s_mXp = null;
		s_mLastAward = null;
		s_mEarnedToday = null;
		s_mCapDay = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref map<string, int>> Store()
	{
		if (!s_mXp)
			s_mXp = new map<string, ref map<string, int>>();

		return s_mXp;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The ladder of a job from the loaded config, or null.
	static LF_JobTrackDef Track(string job)
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config)
			return null;

		return config.FindTrack(job);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Total XP of a character in a job; 0 when none.
	static int XpOf(string playerUid, string job)
	{
		if (!s_mXp || playerUid.IsEmpty())
			return 0;

		map<string, int> jobs = s_mXp.Get(playerUid);
		if (!jobs)
			return 0;

		return jobs.Get(job);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The character's level in a job: 1 plus the thresholds its XP reached; 1 for an
	//! unknown job, an unknown character or an empty uid.
	static int LevelOf(string playerUid, string job)
	{
		LF_JobTrackDef track = Track(job);
		if (!track)
			return 1;

		return track.LevelFor(XpOf(playerUid, job));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The level of the character a body plays in a job; 1 for a body with no player.
	static int LevelOfBody(IEntity body, string job)
	{
		if (!body)
			return 1;

		return LevelOf(LF_Utils.GetPlayerUID(body), job);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: adds XP to one job. The job's daily cap drops the part over it. A job the config does
	//! not list earns nothing (logged).
	//! \param[in] reason What earned it, for the log ("quest", "delivery_run", ...).
	//! \return XP actually added.
	static int AddXp(string playerUid, string job, int amount, string reason)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return 0;

		if (playerUid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("xp refused: empty player id (job %1, %2)", job, reason));
			return 0;
		}

		if (amount <= 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("xp refused: non-positive amount %1 for %2 (job %3, %4)", amount, playerUid, job, reason));
			return 0;
		}

		LF_JobTrackDef track = Track(job);
		if (!track)
		{
			LF_Debug.Warn(FEATURE, string.Format("xp skipped: job '%1' has no track in the jobs config (%2, %3 xp from %4)", job, playerUid, amount, reason));
			return 0;
		}

		int granted = CapToday(playerUid, track, amount);
		if (granted <= 0)
		{
			LF_Debug.Info(FEATURE, string.Format("xp capped: %1 reached the daily %2 cap of %3 (%4)", playerUid, job, track.m_iDailyXpCap, reason));
			return 0;
		}

		int before = XpOf(playerUid, job);
		int levelBefore = track.LevelFor(before);
		SetXp(playerUid, job, before + granted);
		int levelAfter = track.LevelFor(before + granted);

		LF_Debug.Info(FEATURE, string.Format("+%1 %2 xp for %3 from %4 (total %5, level %6)", granted, job, playerUid, reason, before + granted, levelAfter));
		if (levelAfter > levelBefore)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 %2 level %3 -> %4", playerUid, job, levelBefore, levelAfter));
			// Whoever shows a level (the DELIVERY level replicated for the sign-up offer) refreshes
			// from here, whatever granted the XP: a run, a quest, a debug grant.
			LF_EventBus.Publish(LF_OnJobLevelChanged.Create(playerUid, job, levelBefore, levelAfter));
		}

		return granted;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: awards an in-world action's XP (config: job, XP per unit, cooldown).
	//! \param[in] units How many of the action (fruit picked); the XP is per unit.
	//! \return XP actually added; 0 for an unknown action, on cooldown or capped.
	static int Award(string playerUid, string action, int units = 1)
	{
		if (Replication.IsRunning() && !Replication.IsServer())
			return 0;

		LF_JobsConfig config = LF_JobsConfig.Get();
		LF_JobXpAwardDef award;
		if (config)
			award = config.FindAward(action);

		if (!award)
		{
			LF_Debug.Warn(FEATURE, string.Format("xp skipped: action '%1' has no xpAwards row in the jobs config", action));
			return 0;
		}

		if (playerUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("xp skipped: action '%1' awarded to a body with no player id", action));
			return 0;
		}

		if (units <= 0 || award.m_iXp <= 0)
		{
			LF_Debug.Info(FEATURE, string.Format("xp skipped: action '%1' for %2 is worth nothing (%3 unit(s) of %4 xp)", action, playerUid, units, award.m_iXp));
			return 0;
		}

		float now = LF_JobGuards.Now();
		string key = playerUid + "|" + action;
		if (!s_mLastAward)
			s_mLastAward = new map<string, float>();

		if (award.m_fCooldownSeconds > 0 && s_mLastAward.Contains(key))
		{
			float since = now - s_mLastAward.Get(key);
			if (since < award.m_fCooldownSeconds)
			{
				LF_Debug.Info(FEATURE, string.Format("xp on cooldown: %1 %2 again after %3 s (%4 s since the last)", playerUid, action, award.m_fCooldownSeconds, since));
				return 0;
			}
		}

		// The cooldown starts only when XP was granted: a capped or refused award leaves it open.
		int granted = AddXp(playerUid, award.m_sJob, award.m_iXp * units, action);
		if (granted > 0)
			s_mLastAward.Set(key, now);

		return granted;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The part of `amount` the job's daily cap still allows, and books it.
	protected static int CapToday(string playerUid, notnull LF_JobTrackDef track, int amount)
	{
		if (track.m_iDailyXpCap <= 0)
			return amount;

		if (!s_mEarnedToday)
			s_mEarnedToday = new map<string, int>();
		if (!s_mCapDay)
			s_mCapDay = new map<string, int>();

		string key = playerUid + "|" + track.m_sJob;
		int today = Math.Floor(LF_JobGuards.Now() / DAY_SECONDS);
		if (!s_mCapDay.Contains(key) || s_mCapDay.Get(key) != today)
		{
			s_mCapDay.Set(key, today);
			s_mEarnedToday.Set(key, 0);
		}

		int earned = s_mEarnedToday.Get(key);
		int granted = track.m_iDailyXpCap - earned;
		if (amount < granted)
			granted = amount;
		if (granted <= 0)
			return 0;

		s_mEarnedToday.Set(key, earned + granted);
		return granted;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetXp(string playerUid, string job, int xp)
	{
		map<string, int> jobs = Store().Get(playerUid);
		if (!jobs)
		{
			jobs = new map<string, int>();
			Store().Set(playerUid, jobs);
		}

		jobs.Set(job, Math.Max(0, xp));
	}

	//------------------------------------------------------------------------------------------------
	//! Drops a character's XP in one job (LF_CivilianJob.ForgetCareer).
	static void ForgetJob(string playerUid, string job)
	{
		if (!s_mXp)
			return;

		map<string, int> jobs = s_mXp.Get(playerUid);
		if (jobs)
			jobs.Remove(job);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops a character's XP and its session clocks (test cleanup, admin wipe).
	static void Forget(string playerUid)
	{
		if (s_mXp)
			s_mXp.Remove(playerUid);

		ForgetTimes(s_mLastAward, playerUid);
		ForgetCounts(s_mEarnedToday, playerUid);
		ForgetCounts(s_mCapDay, playerUid);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ForgetTimes(map<string, float> clocks, string playerUid)
	{
		if (!clocks)
			return;

		array<string> keys = {};
		foreach (string key, float value : clocks)
		{
			if (key.StartsWith(playerUid + "|"))
				keys.Insert(key);
		}

		foreach (string key : keys)
		{
			clocks.Remove(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void ForgetCounts(map<string, int> clocks, string playerUid)
	{
		if (!clocks)
			return;

		array<string> keys = {};
		foreach (string key, int value : clocks)
		{
			if (key.StartsWith(playerUid + "|"))
				keys.Insert(key);
		}

		foreach (string key : keys)
		{
			clocks.Remove(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Every character key that holds XP (LF_Progression.ExportAll mirrors their legacy fields).
	static void CollectUids(notnull array<string> uids)
	{
		if (!s_mXp)
			return;

		foreach (string uid, map<string, int> jobs : s_mXp)
		{
			if (uids.Find(uid) < 0)
				uids.Insert(uid);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: one row per (character, job) with XP above 0.
	static array<ref LF_JobXpRow> ExportAll()
	{
		array<ref LF_JobXpRow> rows = new array<ref LF_JobXpRow>();
		if (!s_mXp)
			return rows;

		foreach (string uid, map<string, int> jobs : s_mXp)
		{
			if (!jobs)
				continue;

			foreach (string job, int xp : jobs)
			{
				if (xp > 0)
					rows.Insert(LF_JobXpRow.Create(uid, job, xp));
			}
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: replaces the store with the saved rows (idempotent: a second load gives the
	//! same XP). A row with no character or job, or negative XP, is skipped; a repeated
	//! (character, job) keeps the larger XP. A row for a job the config does not list is kept, so a
	//! track a pack drops for a while comes back with its XP, and reads as level 1 meanwhile.
	//! The session cooldown and cap clocks are left alone.
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_JobXpRow> rows)
	{
		s_mXp = new map<string, ref map<string, int>>();
		if (!rows)
			return 0;

		int applied = 0;
		int unknownJobs = 0;
		foreach (LF_JobXpRow row : rows)
		{
			if (!row || row.m_sPlayerUid.IsEmpty() || row.m_sJob.IsEmpty() || row.m_iXp < 0)
			{
				LF_Debug.Warn(FEATURE, "load skipped a job xp row with no character, no job or negative xp");
				continue;
			}

			if (!Track(row.m_sJob))
				unknownJobs++;

			int xp = Math.Max(row.m_iXp, XpOf(row.m_sPlayerUid, row.m_sJob));
			SetXp(row.m_sPlayerUid, row.m_sJob, xp);
			applied++;
		}

		if (unknownJobs > 0)
			LF_Debug.Warn(FEATURE, string.Format("load kept %1 job xp row(s) for jobs the config does not list; they read as level 1", unknownJobs));

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the store with what a "progression" payload's single level and delivery career
	//! hold: the single level's total XP onto CIVILIAN (quests, fruit picking and debug grants all
	//! fed that one number, so it cannot be split by job), the career's onto DELIVERY. The
	//! "progression" payload calls it on every load; a save with the "joblevels" payload then
	//! replaces the result, so only a version 7-27 save keeps it. Idempotent.
	//! \return Characters seeded.
	static int SeedFromLegacy(notnull array<ref LF_ProgressionRecord> records)
	{
		s_mXp = new map<string, ref map<string, int>>();
		int seeded = 0;
		foreach (LF_ProgressionRecord record : records)
		{
			if (!record || record.m_sPlayerUid.IsEmpty())
				continue;

			bool any = false;
			if (record.m_bHasLevel)
			{
				int civilian = LegacyLevelToXp(record.m_iPlayerLevel, record.m_fPlayerExperience);
				if (civilian > 0)
				{
					SetXp(record.m_sPlayerUid, JOB_CIVILIAN, civilian);
					any = true;
				}
			}

			int delivery = LegacyCareerToXp(record.m_iCareerLevel, record.m_fCareerExperience);
			if (delivery > 0)
			{
				SetXp(record.m_sPlayerUid, JOB_DELIVERY, delivery);
				any = true;
			}

			if (any)
				seeded++;
		}

		return seeded;
	}

	//------------------------------------------------------------------------------------------------
	//! The pre-#749 single level as total XP: each level cost level x 100 to the next, so level L
	//! with `xp` toward the next holds 50 L (L - 1) + xp.
	static int LegacyLevelToXp(int level, float xp)
	{
		int clamped = Math.ClampInt(level, 1, LEGACY_MAX_SINGLE_LEVEL);
		int total = 0;
		for (int i = 1; i < clamped; i++)
		{
			total = total + i * LEGACY_LEVEL_STEP;
		}

		return total + WholeXp(xp);
	}

	//------------------------------------------------------------------------------------------------
	//! Total XP back to the pre-#749 single level and the XP toward its next level.
	static void XpToLegacyLevel(int total, out int level, out float xp)
	{
		level = 1;
		int left = Math.Max(0, total);
		while (level < LEGACY_MAX_SINGLE_LEVEL && left >= level * LEGACY_LEVEL_STEP)
		{
			left = left - level * LEGACY_LEVEL_STEP;
			level++;
		}

		xp = left;
	}

	//------------------------------------------------------------------------------------------------
	//! The pre-#749 career curve: XP from level 1 to 2 and the growth of each later step, from the
	//! jobs config (m_iJobXpFirstLevel, m_fJobXpLevelGrowth), the values the old career used.
	static void LegacyCareerCurve(out float first, out float growth)
	{
		first = LF_JobsConfig.DEFAULT_JOB_XP_FIRST_LEVEL;
		growth = LF_JobsConfig.DEFAULT_JOB_XP_LEVEL_GROWTH;
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (config)
		{
			first = config.m_iJobXpFirstLevel;
			growth = config.m_fJobXpLevelGrowth;
		}

		// The old career clamped them the same way (LF_CivilianJob.XpForLevel before #749).
		first = Math.Max(1, first);
		growth = Math.Max(1, growth);
	}

	//------------------------------------------------------------------------------------------------
	//! The pre-#749 delivery career as total XP: the steps of LegacyCareerCurve up to the level,
	//! plus the XP toward the next one.
	static int LegacyCareerToXp(int level, float xp)
	{
		float first;
		float growth;
		LegacyCareerCurve(first, growth);
		int clamped = Math.ClampInt(level, 1, LEGACY_MAX_LEVEL);
		float total = 0;
		float step = first;
		for (int i = 1; i < clamped; i++)
		{
			total = total + Math.Round(step);
			step = step * growth;
		}

		total = total + WholeXp(xp);
		if (total > LEGACY_MAX_XP)
			return LEGACY_MAX_XP;

		int whole = total;
		return whole;
	}

	//------------------------------------------------------------------------------------------------
	//! Saved legacy XP toward the next level, rounded and never negative.
	protected static int WholeXp(float xp)
	{
		int whole = Math.Round(xp);
		if (whole < 0)
			return 0;

		return whole;
	}

	//------------------------------------------------------------------------------------------------
	//! Total XP back to the pre-#749 career level and the XP toward its next level.
	static void XpToLegacyCareer(int total, out int level, out float xp)
	{
		float first;
		float growth;
		LegacyCareerCurve(first, growth);
		level = 1;
		float left = Math.Max(0, total);
		float step = first;
		while (level < LEGACY_MAX_LEVEL && left >= Math.Round(step))
		{
			left = left - Math.Round(step);
			step = step * growth;
			level++;
		}

		xp = left;
	}
}
