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

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_bDue = false;
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
