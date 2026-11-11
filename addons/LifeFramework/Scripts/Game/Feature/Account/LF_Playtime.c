//------------------------------------------------------------------------------------------------
//! The running clock of one connected player. Session-only, never saved: the clock restarts
//! when the player connects again.
class LF_PlaytimeClock
{
	string m_sAccountUid;
	//! System.GetUnixTime() of the last credit.
	int m_iLastUnix;
	//! The job the last credit went to: the disconnect credit falls back to it.
	string m_sLastJob;
}

//------------------------------------------------------------------------------------------------
//! Holds the game mode's disconnect subscription (a static function is not inserted into an invoker).
class LF_PlaytimeDisconnectListener
{
	//------------------------------------------------------------------------------------------------
	void OnPlayerDisconnected(int playerId, KickCauseCode cause = KickCauseCode.NONE, int timeout = -1)
	{
		LF_Playtime.StopClock(playerId, System.GetUnixTime());
	}
}

//------------------------------------------------------------------------------------------------
//! Playtime per account and per job or service (#692), counted on the server. Never replicated.
//!
//! Every connected player with an account uid has a clock. Every TICK_SECONDS, on disconnect and
//! just before a save, the seconds since the last credit go to the job the account holds now
//! (JobFor): an on-duty role (POLICE, MEDIC), else a delivery job it is signed up for, else
//! CIVILIAN. Job and duty state are read through accessors only (LF_PlayerAccount,
//! LF_CivilianJob.GetSignedUpJob); Police and Jobs know nothing of this class.
//!
//! Keyed by the account uid, not the character key, so a gate such as "minimum hours to apply for
//! police" reads the person, whichever character they play. Total(uid) is the sum of the jobs.
//! Staff tools and config gates read Total / JobSeconds. Persisted as the "playtime" payload.
class LF_Playtime
{
	static const string FEATURE = "Playtime";
	static const string JOB_CIVILIAN = "CIVILIAN";
	//! How often online clocks are credited. A duty change is attributed to within this window.
	static const int TICK_SECONDS = 15;
	//! Most seconds one credit may add: a wall-clock jump (NTP, a suspended host) is not playtime.
	static const int MAX_CREDIT_SECONDS = 300;

