//------------------------------------------------------------------------------------------------
//! One heist run at a site (#3411): binds the three LF_HeistStages stages to the world. Stage one
//! is the site's breach door (LF_OnBreachOpened for the door id), stage two the vault drill
//! (LF_OnVaultBreach "opened" for the vault id), stage three the getaway: every crew member is
//! outside the site radius. The door opening starts the run and its clock (the breach arm rings
//! the alarm, LF_Breach.Arm); a vault opened before the door does nothing. The clock running out or
//! the crew gone fails it. Server only; it listens on the bus while Listen() is in force.
//!
//! A configured job (FromRow) is a big job (#4437, plan 0.3.0 Crime A6): its door opening takes
//! the server-wide LF_BigJobLock under the row's id, the run ending (paid out or failed) or being
//! stopped mid-run releases it and starts the job's cooldown, and ArmRefusal refuses arming its door
//! while another big job runs or the job cools down. A lost run frees the lock after
//! LF_BigJobLock.MAX_RUN_SECONDS. A run built from attributes (Create) takes no lock.
class LF_HeistRun : LF_EventSubscriber
{
	static const string FEATURE = "Heist";
	static const int STAGE_DOOR = 0;
	static const int STAGE_VAULT = 1;
	static const int STAGE_GETAWAY = 2;
	static const string NEED_POLICE = "#LF-Heist_NeedPolice";

	//! Tests set a fixed world time in ms; -1 reads the world clock.
	static int s_iNowOverride = -1;

	protected ref LF_HeistStages m_Stages;
	protected ref array<string> m_aCrew = {};
	protected string m_sSiteId;
	protected string m_sDoorId;
	protected string m_sVaultId;
	protected vector m_vCentre;
	protected float m_fRadius;
	protected int m_iClockMs;
	protected bool m_bAlarm;
	protected int m_iMinPolice;
	protected bool m_bClerkHandsUp;
	//! The big-job lock key (the heist row id); empty for a run that is not a configured job.
	protected string m_sJobKey;
	protected int m_iCooldownSeconds;
	//! True while this run holds the big-job lock.
	protected bool m_bHoldsLock;

	//! Throttles the server's refusal log: ArmRefusal also runs for every action-menu frame on a host.
	protected static string s_sLastRefusalLog;
	protected static int s_iLastRefusalLogAt = -1;
	protected static const int REFUSAL_LOG_EVERY_SECONDS = 10; // lf-fixed: log throttle, not tuning

	//------------------------------------------------------------------------------------------------
	//! \param clockMs the whole run's time limit from the door opening; 0 or less means none.
	//! \return null when an id is empty or the radius is not positive (logged).
	static LF_HeistRun Create(string siteId, string doorId, string vaultId, vector centre, float radius, int clockMs)
	{
		if (siteId.IsEmpty() || doorId.IsEmpty() || vaultId.IsEmpty() || radius <= 0)
		{
			LF_Debug.Error(FEATURE, string.Format("heist run %1 refused: door '%2', vault '%3', radius %4", siteId, doorId, vaultId, radius));
			return null;
		}

		LF_HeistRun run = new LF_HeistRun();
		run.m_sSiteId = siteId;
		run.m_sDoorId = doorId;
		run.m_sVaultId = vaultId;
		run.m_vCentre = centre;
		run.m_fRadius = radius;
		run.m_iClockMs = clockMs;
		run.m_Stages = new LF_HeistStages();
		run.m_Stages.AddStage(LF_EHeistGate.TARGET_OPENED, 0);
		run.m_Stages.AddStage(LF_EHeistGate.TARGET_OPENED, 0);
		run.m_Stages.AddStage(LF_EHeistGate.CREW_IN_AREA, 0);
		return run;
	}

