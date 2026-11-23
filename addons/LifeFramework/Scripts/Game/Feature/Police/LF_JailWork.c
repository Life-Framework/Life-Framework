//------------------------------------------------------------------------------------------------
//! Jail work (#127). A prisoner scrubs the cell floor or sorts laundry; each finished task takes
//! seconds off the remaining sentence. The server owns it end to end: the action carries no amount,
//! the job's seconds-off, cooldown and work time are config on the placed action, and the server
//! re-checks that the player is serving, is not already working, and is off cooldown.
//!
//! PERSISTENCE. Nothing new is saved. The reduction lands on the LF_Jail sentence's remaining
//! seconds, which the existing "jail" payload already carries. The per-job cooldown is session-only
//! (a restart forgives it), like other cooldowns.
//------------------------------------------------------------------------------------------------
enum LF_EJailWorkCheck
{
	OK,
	NOT_SERVING,
	BUSY,
	COOLDOWN,
	BAD_CONFIG
}

//------------------------------------------------------------------------------------------------
//! Pure rules, for LOGIC tests.
class LF_JailWorkRules
{
	//! The cap the stations' seconds-off were authored against (the old 5 minute cell).
	static const int REFERENCE_CAP = 300;

	//! One chore never takes off more than this share of the sentence cap.
	static const float CHORE_SHARE_MAX = 0.05;

	//! A full shift (every station once) stays within this share of the cap.
	static const float SHIFT_SHARE_MAX = 0.1;

	//------------------------------------------------------------------------------------------------
	//! Seconds a finished chore takes off: the station's number scaled to the cell's sentence cap
	//! (3x at 15 minutes), held to CHORE_SHARE_MAX of the cap. No cap set (0) leaves it alone.
	static float ScaledSecondsOff(float secondsOff, int cap)
	{
		if (cap <= 0)
			return secondsOff;

		float scaled = secondsOff * cap / REFERENCE_CAP;
		float ceiling = cap * CHORE_SHARE_MAX;
		if (scaled > ceiling)
			return ceiling;

		return scaled;
	}

	//! lastDoneAt below zero means this job was never finished.
	static LF_EJailWorkCheck Check(bool serving, bool busy, float secondsOff, float lastDoneAt, float now, float cooldown)
	{
		if (secondsOff <= 0)
			return LF_EJailWorkCheck.BAD_CONFIG;

		if (!serving)
			return LF_EJailWorkCheck.NOT_SERVING;

		if (busy)
			return LF_EJailWorkCheck.BUSY;

		if (lastDoneAt >= 0 && now - lastDoneAt < cooldown)
			return LF_EJailWorkCheck.COOLDOWN;

		return LF_EJailWorkCheck.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds until the job can be worked again. Zero when ready.
	static float CooldownLeft(float lastDoneAt, float now, float cooldown)
	{
		if (lastDoneAt < 0)
			return 0;

		float left = cooldown - (now - lastDoneAt);
		if (left < 0)
			return 0;

		return left;
	}

	//------------------------------------------------------------------------------------------------
	//! "m:ss" for the toast.
	static string FormatTime(float seconds)
	{
		int total = Math.Ceil(seconds);
		if (total < 0)
			total = 0;

		int m = total / 60;
		int s = total - m * 60;
		string pad = "";
		if (s < 10)
			pad = "0";

		return string.Format("%1:%2%3", m, pad, s);
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_EJailWorkCheck check)
	{
		if (check == LF_EJailWorkCheck.NOT_SERVING)
			return "#LF-JailWork_NotPrisoner";

		if (check == LF_EJailWorkCheck.BUSY)
			return "#LF-JailWork_Busy";

		if (check == LF_EJailWorkCheck.COOLDOWN)
			return "#LF-JailWork_Cooldown";

		return "#LF-JailWork_BadConfig";
	}
}

//------------------------------------------------------------------------------------------------
class LF_JailWork
{
	static const string FEATURE = "Jail";

	protected static ref map<string, float> s_mLastDone;
	protected static ref array<string> s_aBusy;