	//! Key: account uid + "|" + job.
	protected static ref map<string, ref LF_PlaytimeRecord> s_mRows;
	//! Key: player id.
	protected static ref map<int, ref LF_PlaytimeClock> s_mClocks;
	protected static ref LF_PlaytimeDisconnectListener s_Listener;
	protected static bool s_bRunning;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_PlaytimeRecord> Rows()
	{
		if (!s_mRows)
			s_mRows = new map<string, ref LF_PlaytimeRecord>();

		return s_mRows;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<int, ref LF_PlaytimeClock> Clocks()
	{
		if (!s_mClocks)
			s_mClocks = new map<int, ref LF_PlaytimeClock>();

		return s_mClocks;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Key(string accountUid, string job)
	{
		return accountUid + "|" + job;
	}

	//------------------------------------------------------------------------------------------------
	//! World end: stops the clock and drops the counts (LF_StaticReset).
	static void ResetStatics()
	{
		if (s_bRunning && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		SCR_BaseGameMode gameMode;
		if (GetGame())
			gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode && s_Listener)
			gameMode.GetOnPlayerDisconnected().Remove(s_Listener.OnPlayerDisconnected);

		s_bRunning = false;
		s_Listener = null;
		s_mClocks = null;
		s_mRows = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: starts the clock once. Called when the game mode starts.
	static bool Start()
	{
		if (!Replication.IsServer())
			return false;

		if (s_bRunning)
			return true;

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gameMode)
		{
			LF_Debug.Error(FEATURE, "not started: no game mode, playtime is not counted");
			return false;
		}

		s_Listener = new LF_PlaytimeDisconnectListener();
		gameMode.GetOnPlayerDisconnected().Insert(s_Listener.OnPlayerDisconnected);
		GetGame().GetCallqueue().CallLater(Tick, TICK_SECONDS * 1000, true);
		s_bRunning = true;
		LF_Debug.Info(FEATURE, string.Format("counting playtime every %1 s", TICK_SECONDS));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRunning()
	{
		return s_bRunning;
	}

	//------------------------------------------------------------------------------------------------
	//! Credits every connected player, starts clocks for new ones and drops clocks of players who
	//! left without a disconnect event (uncredited: the last tick already paid them).
	static void Tick()
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		int now = System.GetUnixTime();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (Clocks().Contains(playerId))
				Settle(playerId, now);
			else
				StartClock(playerId, LF_Utils.GetAccountUID(playerId), now);
		}

		array<int> gone = {};
		foreach (int clockId, LF_PlaytimeClock clock : Clocks())
		{
			if (!playerIds.Contains(clockId))
				gone.Insert(clockId);
		}

		foreach (int goneId : gone)
		{
			LF_Debug.Warn(FEATURE, string.Format("clock of player %1 dropped: left without a disconnect event", goneId));
			Clocks().Remove(goneId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Starts counting for a player. Nothing is credited until the next settle.
	//! \return False when the player has no account uid yet (the next tick tries again).
	static bool StartClock(int playerId, string accountUid, int nowUnix)
	{
		if (accountUid.IsEmpty())
			return false;

		LF_PlaytimeClock clock = new LF_PlaytimeClock();
		clock.m_sAccountUid = accountUid;
		clock.m_iLastUnix = nowUnix;
		clock.m_sLastJob = JobOf(accountUid, JOB_CIVILIAN);
		Clocks().Set(playerId, clock);
		LF_Debug.Log(FEATURE, string.Format("clock started for player %1 (%2)", playerId, accountUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Credits a player's last stretch and stops the clock (disconnect).
	//! \return Seconds credited.
	static int StopClock(int playerId, int nowUnix)
	{
		if (!s_mClocks || !s_mClocks.Contains(playerId))
			return 0;

		int credited = Settle(playerId, nowUnix);
		s_mClocks.Remove(playerId);
		return credited;
	}

	//------------------------------------------------------------------------------------------------
	//! Credits the seconds since the player's last credit to the job they hold now.
	//! \return Seconds credited.
	static int Settle(int playerId, int nowUnix)
	{
		LF_PlaytimeClock clock = Clocks().Get(playerId);
		if (!clock)
			return 0;

		int seconds = nowUnix - clock.m_iLastUnix;
		clock.m_iLastUnix = nowUnix;
		if (seconds <= 0)
			return 0;

		if (seconds > MAX_CREDIT_SECONDS)
		{
			LF_Debug.Warn(FEATURE, string.Format("%1: %2 s since the last credit is a clock jump, credited %3 s", clock.m_sAccountUid, seconds, MAX_CREDIT_SECONDS));
			seconds = MAX_CREDIT_SECONDS;
		}

		clock.m_sLastJob = JobOf(clock.m_sAccountUid, clock.m_sLastJob);
		Credit(clock.m_sAccountUid, clock.m_sLastJob, seconds);
		return seconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Credits every running clock up to now. Called before each save so it holds the time so far.
	//! \return Clocks settled.
	static int SettleAll()
	{
		if (!s_mClocks)
			return 0;

		int now = System.GetUnixTime();
		array<int> ids = {};
		foreach (int playerId, LF_PlaytimeClock clock : s_mClocks)
		{
			ids.Insert(playerId);
		}

		foreach (int id : ids)
		{
			Settle(id, now);
		}

		return ids.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The job of a resident account, or the fallback when it is not in the cache (a player who
	//! is leaving: the last stretch goes to the job of the last credit).
	protected static string JobOf(string accountUid, string fallback)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return fallback;

		LF_PlayerAccount account = accounts.GetFromCache(accountUid);
		if (!account)
			return fallback;

		return JobFor(account);
	}

	//------------------------------------------------------------------------------------------------
	//! The job an account's time counts toward now: its on-duty role (the LF_Faction name, POLICE or
	//! MEDIC), else the delivery job its active character is signed up for (the LF_EJobType name),
	//! else CIVILIAN.
	static string JobFor(notnull LF_PlayerAccount account)
	{
		if (account.IsOnDuty() && account.GetFaction() != LF_Faction.CIVILIAN)
			return typename.EnumToString(LF_Faction, account.GetFaction());

		LF_EJobType job = LF_CivilianJob.GetSignedUpJob(account.GetActiveCharacterKey());
		if (job != LF_EJobType.UNEMPLOYED)
			return typename.EnumToString(LF_EJobType, job);

		return JOB_CIVILIAN;
	}

	//------------------------------------------------------------------------------------------------
	//! The one write path: adds seconds to an account's job.
	//! \return False (and a warning) for a missing account or job or a non-positive amount.
	static bool Credit(string accountUid, string job, int seconds)
	{
		if (accountUid.IsEmpty() || job.IsEmpty() || seconds <= 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("not credited: account '%1', job '%2', %3 s", accountUid, job, seconds));
			return false;
		}

		string key = Key(accountUid, job);
		LF_PlaytimeRecord row = Rows().Get(key);
		if (!row)
		{
			row = LF_PlaytimeRecord.Create(accountUid, job, 0);
			Rows().Set(key, row);
			LF_Debug.Info(FEATURE, string.Format("%1 starts counting time as %2", accountUid, job));
		}

		row.m_iSeconds = row.m_iSeconds + seconds;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Seconds the account has played in one job (0 when never).
	static int JobSeconds(string accountUid, string job)
	{
		if (!s_mRows)
			return 0;

		LF_PlaytimeRecord row = s_mRows.Get(Key(accountUid, job));
		if (!row)
			return 0;

		return row.m_iSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Seconds the account has played in every job together.
	static int Total(string accountUid)
	{
		int total = 0;
		if (!s_mRows)
			return total;

		foreach (string key, LF_PlaytimeRecord row : s_mRows)
		{
			if (row.m_sAccountUid == accountUid)
				total += row.m_iSeconds;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Fills jobs with every job the account has time in (staff tools).
	static void CollectJobs(string accountUid, notnull array<string> jobs)
	{
		jobs.Clear();
		if (!s_mRows)
			return;

		foreach (string key, LF_PlaytimeRecord row : s_mRows)
		{
			if (row.m_sAccountUid == accountUid)
				jobs.Insert(row.m_sJob);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: credits the online clocks up to now, then copies every row.
	static array<ref LF_PlaytimeRecord> ExportAll()
	{
		SettleAll();
		array<ref LF_PlaytimeRecord> records = new array<ref LF_PlaytimeRecord>();
		if (!s_mRows)
			return records;

		foreach (string key, LF_PlaytimeRecord row : s_mRows)
		{
			records.Insert(row.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the counts with the saved ones (idempotent: a second load gives the same counts).
	//! Rows with no account, job or time are skipped; duplicate keys are summed. Running clocks are
	//! left alone: at a boot load none run yet, and on a live re-apply they keep counting from their
	//! last credit.
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_PlaytimeRecord> records)
	{
		s_mRows = new map<string, ref LF_PlaytimeRecord>();
		if (!records)
			return 0;

		int applied = 0;
		foreach (LF_PlaytimeRecord record : records)
		{
			if (!record || record.m_sAccountUid.IsEmpty() || record.m_sJob.IsEmpty() || record.m_iSeconds <= 0)
			{
				LF_Debug.Warn(FEATURE, "load skipped a row with no account, job or time");
				continue;
			}

			string key = Key(record.m_sAccountUid, record.m_sJob);
			LF_PlaytimeRecord row = s_mRows.Get(key);
			if (row)
				row.m_iSeconds = row.m_iSeconds + record.m_iSeconds;
			else
				s_mRows.Set(key, record.Copy());

			applied++;
		}

		return applied;
	}
}
