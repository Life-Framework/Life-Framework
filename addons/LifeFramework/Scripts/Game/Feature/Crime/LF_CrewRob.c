//------------------------------------------------------------------------------------------------
//! A crew robbery of a till (#3404, parent #1024). A till marked for crew jobs (LF_RobAction
//! m_bCrewJob) is robbed in three steps: "Start the job" forms the crew from the armed civilians
//! close to the starter and sounds the alarm, the holder's "Hold the till" makes the take grow
//! while the holder stands on the spot, and "Take the money" ends the job and pays every crew
//! member present a share in physical cash (marked notes when dirty money is on). A lookout gets
//! a toast when an on-duty officer comes within the till's police radius. The numbers and the
//! pure rules are LF_CrewRobConfig / LF_CrewRobRules (#3401); this file is the server's job
//! state and judging. LOGIC and WORLD tested by LF_Test_CrewRob.
//------------------------------------------------------------------------------------------------
enum LF_ECrewRobEnd
{
	NONE,		//!< The job goes on
	GONE,		//!< The holder left the server, or the till is gone
	DOWNED,		//!< The holder went down, died or was cuffed
	IDLE		//!< The take did not grow for the configured idle time-out
}

//------------------------------------------------------------------------------------------------
//! One member of a crew: who, and the account uid the crime is booked against.
class LF_CrewRobMember : Managed
{
	EntityID m_Id = EntityID.INVALID;
	string m_sUid;

	//------------------------------------------------------------------------------------------------
	static LF_CrewRobMember Make(IEntity entity, string uid)
	{
		LF_CrewRobMember member = new LF_CrewRobMember();
		if (entity)
			member.m_Id = entity.GetID();

		member.m_sUid = uid;
		return member;
	}

	//------------------------------------------------------------------------------------------------
	IEntity Body()
	{
		if (m_Id == EntityID.INVALID || !GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(m_Id);
	}
}

//------------------------------------------------------------------------------------------------
//! A running crew robbery of one till. Server only; the first crew member is the holder.
class LF_CrewRobJob : Managed
{
	//! Weak: the till's action goes when its entity does, and the job then ends as GONE.
	LF_RobAction m_Till;
	ref array<ref LF_CrewRobMember> m_aCrew = {};
	vector m_vSpot;
	float m_fRadius;
	float m_fPoliceRadius;
	bool m_bHolding;
	float m_fSpotSeconds;
	//! Seconds since the take last grew; the job ends at LF_CrewRobConfig.m_fIdleTimeoutSeconds.
	float m_fIdleSeconds;
	bool m_bClosed;
	bool m_bPoliceNoted;
	int m_iLastTickMs;
	int m_iPaidTotal;

	//------------------------------------------------------------------------------------------------
	LF_CrewRobMember Holder()
	{
		if (m_aCrew.IsEmpty())
			return null;

		return m_aCrew[0];
	}

	//------------------------------------------------------------------------------------------------
	//! The take so far, from the seconds the holder stood on the spot.
	int Take()
	{
		LF_CrewRobConfig config = LF_CrewRobConfig.Get();
		return LF_CrewRobRules.Take(m_fSpotSeconds, config.m_iTakePerSecond, config.m_iTakeCap);
	}
}

//------------------------------------------------------------------------------------------------
//! The server's crew robberies.
class LF_CrewRob
{
	static const string FEATURE = "CrewRob";
	static const int TICK_MS = 250; // lf-fixed: judging cadence
	//! How far from the starter an armed civilian may stand to join the crew.
	static const float JOIN_RADIUS = 15.0; // lf-fixed: a shop's floor, not a balance value
	//! How far from the till a crew member may stand and still be paid a share.
	static const float PAY_RADIUS = 40.0; // lf-fixed: the block around a shop, not a balance value
	//! The spot radius when the till's action sets none.
	static const float DEFAULT_RADIUS = 6.0; // lf-fixed: a counter's reach, not a balance value
	static const string REFUSED_BUSY = "crew robbery refused: a job is already running at this till";
	static const string REFUSED_CREW = "crew robbery refused: not enough armed crew";
	static const string REFUSED_NOT_CREW_TILL = "crew robbery refused: this till is not marked for crew jobs";
	static const string REFUSED_NO_JOB = "crew robbery refused: no job is running here";
	static const string REFUSED_NOT_HOLDER = "crew robbery refused: only the holder may do that";
	static const string REFUSED_NOTHING = "crew robbery refused: nothing to take yet";
	static const string REFUSED_ALREADY_HOLDING = "crew robbery refused: the till is already held";

