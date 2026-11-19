// red-proof: Observed 2026-11-19 with `tools/cli test --no-build --red LF_Test_PrisonEscape`: RED PROOF OK with the recapture arrest skipped: `the runner serves the rest plus the escape term (expected 380, got 80)`, `a recaptured prisoner is not arrested a third time`. The unhooked run passes (3 passed with LF_Test_PrisonBreak). Review 2026-11-19: "one escape is one dispatch call (expected 3, got 4)" and "one escape is one wanted row (expected 1, got 2)" failed on the unfixed LF_Jail.NoteEscape (jailbreak report plus Escaped custody), pass with one report.
//! The search afterwards (#3422): an escapee is on the wanted board as "Escaped custody", an officer can
//! arrest them again (back to the cell with the rest of the sentence plus the escape term, the status
//! cleared), and the member who opened the door has the jailbreak on their record and is wanted.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PrisonEscape : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/prison-escape";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string officerUid = "prison-escape-officer";
		string starterUid = "prison-escape-starter";
		string runnerUid = "prison-escape-runner";
		string stayerUid = "prison-escape-stayer";
		string doorId = "prison-escape-door";
		vector placed = "48 0 64";
		vector outside = "48 0 90";

		LF_PrisonBreakRules.ResetStatics();
		LF_PrisonBreak.ResetStatics();

		LF_PrisonBreakConfig config = LF_PrisonBreakConfig.CreateDefault();
		config.m_bEnabled = true;
		config.m_iMinOfficersOnDuty = 0;
		config.m_iEscapeTermSeconds = 300;
		LF_PrisonBreakConfig.UseForTest(config);

		LF_Jail.Configure(40, 200, 1, 4);
		LF_Jail.SetCellOrigin(placed);
		LF_Jail.SetDoorTarget(doorId);
		LF_Breach.Configure(doorId, 0, "");

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		officer.SetPoliceRank(LF_PoliceRanks.SENIOR);
		LF_PlayerAccount starter = LF_PlayerAccountManager.GetOrCreate(starterUid);
		starter.SetWantedLevel(0);
		LF_PlayerAccountManager.GetOrCreate(runnerUid).SetWantedLevel(2);
		LF_PlayerAccountManager.GetOrCreate(stayerUid).SetWantedLevel(2);

		LF_Organization org = LF_OrganizationManager.Create(starterUid, "Escape Crew");
		ctx.NotNull(org, "the starter founds an organisation");
		if (!org)
			return;

		ctx.True(LF_OrganizationManager.JoinDirect(org.GetId(), runnerUid, LF_OrganizationConfig.RANK_MEMBER), "the runner is a member");
		ctx.True(LF_OrganizationManager.JoinDirect(org.GetId(), stayerUid, LF_OrganizationConfig.RANK_MEMBER), "the stayer is a member");
		ctx.True(LF_Jail.Arrest(officerUid, runnerUid, null), "the runner is jailed");
		ctx.True(LF_Jail.Arrest(officerUid, stayerUid, null), "the stayer is jailed");
		ctx.EqualFloat(80, LF_Jail.GetRemaining(runnerUid), 0.01, "the runner serves two wanted steps");

		// The member opens the door: the jailbreak is on their record and they are wanted for it.
		int now = System.GetUnixTime();
		ctx.True(LF_Breach.Arm(doorId, starterUid), "a free member forces the door");
		ctx.True(LF_PrisonBreak.IsWindowOpen(now), "the break window is open");
		ctx.True(LF_CrimeRecord.Count(starterUid, LF_WantedReports.OFFENCE_JAILBREAK) >= 1, "the opener has the jailbreak on their record");
		ctx.True(starter.GetWantedLevel() >= 1, "the opener is wanted");
		ctx.True(LF_PrisonEscape.HasWantedRow(starterUid, LF_WantedReports.OFFENCE_JAILBREAK), "the opener is on the wanted board for the jailbreak");

		// The runner walks out: the escape is a wanted row, the stayer has none.
		ctx.False(LF_PrisonEscape.HasWantedRow(runnerUid, LF_PrisonEscape.OFFENCE_ESCAPED), "nothing is posted before the walk");
		int callsBefore = LF_Dispatch.CallCount();
		int rowsBefore = LF_WantedReports.CountFor(runnerUid);
		ctx.True(LF_Jail.NoteEscape(runnerUid, outside, null), "the runner walks out through the open door");
		ctx.Equal(callsBefore + 1, LF_Dispatch.CallCount(), "one escape is one dispatch call");
		ctx.Equal(rowsBefore + 1, LF_WantedReports.CountFor(runnerUid), "one escape is one wanted row");
		ctx.False(LF_Jail.NoteEscape(runnerUid, outside, null), "standing outside does not report the escape again");
		ctx.Equal(callsBefore + 1, LF_Dispatch.CallCount(), "no second call while out");
		ctx.True(LF_PrisonEscape.HasWantedRow(runnerUid, LF_PrisonEscape.OFFENCE_ESCAPED), "the escapee is on the wanted board as Escaped custody");
		ctx.False(LF_PrisonEscape.HasWantedRow(stayerUid, LF_PrisonEscape.OFFENCE_ESCAPED), "the stayer is not posted");
		ctx.True(LF_PrisonBreak.IsEscaped(runnerUid), "the runner is escaped");
		ctx.True(LF_CrimeRecord.Count(runnerUid, LF_PrisonEscape.OFFENCE_ESCAPED) >= 1, "the escape is on the runner's record");

		// The window ends and the door locks. Whoever is still serving inside cannot be arrested again.
		ctx.False(LF_PrisonBreak.Tick(now + 100000), "the window is over");
		ctx.False(LF_Jail.Arrest(officerUid, stayerUid, null), "a prisoner inside the cell is not arrested again");
		ctx.EqualFloat(80, LF_Jail.GetRemaining(stayerUid), 0.01, "the stayer's sentence is untouched");

		// An officer arrests the escapee: the rest plus the escape term, back in the cell, the status gone.
		// RedProof(): the arrest is never made, so the sentence and status assertions fail (`--red`).
		if (!RedProof())
			ctx.True(LF_Jail.Arrest(officerUid, runnerUid, null), "an officer arrests the escapee again");

		ctx.False(LF_PrisonBreak.IsEscaped(runnerUid), "the recapture clears the escaped status");
		ctx.True(LF_Jail.IsServing(runnerUid), "the runner serves again");
		ctx.EqualFloat(80 + LF_PrisonEscape.TermSeconds(), LF_Jail.GetRemaining(runnerUid), 0.01, "the runner serves the rest plus the escape term");
		ctx.True(LF_Jail.Confines(runnerUid), "the locked door confines the recaptured runner");
		ctx.False(LF_Jail.Arrest(officerUid, runnerUid, null), "a recaptured prisoner is not arrested a third time");
		ctx.EqualFloat(80 + LF_PrisonEscape.TermSeconds(), LF_Jail.GetRemaining(runnerUid), 0.01, "a refused arrest adds no time");

		// Clean up what this test made.
		LF_Jail.Advance(runnerUid, 100000);
		LF_Jail.Advance(stayerUid, 1000);
		LF_OrganizationManager.LeaveDirect(runnerUid);
		LF_OrganizationManager.LeaveDirect(stayerUid);
		LF_OrganizationManager.LeaveDirect(starterUid);
		starter.SetWantedLevel(0);
		LF_PrisonBreak.ResetStatics();
		LF_PrisonBreakRules.ResetStatics();
		LF_PrisonBreakConfig.Reload();
		LF_Jail.SyncPlacedCell();
	}
}