	//! The cell's sentence cap in seconds; 0 until a cell sets it (chores then credit as written).
	protected static int s_iSentenceCap;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mLastDone = null;
		s_aBusy = null;
		s_iSentenceCap = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the placed cell's sentence cap; chores credit in proportion to it (#4207).
	static void SetSentenceCap(int cap)
	{
		s_iSentenceCap = cap;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, float> LastDone()
	{
		if (!s_mLastDone)
			s_mLastDone = new map<string, float>();

		return s_mLastDone;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<string> Busy()
	{
		if (!s_aBusy)
			s_aBusy = {};

		return s_aBusy;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Key(string playerUid, string jobId)
	{
		return playerUid + "|" + jobId;
	}

	//------------------------------------------------------------------------------------------------
	protected static float LastDoneAt(string playerUid, string jobId)
	{
		float at;
		if (LastDone().Find(Key(playerUid, jobId), at))
			return at;

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EJailWorkCheck Check(string playerUid, string jobId, float secondsOff, float cooldown, float now)
	{
		bool busy = Busy().Find(playerUid) >= 0;
		return LF_JailWorkRules.Check(LF_Jail.IsServing(playerUid), busy, secondsOff, LastDoneAt(playerUid, jobId), now, cooldown);
	}

	//------------------------------------------------------------------------------------------------
	//! Logic core: a finished task. Re-checks, takes secondsOff off the sentence once, stamps the
	//! cooldown. Shared by the world path and the tests.
	//! \return The check result. OK means the sentence was reduced.
	static LF_EJailWorkCheck Complete(string playerUid, string jobId, float secondsOff, float cooldown, float now)
	{
		LF_EJailWorkCheck check = Check(playerUid, jobId, secondsOff, cooldown, now);
		if (check != LF_EJailWorkCheck.OK)
			return check;

		LastDone().Set(Key(playerUid, jobId), now);
		float credit = LF_JailWorkRules.ScaledSecondsOff(secondsOff, s_iSentenceCap);
		float left = LF_Jail.Credit(playerUid, credit);
		LF_Debug.Info(FEATURE, string.Format("jail work %1 done by %2: -%3 s, %4 s left", jobId, playerUid, credit, left));
		return LF_EJailWorkCheck.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected static float Now()
	{
		return GetGame().GetWorld().GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the prisoner began a job at `spot`. Checks, then starts the work pose; the credit
	//! lands when the work time is up and the prisoner is still alive and at the spot.
	static bool Begin(IEntity spot, IEntity worker, string jobId, float secondsOff, float cooldown, float workSeconds, LF_EWorkAnimKind kind)
	{
		if (!Replication.IsServer() || !spot || !worker)
			return false;

		string uid = LF_Utils.GetPlayerUid(worker);
		if (uid.IsEmpty())
			return false;

		return BeginFor(uid, spot, worker, jobId, secondsOff, cooldown, workSeconds, kind);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the worker is alive and within LF_WorkAnim.MAX_RANGE of `spot`. Begin and Finish share it.
	static bool IsAtSpot(IEntity worker, vector spot)
	{
		if (!worker)
			return false;

		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(worker);
		if (controller && controller.GetLifeState() != ECharacterLifeState.ALIVE)
			return false;

		return vector.Distance(worker.GetOrigin(), spot) <= LF_WorkAnim.MAX_RANGE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: Begin once the player uid is known (the tests have no player behind the character).
	//! Refuses a dead worker or one beyond LF_WorkAnim.MAX_RANGE of the spot, so a task cannot be
	//! started from across the cell (#2417 item 26).
	static bool BeginFor(string uid, IEntity spot, IEntity worker, string jobId, float secondsOff, float cooldown, float workSeconds, LF_EWorkAnimKind kind)
	{
		if (!Replication.IsServer() || !spot || !worker || uid.IsEmpty())
			return false;

		if (!IsAtSpot(worker, spot.GetOrigin()))
		{
			LF_Debug.Warn(FEATURE, string.Format("jail work %1 refused for %2: out of reach of the work spot, or down", jobId, uid));
			return false;
		}

		LF_EJailWorkCheck check = Check(uid, jobId, secondsOff, cooldown, Now());
		if (check != LF_EJailWorkCheck.OK)
		{
			LF_Debug.Info(FEATURE, string.Format("jail work %1 refused for %2: %3", jobId, uid, typename.EnumToString(LF_EJailWorkCheck, check)));
			string arg = "";
			if (check == LF_EJailWorkCheck.COOLDOWN)
				arg = LF_JailWorkRules.FormatTime(LF_JailWorkRules.CooldownLeft(LastDoneAt(uid, jobId), Now(), cooldown));

			Toast(worker, LF_JailWorkRules.RefusalKey(check), LF_ENotificationType.WARNING, arg);
			return false;
		}

		if (workSeconds < 1)
			workSeconds = 1;

		Reserve(uid);
		LF_WorkAnim.Begin(worker, spot.GetOrigin(), kind, workSeconds);
		GetGame().GetCallqueue().CallLater(Finish, workSeconds * 1000, false, uid, worker, spot.GetOrigin(), jobId, secondsOff, cooldown);
		LF_Debug.Info(FEATURE, string.Format("jail work %1 started by %2 (%3 s)", jobId, uid, workSeconds));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: marks the prisoner as working one task (Begin; the tests use it to stand in for one).
	static void Reserve(string uid)
	{
		if (!uid.IsEmpty() && Busy().Find(uid) < 0)
			Busy().Insert(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the work timer ran out. The uid travels with the timer: a prisoner who logged out
	//! mid-task has no entity left, and must not stay busy (refused every task) after rejoining.
	static void Finish(string uid, IEntity worker, vector spot, string jobId, float secondsOff, float cooldown)
	{
		Busy().RemoveItem(uid);
		if (!worker)
		{
			LF_Debug.Info(FEATURE, string.Format("jail work %1 dropped for %2: the worker is gone", jobId, uid));
			return;
		}

		LF_WorkAnim.End(worker, "jail work finished");

		if (!IsAtSpot(worker, spot))
		{
			LF_Debug.Info(FEATURE, string.Format("jail work %1 abandoned by %2", jobId, uid));
			return;
		}

		LF_EJailWorkCheck result = Complete(uid, jobId, secondsOff, cooldown, Now());
		if (result != LF_EJailWorkCheck.OK)
		{
			Toast(worker, LF_JailWorkRules.RefusalKey(result), LF_ENotificationType.WARNING, "");
			return;
		}

		string arg = "0:00";
		if (LF_Jail.IsServing(uid))
			arg = LF_JailWorkRules.FormatTime(LF_Jail.GetRemaining(uid));

		Toast(worker, "#LF-JailWork_Done", LF_ENotificationType.SUCCESS, arg);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Toast(IEntity worker, string message, LF_ENotificationType type, string arg)
	{
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(worker);
		if (playerId <= 0)
			return;

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-Police_Title", message, 4.0, type, arg);
	}
}

//------------------------------------------------------------------------------------------------
//! A job spot in the cell. The numbers are config on the placed action, never sent by the client.
//! The server runs LF_JailWork.Begin and refuses anyone who is not serving a sentence.
class LF_JailWorkAction : ScriptedUserAction
{
	[Attribute("scrub", UIWidgets.EditBox, desc: "Job id; the cooldown is kept per prisoner and job.")]
	protected string m_sJobId;

	[Attribute("30", UIWidgets.EditBox, desc: "Seconds taken off the sentence when the task is finished.")]
	protected float m_fSecondsOff;

	[Attribute("45", UIWidgets.EditBox, desc: "Seconds before this job can be worked again.")]
	protected float m_fCooldownSeconds;

	[Attribute("6", UIWidgets.EditBox, desc: "Seconds the work pose runs before the task counts.")]
	protected float m_fWorkSeconds;

	[Attribute("561", UIWidgets.ComboBox, desc: "Work pose.", enums: ParamEnumArray.FromEnum(LF_EWorkAnimKind))]
	protected LF_EWorkAnimKind m_eKind;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police"))
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_JailWorkAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_JailWork.Begin(pOwnerEntity, pUserEntity, m_sJobId, m_fSecondsOff, m_fCooldownSeconds, m_fWorkSeconds, m_eKind);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_JailWork's statics (LF_StaticReset finds it by reflection).
class LF_JailWork_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_JailWork";
	}

	override void Apply()
	{
		LF_JailWork.ResetStatics();
	}
}