	protected static ref array<ref LF_CrewRobJob> s_aJobs;
	protected static bool s_bTicking;
	protected static string s_sLastRefusal;
	//! Cash the last finished take paid out, all shares together (test seam).
	protected static int s_iLastPaid;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aJobs = null;
		s_sLastRefusal = "";
		s_iLastPaid = 0;
		if (s_bTicking && GetGame())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_CrewRobJob> Jobs()
	{
		if (!s_aJobs)
			s_aJobs = {};

		return s_aJobs;
	}

	//------------------------------------------------------------------------------------------------
	static int ActiveCount()
	{
		return Jobs().Count();
	}

	//------------------------------------------------------------------------------------------------
	static string LastRefusal()
	{
		return s_sLastRefusal;
	}

	//------------------------------------------------------------------------------------------------
	static int LastPaid()
	{
		return s_iLastPaid;
	}

	//------------------------------------------------------------------------------------------------
	//! The refusal's toast key: the caption the dialogue robbery already has for the shared gates,
	//! or this feature's own.
	static string RefusalKey(string refusal)
	{
		if (refusal == REFUSED_BUSY)
			return "#LF-CrewRob_Refused_Busy";

		if (refusal == REFUSED_CREW)
			return "#LF-CrewRob_Refused_Crew";

		if (refusal == REFUSED_NO_JOB || refusal == REFUSED_NOT_CREW_TILL)
			return "#LF-CrewRob_Refused_NoJob";

		if (refusal == REFUSED_NOT_HOLDER)
			return "#LF-CrewRob_Refused_NotHolder";

		if (refusal == REFUSED_NOTHING)
			return "#LF-CrewRob_Refused_Nothing";

		if (refusal == REFUSED_ALREADY_HOLDING)
			return "#LF-CrewRob_Refused_Holding";

		return LF_RobHoldRules.RefusalKey(refusal);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_CrewRobJob FindByTill(LF_RobAction till)
	{
		foreach (LF_CrewRobJob job : Jobs())
		{
			if (job.m_Till == till)
				return job;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The running job of `till`, or null.
	static LF_CrewRobJob JobOf(LF_RobAction till)
	{
		if (!till)
			return null;

		return FindByTill(till);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: why a crew robbery of `till` cannot start now, or empty. Reads only, logs nothing.
	//! \param policeOnDuty On-duty police now. \param nowMs World time.
	static string StartRefusal(LF_RobAction till, notnull array<ref LF_CrewRobMember> crew, int policeOnDuty, float nowMs)
	{
		if (!till)
			return REFUSED_NOT_CREW_TILL;

		if (!LF_Systems.IsEnabled(LF_RobAction.SYSTEM))
			return LF_RobAction.REFUSED_OFF;

		if (FindByTill(till))
			return REFUSED_BUSY;

		if (!LF_CrewRobRules.CrewMeets(crew.Count(), LF_CrewRobConfig.Get().m_iMinCrew))
			return REFUSED_CREW;

		// The holder carries the cooldown, grace, civilian and police gates of the single-till robbery.
		string refusal = till.CrewGate(crew[0].m_sUid, policeOnDuty, nowMs);
		if (!refusal.IsEmpty())
			return refusal;

		for (int i = 1; i < crew.Count(); i++)
		{
			if (!LF_RobAction.IsFactionAllowedToRob(crew[i].m_sUid))
				return LF_RobAction.REFUSED_NOT_CIVILIAN;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: start the job. The first of `crew` is the holder. Starts the till's cooldown, books
	//! the crime on every member and sounds the alarm once to dispatch. \return The refusal
	//! (logged), or empty when the job began.
	static string Start(LF_RobAction till, vector spot, notnull array<ref LF_CrewRobMember> crew, int policeOnDuty)
	{
		float nowMs = 0;
		if (GetGame() && GetGame().GetWorld())
			nowMs = GetGame().GetWorld().GetWorldTime();

		string refusal = StartRefusal(till, crew, policeOnDuty, nowMs);
		if (!refusal.IsEmpty())
		{
			s_sLastRefusal = refusal;
			LF_Debug.Warn(FEATURE, refusal);
			return refusal;
		}

		LF_CrewRobJob job = new LF_CrewRobJob();
		job.m_Till = till;
		job.m_vSpot = spot;
		job.m_fRadius = till.GetHoldRadius();
		if (job.m_fRadius <= 0)
			job.m_fRadius = DEFAULT_RADIUS;

		job.m_fPoliceRadius = till.GetPoliceRadius();
		job.m_iLastTickMs = System.GetTickCount();
		foreach (LF_CrewRobMember member : crew)
		{
			job.m_aCrew.Insert(member);
		}

		till.MarkRobbed(nowMs);
		s_sLastRefusal = "";
		Report(job, policeOnDuty);
		Jobs().Insert(job);
		if (!s_bTicking && GetGame())
		{
			s_bTicking = true;
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		}

		LF_Debug.Info(FEATURE, string.Format("crew job begins at %1: %2 in the crew, holder uid %3", spot, crew.Count(), crew[0].m_sUid));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The crime is on record for every member, then one alarm and radio call for the job.
	protected static void Report(LF_CrewRobJob job, int policeOnDuty)
	{
		foreach (LF_CrewRobMember member : job.m_aCrew)
		{
			LF_PlayerAccount account = LF_RobAction.FindAccount(member.m_sUid);
			if (!account)
				continue;

			LF_CrimeCatalog.ApplyWanted(account, member.m_sUid, LF_WantedReports.OFFENCE_ROBBERY);
			LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
			if (accountManager)
				accountManager.SaveAndReleaseAccount(account);
		}

		LF_CrewRobMember holder = job.Holder();
		string alarmKind;
		if (job.m_Till)
			alarmKind = job.m_Till.GetAlarmKind();

		LF_Dispatch.Report(holder.m_sUid, holder.Body(), LF_WantedReports.OFFENCE_ROBBERY, job.m_vSpot, policeOnDuty, false, alarmKind);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the holder starts holding the till; the take grows while they stand on the spot.
	//! \return The refusal, or empty.
	static string Hold(LF_RobAction till, LF_CrewRobMember who)
	{
		LF_CrewRobJob job = JobOf(till);
		string refusal = HolderRefusal(job, who);
		if (refusal.IsEmpty() && job.m_bHolding)
			refusal = REFUSED_ALREADY_HOLDING;

		if (!refusal.IsEmpty())
		{
			s_sLastRefusal = refusal;
			return refusal;
		}

		job.m_bHolding = true;
		s_sLastRefusal = "";
		LF_Debug.Info(FEATURE, string.Format("crew job: the holder %1 holds the till", who.m_sUid));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static string HolderRefusal(LF_CrewRobJob job, LF_CrewRobMember who)
	{
		if (!job || job.m_bClosed)
			return REFUSED_NO_JOB;

		LF_CrewRobMember holder = job.Holder();
		if (!who || !holder || who.m_Id != holder.m_Id)
			return REFUSED_NOT_HOLDER;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one judging step of `dtSeconds`. The take grows only while the holder holds and
	//! stands on the spot; a lookout is warned when an officer comes within the police radius.
	//! \return What ended the job (it is closed and unlisted by Tick, not here), NONE while it goes on.
	static LF_ECrewRobEnd Step(LF_CrewRobJob job, float dtSeconds)
	{
		if (!job || job.m_bClosed)
			return LF_ECrewRobEnd.GONE;

		IEntity holder = job.Holder().Body();
		if (!holder || !job.m_Till)
			return LF_ECrewRobEnd.GONE;

		// Cuffed ends it like downed: the server decides, not the holder's client (#2417).
		if (LF_Downed.IsDowned(holder) || IsDead(holder) || LF_Restraint.IsRestrained(holder))
			return LF_ECrewRobEnd.DOWNED;

		bool onSpot = job.m_bHolding && vector.Distance(holder.GetOrigin(), job.m_vSpot) <= job.m_fRadius;
		int takeBefore = job.Take();
		job.m_fSpotSeconds = LF_CrewRobRules.AdvanceSpotSeconds(job.m_fSpotSeconds, dtSeconds, onSpot);
		if (job.Take() > takeBefore)
			job.m_fIdleSeconds = 0;
		else if (dtSeconds > 0)
			job.m_fIdleSeconds = job.m_fIdleSeconds + dtSeconds;

		// A holder who stands idle (or sits on a capped take) does not keep the till forever.
		float idleLimit = LF_CrewRobConfig.Get().m_fIdleTimeoutSeconds;
		if (idleLimit > 0 && job.m_fIdleSeconds >= idleLimit)
			return LF_ECrewRobEnd.IDLE;

		bool policeNear = LF_RobHold.PoliceWithin(job.m_vSpot, job.m_fPoliceRadius);
		if (WarnLookouts(policeNear, job.m_bPoliceNoted))
			NotifyLookouts(job);

		job.m_bPoliceNoted = policeNear;
		return LF_ECrewRobEnd.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds between two tick-count readings in ms (float: a 250 ms tick is 0.25 s, not 0).
	static float TickSeconds(int nowMs, int lastMs)
	{
		return (nowMs - lastMs) / 1000.0; // lf-float: a short span of ticks to seconds
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsDead(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && controller.GetLifeState() == ECharacterLifeState.DEAD;
	}

	//------------------------------------------------------------------------------------------------
	//! The lookouts are warned once per approach: when an officer arrives, not while one stays.
	static bool WarnLookouts(bool policeNear, bool alreadyNoted)
	{
		return policeNear && !alreadyNoted;
	}

	//------------------------------------------------------------------------------------------------
	protected static void NotifyLookouts(LF_CrewRobJob job)
	{
		for (int i = 1; i < job.m_aCrew.Count(); i++)
		{
			IEntity body = job.m_aCrew[i].Body();
			if (body)
				LF_Utils.NotifyEntity(body, "#LF-CrewRob_PoliceNear", "#LF-CrewRob_Title", 4.0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the holder takes the money. Ends the job and pays every crew member within reach of
	//! the till an equal share (the remainder one cash each to the first), once.
	//! \return The refusal, or empty when the job paid (LastPaid is the total).
	static string TakeMoney(LF_RobAction till, LF_CrewRobMember who)
	{
		LF_CrewRobJob job = JobOf(till);
		string refusal = HolderRefusal(job, who);
		if (refusal.IsEmpty() && job.Take() <= 0)
			refusal = REFUSED_NOTHING;

		if (!refusal.IsEmpty())
		{
			s_sLastRefusal = refusal;
			return refusal;
		}

		// Robbery turned off while the job ran (#960): the till stays shut.
		if (LF_Systems.Refuses(LF_RobAction.SYSTEM, "crew robbery payout"))
		{
			s_sLastRefusal = LF_RobAction.REFUSED_OFF;
			return LF_RobAction.REFUSED_OFF;
		}

		int total = job.Take();
		array<ref LF_CrewRobMember> present = {};
		foreach (LF_CrewRobMember member : job.m_aCrew)
		{
			IEntity body = member.Body();
			if (!body || vector.Distance(body.GetOrigin(), job.m_vSpot) > PAY_RADIUS)
				continue;

			// A member who is down, dead or cuffed is not present for a share.
			if (LF_Downed.IsDowned(body) || IsDead(body) || LF_Restraint.IsRestrained(body))
				continue;

			present.Insert(member);
		}

		array<int> shares = {};
		LF_CrewRobRules.Split(total, present.Count(), shares);
		int paid = 0;
		for (int i = 0; i < present.Count(); i++)
		{
			IEntity body = present[i].Body();
			int got = LF_RobAction.PayLoot(body, shares[i]);
			paid = paid + got;
			if (got > 0)
				LF_Utils.NotifyEntity(body, "#LF-CrewRob_Paid", "#LF-CrewRob_Title", 4.0, got.ToString());
		}

		job.m_iPaidTotal = paid;
		s_iLastPaid = paid;
		s_sLastRefusal = "";
		Close(job);
		LF_Debug.Info(FEATURE, string.Format("crew job ends: take %1, paid %2 among %3 present of %4", total, paid, present.Count(), job.m_aCrew.Count()));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Closes `job` and takes it off the list; it can pay no more.
	protected static void Close(LF_CrewRobJob job)
	{
		job.m_bClosed = true;
		Jobs().RemoveItem(job);
		if (Jobs().IsEmpty() && s_bTicking && GetGame())
		{
			GetGame().GetCallqueue().Remove(Tick);
			s_bTicking = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		int nowMs = System.GetTickCount();
		array<ref LF_CrewRobJob> jobs = Jobs();
		for (int i = jobs.Count() - 1; i >= 0; i--)
		{
			LF_CrewRobJob job = jobs[i];
			float dt = TickSeconds(nowMs, job.m_iLastTickMs);
			job.m_iLastTickMs = nowMs;
			LF_ECrewRobEnd end = Step(job, dt);
			if (end == LF_ECrewRobEnd.NONE)
				continue;

			LF_Debug.Info(FEATURE, string.Format("crew job ends unpaid: %1", typename.EnumToString(LF_ECrewRobEnd, end)));
			foreach (LF_CrewRobMember member : job.m_aCrew)
			{
				IEntity body = member.Body();
				if (body)
					LF_Utils.NotifyEntity(body, "#LF-CrewRob_Failed", "#LF-CrewRob_Title", 4.0);
			}

			Close(job);
		}

		if (jobs.IsEmpty() && s_bTicking)
		{
			GetGame().GetCallqueue().Remove(Tick);
			s_bTicking = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the crew an action's starter gets: the starter first, then every other armed civilian
	//! within JOIN_RADIUS who is up and not cuffed.
	static void GatherCrew(IEntity starter, notnull array<ref LF_CrewRobMember> crew)
	{
		crew.Clear();
		if (!starter)
			return;

		crew.Insert(LF_CrewRobMember.Make(starter, LF_Utils.GetPlayerUid(starter)));
		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity body = players.GetPlayerControlledEntity(id);
			if (!body || body == starter)
				continue;

			if (vector.Distance(body.GetOrigin(), starter.GetOrigin()) > JOIN_RADIUS)
				continue;

			if (!LF_HoldUp.IsArmed(body) || LF_Downed.IsDowned(body) || LF_Restraint.IsRestrained(body))
				continue;

			string uid = LF_Utils.GetPlayerUid(body);
			if (uid.IsEmpty() || !LF_RobAction.IsFactionAllowedToRob(uid))
				continue;

			crew.Insert(LF_CrewRobMember.Make(body, uid));
		}
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_CrewRob's statics (LF_StaticReset finds it by reflection).
class LF_CrewRob_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_CrewRob";
	}

	override void Apply()
	{
		LF_CrewRob.ResetStatics();
	}
}
