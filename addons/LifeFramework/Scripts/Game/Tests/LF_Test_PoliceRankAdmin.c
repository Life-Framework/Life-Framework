// red-proof: 2026-11-18 `tools/cli test --no-build --red LF_Test_PoliceRankAdmin`: RED PROOF OK (the hook skips both SetRank calls, so the promotion and demotion assertions fail; log tmp/logs/test-20261118-051028.log). Green with LF_PoliceRankAdmin.
//! police/rank-admin-set (#2838): an administrator sets a police rank up or down through one server
//! path. A caller who is not an administrator, an unknown account, a malformed line and a rank off the
//! ladder are refused and change nothing. A demotion sticks: the next duty accrual does not promote
//! the officer straight back.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PoliceRankAdmin : LF_Test
{
	protected static const string ADMIN = "rank-admin-caller";
	protected static const string TARGET = "rank-admin-target";
	protected static const string NOBODY_ROSTER = "$profile:LifeFramework/test-rank-admin-no-staff.txt";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/rank-admin-set";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PoliceApplicationStaff.SetStaffPath(NOBODY_ROSTER);
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(TARGET);
		account.AddCharacter(LF_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "Rank", "Target", 30), true);
		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDutySeconds(0);
		int top = LF_PoliceRanks.TopRank();
		ctx.True(top >= 1, "the ladder has a rank above patrol");

		// Refusals change nothing.
		ctx.EqualStr(LF_PoliceRankAdmin.REFUSED_DENIED, LF_PoliceRankAdmin.SetRank(ADMIN, false, TARGET, top), "a caller off the roster and not an engine admin is refused");
		ctx.EqualStr(LF_PoliceRankAdmin.REFUSED_TARGET, LF_PoliceRankAdmin.SetRank(ADMIN, true, "rank-admin-nobody", top), "an unknown account is refused");
		ctx.EqualStr(LF_PoliceRankAdmin.REFUSED_TARGET, LF_PoliceRankAdmin.SetRank(ADMIN, true, "bad uid!", top), "a malformed account id is refused");
		ctx.EqualStr(LF_PoliceRankAdmin.REFUSED_RANK, LF_PoliceRankAdmin.SetRank(ADMIN, true, TARGET, top + 1), "a rank above the ladder is refused");
		ctx.EqualStr(LF_PoliceRankAdmin.REFUSED_RANK, LF_PoliceRankAdmin.SetRank(ADMIN, true, TARGET, -1), "a negative rank is refused");
		ctx.Equal(LF_PoliceRanks.PATROL, account.GetPoliceRank(), "no refusal changed the rank");

		string uid;
		int rank;
		ctx.True(LF_PoliceRankAdmin.ParseArgs("  " + TARGET + " 1 ", uid, rank), "the chat line '<uid> <rank>' parses");
		ctx.EqualStr(TARGET, uid, "the parsed uid");
		ctx.Equal(1, rank, "the parsed rank");
		ctx.False(LF_PoliceRankAdmin.ParseArgs(TARGET, uid, rank), "a line with no rank does not parse");
		ctx.False(LF_PoliceRankAdmin.ParseArgs(TARGET + " one", uid, rank), "a rank that is not a number does not parse");

		// Promotion to the top, then demotion back to patrol.
		string refusal = "not run";
		if (!RedProof())
			refusal = LF_PoliceRankAdmin.SetRank(ADMIN, true, TARGET, top);
		ctx.EqualStr("", refusal, "an engine admin sets the top rank");
		ctx.Equal(top, account.GetPoliceRank(), "the target holds the top rank");

		account.SetOnDutySeconds(LF_PoliceRanks.DutySecondsForNext(LF_PoliceRanks.PATROL) + 5000);
		refusal = "not run";
		if (!RedProof())
			refusal = LF_PoliceRankAdmin.SetRank(ADMIN, true, TARGET, LF_PoliceRanks.PATROL);
		ctx.EqualStr("", refusal, "an engine admin demotes to patrol");
		ctx.Equal(LF_PoliceRanks.PATROL, account.GetPoliceRank(), "the target is patrol again");

		account.SetFaction(LF_Faction.POLICE);
		LF_PoliceRanks.AccrueDuty(TARGET, 1);
		ctx.Equal(LF_PoliceRanks.PATROL, account.GetPoliceRank(), "the next duty second does not undo the demotion");
		ctx.Equal(1, account.GetOnDutySeconds(), "a demoted officer's rank time starts over");

		account.SetFaction(LF_Faction.CIVILIAN);
		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDutySeconds(0);
		LF_PoliceApplicationStaff.SetStaffPath("");
	}
}
