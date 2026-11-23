// red-proof: 2026-11-23 `tools/cli test --no-build --red LF_Test_HeistArmLock` printed `RED-OK crime/heist-arm-lock (fails with the hook on, 35 assertions)`; RedProof() skips crew one arming the bank door, so no lock is taken at arming and the second crew arms inside the window.
//! #4492: the big-job lock is taken when a heist job's door charge is ARMED, not when the door opens,
//! so a second crew is refused inside the first crew's fuse window. The run adopts the lock when the
//! door opens and its end starts the cooldown from the job row (m_iCooldownSeconds; 0 is the 3 h
//! default). A charge defused before the fuse runs out ends the job. The heist refusal reason says
//! which rule refused (police, a big job running, the cooldown). Server-scripted arms on configured
//! targets with no entity (no charge carrier), the job table set through the LF_HeistRun test seam.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HeistArmLock : LF_Test
{
	protected static const string BANK_DOOR = "armlock-test-bank-door";
	protected static const string BANK_VAULT = "armlock-test-bank-vault";
	protected static const string DEPOT_DOOR = "armlock-test-depot-door";
	protected static const string DEPOT_VAULT = "armlock-test-depot-vault";
	protected static const string CREW_ONE = "armlock-test-crew-one";
	protected static const string CREW_TWO = "armlock-test-crew-two";
	protected static const string POLICE = "armlock-test-police";
	protected static const int BANK_COOLDOWN = 3600;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crime/heist-arm-lock";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_HeistStageRow Stage(string kind, string targetId, float radius)
	{
		LF_HeistStageRow stage = new LF_HeistStageRow();
		stage.m_sKind = kind;
		stage.m_sTargetId = targetId;
		stage.m_fRadius = radius;
		return stage;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_HeistSiteRow Job(string id, string doorId, string vaultId, int cooldownSeconds)
	{
		LF_HeistSiteRow row = LF_HeistSiteRow.Make(id);
		row.m_iClockSeconds = 720;
		row.m_iMinPoliceOnDuty = 2;
		row.m_iCooldownSeconds = cooldownSeconds;
		row.m_aStages.Insert(Stage(LF_HeistKinds.DOOR, doorId, 0));
		row.m_aStages.Insert(Stage(LF_HeistKinds.VAULT, vaultId, 0));
		row.m_aStages.Insert(Stage(LF_HeistKinds.GETAWAY, "", 50));
		return row;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_BigJobLock.ResetStatics();
		LF_HeistRun.ResetStatics();
		int t = 200000;
		LF_BigJobLock.SetClockForTests(t);

		LF_HeistSiteRow bank = Job("armlock-bank", BANK_DOOR, BANK_VAULT, BANK_COOLDOWN);
		LF_HeistSiteRow depot = Job("armlock-depot", DEPOT_DOOR, DEPOT_VAULT, 0);
		LF_HeistConfig config = new LF_HeistConfig();
		config.m_aSites = new array<ref LF_HeistSiteRow>();
		config.m_aSites.Insert(bank);
		config.m_aSites.Insert(depot);
		LF_HeistRun.SetConfigForTests(config);

		// Item 2: the cooldown comes from the row; a row that leaves it 0 keeps the 3 h default.
		ctx.Equal(BANK_COOLDOWN, LF_HeistRun.CooldownSeconds(bank), "the bank row's cooldown is read from the row");
		ctx.Equal(LF_BigJobLock.COOLDOWN_BANK, LF_HeistRun.CooldownSeconds(depot), "a row with no cooldown keeps the 3 h default");

		LF_Breach.Configure(BANK_DOOR, 30, "");
		LF_Breach.Configure(DEPOT_DOOR, 30, "");
		LF_PlayerAccountManager.GetOrCreate(POLICE).SetFaction(LF_Faction.POLICE);
		LF_PlayerAccountManager.GetOrCreate(POLICE).SetOnDuty(true);

		// Item 1: arming the bank door takes the lock while the fuse still runs.
		if (!RedProof())
			ctx.True(LF_Breach.Arm(BANK_DOOR, CREW_ONE), "crew one arms the bank door");

		ctx.False(LF_Breach.IsOpen(BANK_DOOR), "the fuse still runs: the bank door is shut");
		ctx.EqualStr("armlock-bank", LF_BigJobLock.RunningJob(t), "arming took the server-wide lock for the bank");
		ctx.False(LF_Breach.Arm(DEPOT_DOOR, CREW_TWO), "a second crew inside the first fuse window is refused");
		ctx.EqualStr("armlock-bank", LF_BigJobLock.RunningJob(t), "the refused arm leaves the bank holding the lock");

		// Item 3: the refusal says the real reason.
		string why;
		ctx.EqualStr(LF_BigJobLock.RefusalKey(LF_EBigJobLock.BUSY), LF_HeistRun.ArmRefusalWhy(DEPOT_DOOR, 5, why), "the depot door is refused as busy");
		ctx.True(why.Contains("big job armlock-bank is running"), "the busy reason names the running job: " + why);
		ctx.EqualStr(LF_HeistRun.NEED_POLICE, LF_HeistRun.ArmRefusalWhy(DEPOT_DOOR, 1, why), "one officer: the police minimum refuses first");
		ctx.True(why.Contains("too few police"), "the police reason says so: " + why);
		ctx.EqualStr("", LF_HeistRun.ArmRefusalWhy("armlock-test-no-job", 0, why), "a door that is no job's door is never refused");
		ctx.EqualStr("", why, "and has no reason");

		// The door opening hands the lock to the run; the run's end starts the row's cooldown.
		LF_HeistRun run = LF_HeistRun.FromRow("armlock-bank-site", bank, "0 0 0", 40);
		ctx.NotNull(run, "the bank row builds a run");
		if (run)
		{
			ctx.True(run.DoorOpened(CREW_ONE, 1000), "the door opening starts the run under the lock taken at arming");
			ctx.True(run.HoldsBigJobLock(), "the run holds the lock");
			run.Stop();
		}

		ctx.EqualStr("", LF_BigJobLock.RunningJob(t), "the stopped run freed the lock");
		ctx.Equal(BANK_COOLDOWN, LF_BigJobLock.CooldownLeft("armlock-bank", t), "the bank cools down for its row's 1 h");
		string cooling = LF_HeistRun.ArmRefusalWhy(BANK_DOOR, 5, why);
		ctx.True(cooling.Contains("60"), "the cooldown refusal names 60 minutes: " + cooling);
		ctx.True(why.Contains("cooling down"), "the cooldown reason says so: " + why);
		LF_Breach.Reset(BANK_DOOR);

		// A charge defused before its fuse runs out ends the job taken at arming.
		ctx.True(LF_Breach.Arm(DEPOT_DOOR, CREW_TWO), "with the lock free the depot door arms");
		ctx.EqualStr("armlock-depot", LF_BigJobLock.RunningJob(t), "the depot holds the lock from arming");
		ctx.True(LF_Breach.Repair(DEPOT_DOOR, POLICE), "the police defuse the depot charge");
		ctx.EqualStr("", LF_BigJobLock.RunningJob(t), "the defused charge freed the lock");
		ctx.Equal(LF_BigJobLock.COOLDOWN_BANK, LF_BigJobLock.CooldownLeft("armlock-depot", t), "the depot cools down for the 3 h default");

		LF_Breach.Reset(DEPOT_DOOR);
		LF_BigJobLock.ResetStatics();
		LF_HeistRun.ResetStatics();
	}
}
