// red-proof: observed 2026-11-18 with `test --no-build --red LF_Test_DutyShiftCap,LF_Test_TrialExpiryDuty,LF_Test_TicketRequestLimit` (RED-OK, all 3 failed with the hook on, log tmp/logs/test-20261118-003756.log): the hook skips the shift settle and the clock stop, so 'eight hours on one clock-in count only the shift cap for pay' and 'a disconnect stops the duty clock' fail. Green with the fix.
// tier: LOGIC
//! police/duty-shift-cap (#2416): duty pay and rank count at most dutyShiftMaxMinutes of one shift
//! (an AFK officer stops earning), a disconnect stops the clock so offline time is never counted,
//! and a returning officer's clock restarts inside the same shift cap.
class LF_Test_DutyShiftCap : LF_Test
{
	protected static const string AFK = "duty-cap-afk";
	protected static const string LEAVER = "duty-cap-leaver";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/duty-shift-cap";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int cap = LF_PoliceConfig.DutyShiftMaxSeconds();
		ctx.True(cap >= 300, "the police config loads a shift cap");

		ctx.Equal(60, LF_PoliceRanks.ShiftCredit(60, 0, cap), "inside the cap every second counts");
		ctx.Equal(20, LF_PoliceRanks.ShiftCredit(60, cap - 20, cap), "the last seconds of the cap count");
		ctx.Equal(0, LF_PoliceRanks.ShiftCredit(60, cap, cap), "nothing counts past the cap");
		ctx.Equal(0, LF_PoliceRanks.ShiftCredit(60, cap + 500, cap), "a shift already over the cap counts nothing");

		// An officer who walked away: eight hours on one clock-in.
		LF_PlayerAccount afk = Police(AFK);
		LF_PoliceRanks.OnDutyChangedAt(afk, true, 0);
		int eightHoursMs = 8 * 3600 * 1000;
		if (!RedProof())
			LF_PoliceRanks.SettleDutyAt(afk, true, eightHoursMs);
		ctx.Equal(cap, afk.GetUnpaidDutySeconds(), "eight hours on one clock-in count only the shift cap for pay");
		ctx.Equal(cap, afk.GetShiftSeconds(), "the shift remembers what it counted");

		LF_PoliceRanks.SettleDutyAt(afk, true, eightHoursMs + 600000);
		ctx.Equal(cap, afk.GetUnpaidDutySeconds(), "the next ten minutes past the cap add nothing");

		// Clocking in again starts a new shift.
		LF_PoliceRanks.OnDutyChangedAt(afk, false, eightHoursMs + 700000);
		afk.TakeUnpaidDutySeconds();
		LF_PoliceRanks.OnDutyChangedAt(afk, true, eightHoursMs + 800000);
		LF_PoliceRanks.SettleDutyAt(afk, false, eightHoursMs + 860000);
		ctx.Equal(60, afk.GetUnpaidDutySeconds(), "a new clock-in counts again from zero");

		// A disconnect stops the clock: the time away is not counted.
		LF_PlayerAccount leaver = Police(LEAVER);
		LF_PoliceRanks.OnDutyChangedAt(leaver, true, 0);
		if (!RedProof())
			LF_PoliceRanks.StopDutyClockOf(leaver);
		ctx.False(leaver.IsDutyClockRunning(), "a disconnect stops the duty clock");
		ctx.True(leaver.IsOnDuty(), "the officer stays on duty while away");

		int unpaidAtLeave = leaver.GetUnpaidDutySeconds();
		LF_PoliceRanks.SettleDutyAt(leaver, true, eightHoursMs);
		LF_PoliceRanks.SettleDutyAt(leaver, false, 2 * eightHoursMs);
		ctx.Equal(unpaidAtLeave, leaver.GetUnpaidDutySeconds(), "hours offline add no pay");

		// Back in the game: the clock restarts and the same shift cap applies.
		ctx.True(LF_PoliceRanks.ResumeDutyClock(leaver, 1000000), "an officer still on duty gets the clock back when they spawn");
		ctx.False(LF_PoliceRanks.ResumeDutyClock(leaver, 1000000), "a running clock is left alone");
		LF_PoliceRanks.SettleDutyAt(leaver, false, 1060000);
		ctx.Equal(unpaidAtLeave + 60, leaver.GetUnpaidDutySeconds(), "only the minute after the return counts");

		LF_PlayerAccount civilian = LF_PlayerAccountManager.GetOrCreate("duty-cap-civilian");
		civilian.SetFaction(LF_Faction.CIVILIAN);
		civilian.SetOnDuty(false);
		ctx.False(LF_PoliceRanks.ResumeDutyClock(civilian, 5000), "a civilian never gets a duty clock");
		ctx.False(LF_PoliceRanks.StopDutyClockOf(null), "no account stops nothing");

		afk.SetOnDuty(false);
		afk.StopDutyClock();
		afk.SetPoliceRank(LF_PoliceRanks.PATROL);
		afk.SetOnDutySeconds(0);
		leaver.SetOnDuty(false);
		leaver.StopDutyClock();
		leaver.SetOnDutySeconds(0);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerAccount Police(string uid)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(LF_Faction.POLICE);
		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDutySeconds(0);
		account.SetOnDuty(true);
		account.StopDutyClock();
		account.ResetShiftSeconds();
		account.TakeUnpaidDutySeconds();
		return account;
	}
}
