// red-proof: 2026-11-18 `tools/cli test --no-build --red LF_Test_DutyPayRankChange,LF_Test_ArmoryDemotionStrip`: RED PROOF OK; with the hook paying every unpaid second at the clock-out rate (the pre-#2874 rule), "demotion ... pay 250 (expected 250, got 150)", "time promotion ... (expected 250, got 300)", "medic promotion ... (expected 80, got 140)". Green with `--only`.
//! police/duty-pay-rank-change (#2874): a shift that spans a rank change pays each part at the rank
//! held during it. An administrator promotion, an administrator demotion, a time promotion and a medic
//! time promotion each restate the unpaid seconds earned so far at the new rank's rate, so the
//! clock-out pay is the sum of the parts (a promotion used to pay the whole shift at the new rate, a
//! demotion at the lower one).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DutyPayRankChange : LF_Test
{
	protected static const string ADMIN = "pay-rank-admin";
	protected static const string OFFICER = "pay-rank-officer";
	protected static const string MEDIC = "pay-rank-medic";
	protected static const string NOBODY_ROSTER = "$profile:LifeFramework/test-pay-rank-no-staff.txt";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/duty-pay-rank-change";
	}

	//------------------------------------------------------------------------------------------------
	//! seconds on the duty clock, from a clock started at 0 ms. The officer stays on duty with the clock stopped.
	protected static void Work(LF_PlayerAccount account, int seconds)
	{
		account.StartDutyClock(0);
		LF_PoliceRanks.SettleDutyAt(account, false, seconds * 1000);
	}

	//------------------------------------------------------------------------------------------------
	//! The clock-out pay the account is owed now, at its current rank's rate.
	protected int Pay(LF_PlayerAccount account)
	{
		// RedProof(): the shift is paid as before #2874, every unpaid second at the rate held at clock-out.
		if (RedProof())
			account.SetUnpaidDutySeconds(account.GetShiftSeconds());

		return LF_DutyPay.TakeShiftPay(account, LF_DutyPay.RateForAccount(account));
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The pure restatement.
		ctx.Equal(50, LF_DutyPay.CarrySeconds(100, 0.5, 1.0), "100 s at 0.5 are 50 s at 1.0");
		ctx.Equal(200, LF_DutyPay.CarrySeconds(100, 1.0, 0.5), "100 s at 1.0 are 200 s at 0.5");
		ctx.Equal(2, LF_DutyPay.CarrySeconds(3, 0.3, 0.65), "a part second rounds up, never paying less");
		ctx.Equal(0, LF_DutyPay.CarrySeconds(100, 0, 1.0), "seconds at an unpaid rank are worth nothing");
		ctx.Equal(100, LF_DutyPay.CarrySeconds(100, 1.0, 0), "seconds cannot be restated at a rate of 0, so they stay");
		ctx.Equal(0, LF_DutyPay.CarrySeconds(-5, 1.0, 0.5), "negative seconds carry nothing");

		LF_PoliceConfig police = LF_PoliceConfig.Get();
		LF_MedicConfig medics = LF_MedicConfig.Get();
		ctx.NotNull(police, "the police config resolves");
		ctx.NotNull(medics, "the medic config resolves");
		if (!police || !medics)
			return;

		// A ladder with two pay rates: patrol 0.5 per second, promoted by time at 1000 s; senior 1.0.
		array<ref LF_PoliceRank> savedPolice = police.m_aRanks;
		array<ref LF_PoliceRank> ladder = new array<ref LF_PoliceRank>();
		ladder.Insert(LF_PoliceRank.Create("patrol", "EL-Rank_Patrol", 100, 0.5, 1000, LF_PoliceRanks.SIDEARM));
		ladder.Insert(LF_PoliceRank.Create("senior", "EL-Rank_Senior", 1000, 1.0, 0, LF_PoliceRanks.LONG_GUN));
		police.m_aRanks = ladder;
		LF_PoliceApplicationStaff.SetStaffPath(NOBODY_ROSTER);

		LF_PlayerAccount officer = LF_TestPoliceKit.Account(OFFICER, true);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		officer.SetPoliceRank(LF_PoliceRanks.PATROL);
		officer.SetOnDutySeconds(0);
		officer.SetUnpaidDutySeconds(0);
		officer.ResetShiftSeconds();

		// Administrator promotion mid-shift: 600 s at patrol, then 100 s at senior.
		Work(officer, 600);
		ctx.EqualStr("", LF_PoliceRankAdmin.SetRank(ADMIN, true, OFFICER, LF_PoliceRanks.SENIOR), "an administrator promotes the officer");
		Work(officer, 100);
		ctx.Equal(400, Pay(officer), "promotion: 600 s at 0.5 and 100 s at 1.0 pay 400");

		// Administrator demotion mid-shift: 200 s at senior, then 100 s at patrol.
		officer.ResetShiftSeconds();
		Work(officer, 200);
		ctx.EqualStr("", LF_PoliceRankAdmin.SetRank(ADMIN, true, OFFICER, LF_PoliceRanks.PATROL), "an administrator demotes the officer");
		Work(officer, 100);
		ctx.Equal(250, Pay(officer), "demotion: 200 s at 1.0 and 100 s at 0.5 pay 250");

		// Time promotion: one settle across the 1000 rank-second threshold is split there.
		officer.ResetShiftSeconds();
		officer.SetOnDutySeconds(900);
		Work(officer, 200);
		ctx.Equal(LF_PoliceRanks.SENIOR, officer.GetPoliceRank(), "time on duty promoted the officer");
		Work(officer, 100);
		ctx.Equal(250, Pay(officer), "time promotion: 100 s at 0.5, then 100 s and 100 s at 1.0, pay 250");

		// Medic time promotion: 0.1 per second to 0.2, promoted at 600 medic seconds.
		array<ref LF_MedicRank> savedMedic = medics.m_aRanks;
		array<ref LF_MedicRank> medicLadder = new array<ref LF_MedicRank>();
		medicLadder.Insert(LF_MedicRank.Create("first", "LF-MedicRank_Trainee", 0.1, 600));
		medicLadder.Insert(LF_MedicRank.Create("second", "LF-MedicRank_Paramedic", 0.2, 0));
		medics.m_aRanks = medicLadder;

		LF_PlayerAccount medic = LF_PlayerAccountManager.GetOrCreate(MEDIC);
		medic.SetFaction(LF_Faction.MEDIC);
		medic.SetOnDuty(true);
		medic.SetMedicRank(0);
		medic.SetMedicDutySeconds(0);
		medic.SetUnpaidDutySeconds(0);
		medic.ResetShiftSeconds();
		Work(medic, 700);
		ctx.Equal(1, medic.GetMedicRank(), "time on duty promoted the medic");
		ctx.Equal(80, Pay(medic), "medic promotion: 600 s at 0.1 and 100 s at 0.2 pay 80");

		medics.m_aRanks = savedMedic;
		medic.SetMedicRank(0);
		medic.SetMedicDutySeconds(0);
		medic.SetOnDuty(false);
		medic.SetFaction(LF_Faction.CIVILIAN);

		police.m_aRanks = savedPolice;
		officer.SetPoliceRank(LF_PoliceRanks.PATROL);
		officer.SetOnDutySeconds(0);
		LF_TestPoliceKit.Release(OFFICER);
		LF_PoliceApplicationStaff.SetStaffPath("");
	}
}