	//------------------------------------------------------------------------------------------------
	//! A run built from a configured heist job (#3748, LF_HeistConfig): the door and vault stages
	//! name the target ids, the getaway stage the radius (0 keeps \p fallbackRadius), the row the
	//! clock, the police minimum and the clerk hands-up flag.
	//! \return null (logged) when the row cannot run or lacks a door, vault or getaway stage.
	static LF_HeistRun FromRow(string siteId, LF_HeistSiteRow row, vector centre, float fallbackRadius)
	{
		if (!row)
		{
			LF_Debug.Error(FEATURE, string.Format("heist run %1 refused: no heist row", siteId));
			return null;
		}

		string problem = row.Problem();
		LF_HeistStageRow door = row.FindStage(LF_HeistKinds.DOOR);
		LF_HeistStageRow vault = row.FindStage(LF_HeistKinds.VAULT);
		LF_HeistStageRow getaway = row.FindStage(LF_HeistKinds.GETAWAY);
		if (problem.IsEmpty() && (!door || !vault || !getaway))
			problem = string.Format("heist site '%1' needs a door, a vault and a getaway stage", row.m_sId);

		if (!problem.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("heist run %1 refused: %2", siteId, problem));
			return null;
		}

		float radius = fallbackRadius;
		if (getaway.m_fRadius > 0)
			radius = getaway.m_fRadius;

		LF_HeistRun run = Create(siteId, door.m_sTargetId, vault.m_sTargetId, centre, radius, row.m_iClockSeconds * 1000);
		if (!run)
			return null;

