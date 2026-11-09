// red-proof: observed 2026-11-08 with `tools\cli test --no-build --tier all`: making
// TakeUnpaidDutySeconds skip `m_iUnpaidDutySeconds = 0;` printed
// [LFTEST]   - a second clock-out pays nothing (expected 0, got 240); restored, green.

// tier: LOGIC
class LF_Test_DutyPay : LF_Test
{
	override string GetName()
	{
		return "jobs/duty-pay";
	}

	override void Run(LF_TestContext ctx)
	{
		float rate = 2.0;

		ctx.Equal(240, LF_DutyPay.Owed(120, rate), "duty pay is rate times seconds");
		ctx.Equal(1, LF_DutyPay.Owed(3, 0.5), "a partial dollar rounds down");
		ctx.Equal(0, LF_DutyPay.Owed(0, rate), "no seconds owe nothing");
		ctx.Equal(0, LF_DutyPay.Owed(120, 0), "a zero rate owes nothing");
		ctx.True(LF_DutyPay.PoliceRatePerSecond() > 0, "the jobs config loads a police duty rate");

		// One shift: on duty at 1 s, off duty at 121 s.
		LF_PlayerAccount officer = Police("duty-pay-officer");
		LF_PoliceRanks.OnDutyChangedAt(officer, true, 1000);
		LF_PoliceRanks.OnDutyChangedAt(officer, false, 121000);
		ctx.Equal(240, LF_DutyPay.TakeShiftPay(officer, rate), "clock-out pays rate times duty seconds");
		ctx.Equal(120, officer.GetOnDutySeconds(), "the same seconds still count toward rank");

		// A repeated clock-out pays nothing more.
		LF_PoliceRanks.OnDutyChangedAt(officer, false, 600000);
		ctx.Equal(0, LF_DutyPay.TakeShiftPay(officer, rate), "a second clock-out pays nothing");

		// A mid-shift settle (the armory draw) keeps the seconds for the clock-out.
		LF_PlayerAccount drawer = Police("duty-pay-drawer");
		LF_PoliceRanks.OnDutyChangedAt(drawer, true, 0);
		LF_PoliceRanks.SettleDutyAt(drawer, true, 30000);
		LF_PoliceRanks.OnDutyChangedAt(drawer, false, 90000);
		ctx.Equal(180, LF_DutyPay.TakeShiftPay(drawer, rate), "a mid-shift settle still pays the whole shift once");

		// No clock-in, no pay.
		LF_PlayerAccount idle = Police("duty-pay-idle");
		LF_PoliceRanks.OnDutyChangedAt(idle, false, 500000);
		ctx.Equal(0, LF_DutyPay.TakeShiftPay(idle, rate), "clock-out without clock-in pays nothing");

		// A civilian with a running clock is owed nothing.
		LF_PlayerAccount civilian = LF_PlayerAccountManager.GetOrCreate("duty-pay-civilian");
		civilian.SetFaction(LF_Faction.CIVILIAN);
		LF_PoliceRanks.OnDutyChangedAt(civilian, true, 0);
		LF_PoliceRanks.OnDutyChangedAt(civilian, false, 600000);
		ctx.Equal(0, LF_DutyPay.TakeShiftPay(civilian, rate), "a civilian is paid no duty pay");

		ctx.Equal(0, LF_DutyPay.TakeShiftPay(null, rate), "no account is paid nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerAccount Police(string uid)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(LF_Faction.POLICE);
		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDuty(false);
		account.StopDutyClock();
		account.TakeUnpaidDutySeconds();
		return account;
	}
}
