// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_PoliceRankPermissions`
// and LF_Police.conf set to m_iArrestMinRank 1 (the old senior-only rule):
// [LFTEST] FAIL police/rank-permissions
// [LFTEST]   - the flagship lets every officer arrest (expected 0, got 1)
// [LFTEST]   - the server lets a new officer arrest
// [LFTEST]   - a new officer jails a wanted robber
// LF_Jail logged "arrest refused: rank 0 is below the arrest rank 1". Restored to 0; green.

// tier: LOGIC
class LF_Test_PoliceRankPermissions : LF_Test
{
	override string GetName()
	{
		return "police/rank-permissions";
	}

	override void Run(LF_TestContext ctx)
	{
		string officerUid = "rank-perm-officer";
		string robberUid = "rank-perm-robber";
		string runnerUid = "rank-perm-runner";
		vector cell = "48 0 64";

		ctx.NotNull(LF_PoliceConfig.Get(), "the police config loads");
		ctx.Equal(LF_PoliceRanks.PATROL, LF_PoliceConfig.ArrestMinRank(), "the flagship lets every officer arrest");
		ctx.Equal(LF_PoliceRanks.PATROL, LF_PoliceConfig.EscortMinRank(), "the flagship lets every officer escort");
		ctx.Equal(1000, LF_PoliceConfig.PatrolFineCap(), "the patrol fine cap comes from the config");

		// The framework range: a server may still keep arrest for senior officers.
		ctx.False(LF_PoliceRanks.RankMeets(LF_PoliceRanks.PATROL, LF_PoliceRanks.SENIOR), "a senior-only rule refuses a patrol officer");
		ctx.True(LF_PoliceRanks.RankMeets(LF_PoliceRanks.SENIOR, LF_PoliceRanks.SENIOR), "a senior-only rule allows a senior officer");

		// A fresh officer on a fresh server: patrol rank, no time on duty.
		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetPoliceRank(LF_PoliceRanks.PATROL);
		officer.SetOnDuty(true);
		ctx.Equal(0, officer.GetOnDutySeconds(), "the officer has no time on duty");

		ctx.True(LF_PoliceRanks.CanArrestRank(officer.GetPoliceRank()), "the offer check lets a new officer arrest");
		ctx.True(LF_PoliceRanks.CanEscortRank(officer.GetPoliceRank()), "the offer check lets a new officer escort");
		ctx.True(LF_PoliceRanks.AuthorizeArrest(officer), "the server lets a new officer arrest");

		LF_Jail.Configure(60, 300, 1, 8);
		LF_Jail.SetCellOrigin(cell);

		LF_PlayerAccount robber = LF_PlayerAccountManager.GetOrCreate(robberUid);
		robber.SetWantedLevel(1);
		ctx.True(LF_Jail.Arrest(officerUid, robberUid, null), "a new officer jails a wanted robber");
		ctx.True(LF_Jail.IsServing(robberUid), "the robber is serving");
		LF_Jail.Advance(robberUid, 300);

		LF_PlayerAccount runner = LF_PlayerAccountManager.GetOrCreate(runnerUid);
		runner.SetWantedLevel(1);
		ctx.True(LF_JailEscort.Escort(officerUid, runnerUid), "a new officer escorts a wanted player");
		LF_JailEscort.ReleasePlayer(runnerUid);

		// Rank still gates the fine cap.
		ctx.False(LF_PoliceRanks.AuthorizeFine(officer, LF_PoliceConfig.PatrolFineCap() + 1), "rank still caps a patrol officer's fine");

		LF_Jail.SyncPlacedCell();
	}
}
