// red-proof: observed 2026-11-17 with `test --no-build --red LF_Test_DutyClockDisconnect` (RED PROOF OK, log tmp/logs/test-20261117-220601.log): the hook skips the disconnect stop and the 1000 s cap, so 'rank time counts only the paid seconds' (expected 600, got 4000) and 'a 4000 s shift pays the 1000 s cap' (expected 1000, got 4000) fail. On main the test does not compile (no dutyShiftMaxSeconds). Green with the change.
// tier: WORLD
//! police/duty-clock-disconnect (#2622): a disconnect settles the duty clock at the disconnect time,
//! so the time away is never paid at the next clock-out, and one shift pays at most the police
//! config's dutyShiftMaxSeconds (0 means no cap). Rank time counts only the paid seconds.
class LF_Test_DutyClockDisconnect : LF_Test
{
	protected static const string LEAVER = "duty-disconnect-leaver";
	protected static const string CAPPED = "duty-disconnect-capped";
	protected static const string UNCAPPED = "duty-disconnect-uncapped";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/duty-clock-disconnect";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PoliceConfig config = LF_PoliceConfig.Get();
		ctx.True(config != null, "the police config loads");
		if (!config)
			return;

		int savedCap = config.m_iDutyShiftMaxSeconds;
		ctx.Equal(10800, LF_PoliceConfig.DutyShiftMaxSeconds(), "the shipped shift cap is three hours");

		// Clock in at t0, disconnect at t0+600 s, rejoin and clock out at t0+4000 s.
		int t0 = 50000;
		LF_PlayerAccount leaver = Police(LEAVER);
		LF_PoliceRanks.OnDutyChangedAt(leaver, true, t0);
		if (!RedProof())
			LF_PoliceRanks.StopDutyClockAt(leaver, t0 + 600000);
		ctx.True(leaver.IsOnDuty(), "the officer stays on duty while away");
		LF_PoliceRanks.ResumeDutyClock(leaver, t0 + 4000000);
		LF_PoliceRanks.OnDutyChangedAt(leaver, false, t0 + 4000000);
		ctx.Equal(600, LF_DutyPay.TakeShiftPay(leaver, 1.0), "the clock-out after a disconnect pays only the 600 s before it");
		ctx.Equal(600, leaver.GetOnDutySeconds(), "rank time counts only the paid seconds");

		// A cap of 1000 s and a 4000 s shift: 1000 s paid.
		if (!RedProof())
			config.m_iDutyShiftMaxSeconds = 1000;
		LF_PlayerAccount capped = Police(CAPPED);
		LF_PoliceRanks.OnDutyChangedAt(capped, true, t0);
		LF_PoliceRanks.SettleDutyAt(capped, true, t0 + 2000000);
		LF_PoliceRanks.OnDutyChangedAt(capped, false, t0 + 4000000);
		ctx.Equal(1000, LF_DutyPay.TakeShiftPay(capped, 1.0), "a 4000 s shift pays the 1000 s cap");
		ctx.Equal(1000, capped.GetOnDutySeconds(), "rank time stops at the cap too");

		// 0 means no cap.
		config.m_iDutyShiftMaxSeconds = 0;
		LF_PlayerAccount uncapped = Police(UNCAPPED);
		LF_PoliceRanks.OnDutyChangedAt(uncapped, true, t0);
		LF_PoliceRanks.OnDutyChangedAt(uncapped, false, t0 + 4000000);
		ctx.Equal(4000, LF_DutyPay.TakeShiftPay(uncapped, 1.0), "a cap of 0 pays the whole shift");

		config.m_iDutyShiftMaxSeconds = savedCap;
		Clear(leaver);
		Clear(capped);
		Clear(uncapped);
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

	//------------------------------------------------------------------------------------------------
	protected void Clear(LF_PlayerAccount account)
	{
		account.SetOnDuty(false);
		account.StopDutyClock();
		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDutySeconds(0);
		account.SetFaction(LF_Faction.CIVILIAN);
	}
}