		run.m_iMinPolice = row.m_iMinPoliceOnDuty;
		run.m_bClerkHandsUp = door.m_bClerkHandsUp;
		run.m_sJobKey = row.m_sId;
		run.m_iCooldownSeconds = CooldownSeconds(row);
		return run;
	}

	//------------------------------------------------------------------------------------------------
	//! The big-job cooldown a configured job starts when it ends. LF_HeistSiteRow has no cooldown
	//! field yet, so every heist job takes the bank's plan default (3 h); the pharmacy takes the lock
	//! in its own code with its own cooldown.
	static int CooldownSeconds(LF_HeistSiteRow row)
	{
		return LF_BigJobLock.COOLDOWN_BANK;
	}

	//------------------------------------------------------------------------------------------------
	//! Why arming the breach on \p targetId is refused for a configured heist job: the string key of
	//! the refusal when the target is a job's door and fewer than its police minimum are on duty,
	//! else empty. The offer (client, replicated count) and the perform (server, live count) both ask.
	static string ArmRefusal(string targetId, int policeOnDuty)
	{
		if (targetId.IsEmpty())
			return string.Empty;

		LF_HeistConfig config = LF_HeistConfig.Get();
		if (!config || !config.m_aSites)
			return string.Empty;

		foreach (LF_HeistSiteRow row : config.m_aSites)
		{
			string refusal = RowArmRefusal(row, targetId, policeOnDuty, LF_BigJobLock.Now());
			if (!refusal.IsEmpty())
				return refusal;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! ArmRefusal for one job row: NEED_POLICE under the police minimum; then the big-job refusal
	//! (LF_BigJobLock.RefusalKey for another job running, the cooldown in words with its minutes) when
	//! the lock would refuse this job. Empty when \p targetId is not the row's door or arming may go on.
	//! Pure: it never takes the lock (the door opening does). The lock lives on the server, so the
	//! client's offer passes and the server's perform decides.
	static string RowArmRefusal(LF_HeistSiteRow row, string targetId, int policeOnDuty, int nowSeconds)
	{
		if (!row || targetId.IsEmpty())
			return string.Empty;

		LF_HeistStageRow door = row.FindStage(LF_HeistKinds.DOOR);
		if (!door || door.m_sTargetId != targetId)
			return string.Empty;

		if (!LF_Bank.PoliceOnDutyAllows(policeOnDuty, row.m_iMinPoliceOnDuty))
			return NEED_POLICE;

		string running = LF_BigJobLock.RunningJob(nowSeconds);
		if (!running.IsEmpty())
		{
			LogRefusal(row.m_sId, string.Format("arming %1 refused: big job %2 is running", row.m_sId, running), nowSeconds);
			return LF_BigJobLock.RefusalKey(LF_EBigJobLock.BUSY);
		}

		int left = LF_BigJobLock.CooldownLeft(row.m_sId, nowSeconds);
		if (left > 0)
		{
			LogRefusal(row.m_sId, string.Format("arming %1 refused: cooling down, %2 s left", row.m_sId, left), nowSeconds);
			return LF_BigJobLock.RefusalText(LF_EBigJobLock.COOLDOWN, row.m_sId, nowSeconds);
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected static void LogRefusal(string jobKey, string message, int nowSeconds)
	{
		if (jobKey == s_sLastRefusalLog && s_iLastRefusalLogAt >= 0 && nowSeconds - s_iLastRefusalLogAt < REFUSAL_LOG_EVERY_SECONDS)
			return;

		s_sLastRefusalLog = jobKey;
		s_iLastRefusalLogAt = nowSeconds;
		LF_Debug.Info(LF_BigJobLock.FEATURE, message);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iNowOverride = -1;
		s_sLastRefusalLog = string.Empty;
		s_iLastRefusalLogAt = -1;
	}

	//------------------------------------------------------------------------------------------------
	static int Now()
	{
		if (s_iNowOverride >= 0)
			return s_iNowOverride;

		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribe to the door and vault events. \return false when either subscription failed.
	bool Listen()
	{
		bool door = LF_EventBus.IsSubscribed(LF_OnBreachOpened, this) || LF_EventBus.Subscribe(LF_OnBreachOpened, this);
		bool vault = LF_EventBus.IsSubscribed(LF_OnVaultBreach, this) || LF_EventBus.Subscribe(LF_OnVaultBreach, this);
		return door && vault;
	}

	//------------------------------------------------------------------------------------------------
	void Stop()
	{
		LF_EventBus.Unsubscribe(LF_OnBreachOpened, this);
		LF_EventBus.Unsubscribe(LF_OnVaultBreach, this);
		if (m_bHoldsLock)
		{
			LF_Debug.Info(FEATURE, string.Format("%1: run stopped mid-way; the big-job lock is released", m_sSiteId));
			ReleaseLock();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ends this run's hold on the big-job lock and starts the job's cooldown.
	protected void ReleaseLock()
	{
		if (!m_bHoldsLock)
			return;

		m_bHoldsLock = false;
		LF_BigJobLock.Finish(m_sJobKey, LF_BigJobLock.Now());
	}

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "LF_HeistRun:" + m_sSiteId;
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnBreachOpened opened = LF_OnBreachOpened.Cast(busEvent);
		if (opened)
		{
			if (opened.m_sTargetId == m_sDoorId)
				DoorOpened(opened.m_sPlayerUid, Now());

			return true;
		}

		LF_OnVaultBreach breach = LF_OnVaultBreach.Cast(busEvent);
		if (breach && breach.m_sVaultId == m_sVaultId && breach.m_sOutcome == "opened")
			VaultOpened(breach.m_sPlayerUid, Now());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Stage one: the door opened. Starts the run and its clock. \return true when the run advanced.
	bool DoorOpened(string playerUid, int nowMs)
	{
		if (m_Stages.GetState() == LF_EHeistState.IDLE)
		{
			if (!m_sJobKey.IsEmpty())
			{
				LF_EBigJobLock lockResult = LF_BigJobLock.TryBegin(m_sJobKey, playerUid, m_iCooldownSeconds, LF_BigJobLock.Now());
				if (lockResult != LF_EBigJobLock.OK)
				{
					LF_Debug.Info(LF_BigJobLock.FEATURE, string.Format("%1: run not started by %2: the big-job lock refused (%3)", m_sSiteId, playerUid, typename.EnumToString(LF_EBigJobLock, lockResult)));
					return false;
				}

				m_bHoldsLock = true;
			}

			if (!m_Stages.Start(nowMs, m_iClockMs))
			{
				ReleaseLock();
				return false;
			}

			m_bAlarm = true;
			LF_Debug.Info(FEATURE, string.Format("%1: run started by %2 at the door %3; alarm raised; clerk may raise hands: %4", m_sSiteId, playerUid, m_sDoorId, m_bClerkHandsUp));
		}

		AddCrew(playerUid);
		return m_Stages.TargetOpened(STAGE_DOOR, nowMs);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage two: the vault drill finished. Ignored unless the door stage is done.
	bool VaultOpened(string playerUid, int nowMs)
	{
		if (!m_Stages.TargetOpened(STAGE_VAULT, nowMs))
		{
			LF_Debug.Info(FEATURE, string.Format("%1: vault %2 opened at stage %3 (state %4); the run ignores it", m_sSiteId, m_sVaultId, m_Stages.GetStage(), m_Stages.GetState()));
			return false;
		}

		AddCrew(playerUid);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Periodic check with the live crew bodies' positions (a dead or logged-out member is left out).
	//! The clock or an empty crew fails the run; every member outside the radius at the getaway stage
	//! completes it. \return true when the run finished (succeeded or failed) on this tick.
	bool Tick(int nowMs, array<vector> crewPositions)
	{
		if (m_Stages.GetState() != LF_EHeistState.RUNNING)
			return false;

		int alive;
		if (crewPositions)
			alive = crewPositions.Count();

		m_Stages.Tick(nowMs, alive);
		if (m_Stages.GetState() != LF_EHeistState.RUNNING)
			return Finished();

		if (m_Stages.GetStage() != STAGE_GETAWAY)
			return false;

		m_Stages.CrewInArea(STAGE_GETAWAY, AllOutside(crewPositions), nowMs);
		if (m_Stages.GetState() != LF_EHeistState.RUNNING)
			return Finished();

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool AllOutside(array<vector> positions)
	{
		if (!positions || positions.IsEmpty())
			return false;

		float radiusSq = m_fRadius * m_fRadius;
		foreach (vector pos : positions)
		{
			if (vector.DistanceSqXZ(pos, m_vCentre) < radiusSq)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Finished()
	{
		if (m_Stages.IsFailed())
			LF_Debug.Info(FEATURE, string.Format("%1: run failed (%2)", m_sSiteId, m_Stages.GetFailReason()));
		else
			LF_Debug.Info(FEATURE, string.Format("%1: run complete, crew of %2 got away", m_sSiteId, m_aCrew.Count()));

		ReleaseLock();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddCrew(string playerUid)
	{
		if (!playerUid.IsEmpty() && m_aCrew.Find(playerUid) < 0)
			m_aCrew.Insert(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	int GetState()
	{
		return m_Stages.GetState();
	}

	//------------------------------------------------------------------------------------------------
	int GetStage()
	{
		return m_Stages.GetStage();
	}

	//------------------------------------------------------------------------------------------------
	string GetFailReason()
	{
		return m_Stages.GetFailReason();
	}

	//------------------------------------------------------------------------------------------------
	bool IsAlarmRaised()
	{
		return m_bAlarm;
	}

	//------------------------------------------------------------------------------------------------
	string GetDoorId()
	{
		return m_sDoorId;
	}

	//------------------------------------------------------------------------------------------------
	string GetVaultId()
	{
		return m_sVaultId;
	}

	//------------------------------------------------------------------------------------------------
	float GetRadius()
	{
		return m_fRadius;
	}

	//------------------------------------------------------------------------------------------------
	int GetClockMs()
	{
		return m_iClockMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Police that must be on duty before the door may be armed (a configured job; 0 otherwise).
	int GetMinPolice()
	{
		return m_iMinPolice;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the job lets the clerk inside choose hands up at the door. Nothing acts on it until
	//! hands-up is a state a target chooses (#782); the run only carries the row's choice.
	bool ClerkMayHandsUp()
	{
		return m_bClerkHandsUp;
	}

	//------------------------------------------------------------------------------------------------
	//! Uids of the players who opened the door or the vault in this run.
	array<string> GetCrew()
	{
		return m_aCrew;
	}

	//------------------------------------------------------------------------------------------------
	string GetSiteId()
	{
		return m_sSiteId;
	}

	//------------------------------------------------------------------------------------------------
	//! The big-job lock key (the heist row id), empty for a run that takes no lock.
	string GetJobKey()
	{
		return m_sJobKey;
	}

	//------------------------------------------------------------------------------------------------
	bool HoldsBigJobLock()
	{
		return m_bHoldsLock;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_HeistRun's statics (LF_StaticReset finds it by reflection).
class LF_HeistRun_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_HeistRun";
	}

	override void Apply()
	{
		LF_HeistRun.ResetStatics();
	}
}
