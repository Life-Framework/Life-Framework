//------------------------------------------------------------------------------------------------
//! How full the evidence locker is (#3378). The locker's rows are counted against the configured
//! threshold (LF_EvidenceConfig.ConvoyThreshold). Crossing it raises LF_OnEvidenceConvoyDue once;
//! the locker emptying (a raid, a world reset, a load) re-arms it. A break-in on the locker (its
//! breach target reset or its charge cancelled, LF_Breach) raises the event at once, whatever the
//! count. Server only. The locker's rows and save are not touched.
class LF_EvidenceLockerFill
{
	static const string FEATURE = "EvidenceLocker";
	//! The locker prefab's LF_BreachTargetComponent target id.
	static const string LOCKER_TARGET_ID = "LF_EvidenceLocker";
	static const string REASON_FULL = "full";
	static const string REASON_BREAK_IN = "break-in";

	//! True from the moment the convoy was ordered until the locker drops below the threshold.
	protected static bool s_bDue;
	//! True while a cool-down timer will re-arm the order (see ScheduleRearm).
	protected static bool s_bRearmPending;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_bDue = false;
		s_bRearmPending = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(RearmNow);
	}

	//------------------------------------------------------------------------------------------------
	//! True while a cool-down is running after a refused or called-off convoy.
	static bool IsRearmPending()
	{
		return s_bRearmPending;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a convoy was ordered but did not go (the start was refused, or the convoy was called off
	//! with its rows still in the locker). Without this the order stayed "due" until the locker dropped
	//! below the threshold or the server restarted, so a locker that stayed full ordered nothing more.
	//! The order is armed again after the configured cool-down; a still-full locker then orders another.
	static void ScheduleRearm()
	{
		if (!Replication.IsServer() || !s_bDue)
			return;

		int seconds = LF_EvidenceConfig.ConvoyRearmSeconds();
		s_bRearmPending = true;
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(RearmNow);
			GetGame().GetCallqueue().CallLater(RearmNow, seconds * 1000, false);
		}

		LF_Debug.Info(FEATURE, string.Format("convoy not under way; the order re-arms in %1 s", seconds));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the cool-down is over. Arms the order again and checks the count, so a locker still at the
	//! threshold orders another convoy (and a locker below it just stays armed).
	static void RearmNow()
	{
		s_bRearmPending = false;
		s_bDue = false;
		LF_Debug.Info(FEATURE, "convoy order re-armed");
		Check();
	}

	//------------------------------------------------------------------------------------------------
	static int Threshold()
	{
		return LF_EvidenceConfig.ConvoyThreshold();
	}

	//------------------------------------------------------------------------------------------------
	//! True while a convoy has been ordered and the locker has not been emptied since.
	static bool IsDue()
	{
		return s_bDue;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: called by the locker after its rows changed. Raises the convoy event the first time
	//! the count reaches the threshold; a count below it re-arms.
	static void Check()
	{
		if (!Replication.IsServer())
			return;

		int rows = LF_EvidenceLocker.Count();
		if (rows < Threshold())
		{
			s_bDue = false;
			// A pending cool-down has nothing left to re-arm.
			if (s_bRearmPending && GetGame() && GetGame().GetCallqueue())
				GetGame().GetCallqueue().Remove(RearmNow);

			s_bRearmPending = false;
			return;
		}

		Raise(rows, REASON_FULL);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: LF_Breach reports a reset or a cancelled charge on a target. Only the locker's own
	//! target counts; the convoy is ordered early, once.
	static void OnBreakIn(string targetId)
	{
		if (targetId != LOCKER_TARGET_ID || !Replication.IsServer())
			return;

		Raise(LF_EvidenceLocker.Count(), REASON_BREAK_IN);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Raise(int rows, string reason)
	{
		if (s_bDue)
			return;

		s_bDue = true;
		LF_Debug.Info(FEATURE, string.Format("convoy due: %1 row(s) against threshold %2 (%3)", rows, Threshold(), reason));
		LF_EventBus.Publish(LF_OnEvidenceConvoyDue.Create(rows, Threshold(), reason));
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_EvidenceLockerFill's statics (LF_StaticReset finds it by reflection).
class LF_EvidenceLockerFill_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_EvidenceLockerFill";
	}

	override void Apply()
	{
		LF_EvidenceLockerFill.ResetStatics();
	}
}
