//------------------------------------------------------------------------------------------------
//! The truck licence check (WK-W18, decision N13): at the licence desk a player pays the fee, is
//! lent a truck and drives a short course through its gates in order. Every gate in order passes
//! and earns the truck licence; a gate out of order, or leaving the course (Fail), ends the run and
//! sets a retake wait. The fee is not refunded.
//!
//! This is the rules only. The lent Ural, the gate markers and the examiner who calls Start and
//! ReachGate are the world and NPC cards (W21); they all go through this class on the server.
//!
//! RUN. An open run is server memory only (a restart abandons it, and the fee is lost like any
//! failed attempt). The licence and the retake wait are account flags in the progression store
//! ("truck_licence", "truck_retry:<in-game minute>"), so they ride the existing progression
//! payload with no new save field. The wait is counted on the in-game clock, as the permit's is.
//!
//! The truck licence is a flag, not an LF_ELicenseType: a licence type needs a catalogue row, a
//! replication bit and a seizure rule, which belong to the truck yard card. HoldsTruckLicence is
//! the one place that card reads.
//------------------------------------------------------------------------------------------------
enum LF_EDrivingCheck
{
	//! Start: the fee is taken and the run is open.
	STARTED,
	//! ReachGate: the gate was the next one; more remain.
	GATE_OK,
	//! ReachGate: the last gate; the truck licence is granted.
	PASSED,
	//! Fail, or a gate out of order: the run is over and the retake wait is set.
	FAILED_COURSE,
	//! ReachGate or Fail with no open run.
	NO_RUN,
	//! Start: a failed attempt's wait has not ended.
	RETAKE_WAIT,
	NOT_ENOUGH_CASH,
	ALREADY_HELD,
	//! Start: a run is already open.
	ALREADY_RUNNING,
	NO_CLOCK,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! One open check run: the next gate the examiner expects.
class LF_DrivingCheckRun : Managed
{
	int m_iNextGate;
}

//------------------------------------------------------------------------------------------------
class LF_DrivingCheck
{
	static const string FEATURE = "License";
	static const string LICENCE_FLAG = "truck_licence";
	static const string RETRY_FLAG = "truck_retry:";
	static const string LEDGER_REASON = "licence-fee";
	static const int FEE = 300;			// lf-fixed: decision N13, the truck check fee in dollars
	static const int RETAKE_MINUTES = 10;	// lf-fixed: decision N13, in-game minutes before a retake
	static const int GATE_COUNT = 4;		// lf-fixed: gates on the check course, passed in order

	protected static ref map<string, ref LF_DrivingCheckRun> s_mRuns;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mRuns = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_DrivingCheckRun> Runs()
	{
		if (!s_mRuns)
			s_mRuns = new map<string, ref LF_DrivingCheckRun>();

		return s_mRuns;
	}

	//------------------------------------------------------------------------------------------------
	static bool HoldsTruckLicence(string uid)
	{
		return LF_Progression.HasFlag(uid, LICENCE_FLAG);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether a run is open for the account.
	static bool IsRunning(string uid)
	{
		return Runs().Contains(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The next gate the open run expects (0 based), or -1 when no run is open.
	static int NextGate(string uid)
	{
		LF_DrivingCheckRun run = Runs().Get(uid);
		if (!run)
			return -1;

		return run.m_iNextGate;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The in-game minute a retake opens, or -1 when no wait is set.
	static int RetryMinute(string uid)
	{
		LF_ProgressionRecord record = LF_Progression.Get(uid);
		if (!record)
			return -1;

		string prefix = LF_Progression.FLAG_PREFIX + RETRY_FLAG;
		foreach (string entry : record.m_aTurnedInQuests)
		{
			if (!entry.StartsWith(prefix))
				continue;

			string digits = entry.Substring(prefix.Length(), entry.Length() - prefix.Length());
			if (digits.IsEmpty() || digits.ToInt().ToString() != digits)
			{
				LF_Debug.Error(FEATURE, string.Format("truck check: %1 has an unreadable retake minute '%2'; treated as none", uid, digits));
				return -1;
			}

			return digits.ToInt();
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return In-game minutes until a retake, 0 when none is waiting or it has passed. A clock set
	//! back waits one term, never longer.
	static int MinutesUntilRetake(string uid, int now)
	{
		int retry = RetryMinute(uid);
		if (retry < 0 || now < 0)
			return 0;

		int left = retry - now;
		if (left > RETAKE_MINUTES)
			left = RETAKE_MINUTES;

		if (left < 0)
			return 0;

		return left;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ClearRetry(string uid)
	{
		int retry = RetryMinute(uid);
		while (retry >= 0)
		{
			LF_Progression.ClearFlag(uid, RETRY_FLAG + retry.ToString());
			int next = RetryMinute(uid);
			if (next == retry)
				return;

			retry = next;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: by the player's own account. See StartFor.
	static LF_EDrivingCheck Start(IEntity player, out int minutesLeft)
	{
		return StartFor(LF_Utils.GetPlayerUID(player), new LF_EntityCashWallet(player, LEDGER_REASON, string.Empty), minutesLeft);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take the fee and open a run. Refused when the licence is held, a run is open, a
	//! retake wait has not ended (minutesLeft), or the wallet is short (nothing taken).
	//! \param[out] minutesLeft In-game minutes until a retake (RETAKE_WAIT).
	static LF_EDrivingCheck StartFor(string uid, notnull LF_CashWallet wallet, out int minutesLeft)
	{
		minutesLeft = 0;
		if (!Replication.IsServer() || uid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("truck check refused: server %1, account '%2'", Replication.IsServer(), uid));
			return LF_EDrivingCheck.FAILED;
		}

		if (HoldsTruckLicence(uid))
		{
			LF_Debug.Info(FEATURE, string.Format("truck check refused: %1 already holds the truck licence", uid));
			return LF_EDrivingCheck.ALREADY_HELD;
		}

		if (IsRunning(uid))
		{
			LF_Debug.Info(FEATURE, string.Format("truck check refused: %1 already has a run open", uid));
			return LF_EDrivingCheck.ALREADY_RUNNING;
		}

		int now = LF_LicenceDesk.NowMinutes();
		if (now < 0)
			return LF_EDrivingCheck.NO_CLOCK;

		minutesLeft = MinutesUntilRetake(uid, now);
		if (minutesLeft > 0)
		{
			LF_Debug.Info(FEATURE, string.Format("truck check refused: %1 retakes in %2 in-game minute(s)", uid, minutesLeft));
			return LF_EDrivingCheck.RETAKE_WAIT;
		}

		if (wallet.Carried() < FEE)
		{
			LF_Debug.Info(FEATURE, string.Format("truck check refused for %1: carries %2, the fee is %3", uid, wallet.Carried(), FEE));
			return LF_EDrivingCheck.NOT_ENOUGH_CASH;
		}

		int taken = wallet.Take(FEE);
		if (taken != FEE)
		{
			if (taken > 0)
				wallet.Put(taken);

			LF_Debug.Error(FEATURE, string.Format("truck check: the fee of %1 could not be taken from %2 (took %3, given back)", FEE, uid, taken));
			return LF_EDrivingCheck.NOT_ENOUGH_CASH;
		}

		ClearRetry(uid);
		Runs().Set(uid, new LF_DrivingCheckRun());
		LF_Debug.Info(FEATURE, string.Format("truck check: %1 paid %2 and started the course", uid, FEE));
		return LF_EDrivingCheck.STARTED;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the truck reached gate (0 based). The next gate in order moves the run on; the last
	//! one grants the truck licence; any other gate fails the run.
	static LF_EDrivingCheck ReachGate(string uid, int gate)
	{
		LF_DrivingCheckRun run = Runs().Get(uid);
		if (!run)
		{
			LF_Debug.Info(FEATURE, string.Format("truck check: %1 reached gate %2 with no run open", uid, gate));
			return LF_EDrivingCheck.NO_RUN;
		}

		if (gate != run.m_iNextGate)
		{
			LF_Debug.Info(FEATURE, string.Format("truck check: %1 reached gate %2, expected %3", uid, gate, run.m_iNextGate));
			return Fail(uid);
		}

		run.m_iNextGate = run.m_iNextGate + 1;
		if (run.m_iNextGate < GATE_COUNT)
			return LF_EDrivingCheck.GATE_OK;

		Runs().Remove(uid);
		LF_Progression.NoteFlag(uid, LICENCE_FLAG);
		LF_Debug.Info(FEATURE, string.Format("truck check: %1 passed the course and holds the truck licence", uid));
		return LF_EDrivingCheck.PASSED;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: end the run as failed (left the course, wrecked or abandoned the truck) and set the
	//! retake wait. The fee is not refunded.
	static LF_EDrivingCheck Fail(string uid)
	{
		if (!Runs().Contains(uid))
			return LF_EDrivingCheck.NO_RUN;

		Runs().Remove(uid);
		int now = LF_LicenceDesk.NowMinutes();
		if (now >= 0)
		{
			ClearRetry(uid);
			LF_Progression.NoteFlag(uid, RETRY_FLAG + (now + RETAKE_MINUTES).ToString());
		}

		LF_Debug.Info(FEATURE, string.Format("truck check: %1 failed the course; retake in %2 in-game minute(s)", uid, RETAKE_MINUTES));
		return LF_EDrivingCheck.FAILED_COURSE;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_DrivingCheck's statics (LF_StaticReset finds it by reflection).
class LF_DrivingCheck_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_DrivingCheck";
	}

	override void Apply()
	{
		LF_DrivingCheck.ResetStatics();
	}
}
