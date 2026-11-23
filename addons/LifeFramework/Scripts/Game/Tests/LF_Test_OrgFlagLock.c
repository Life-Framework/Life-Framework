// red-proof: Observed 2026-11-23: against a stub LF_OrgFlagRules that refuses nothing and never flies a plain cloth, `--only LF_Test_OrgFlagLock` failed 6 assertions (the refused picks, "the second gang did not get the flag", the plain cloth, the unknown organization); with the rules written it passes.
//! The gang flag lock (#4220, gangs C3, decision N21): the first gang keeps its flag; a second gang may
//! not pick it while a member of the first is online; when both are online the later gang flies a plain
//! cloth in its colour. "Online" is a list of character ids, so the rules need no players. The test puts
//! back the organizations and flags it found.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OrgFlagLock : LF_Test
{
	protected static const string ORG_A = "flaglock-test-a";
	protected static const string ORG_B = "flaglock-test-b";
	protected static const string CHAR_A = "flaglock-char-a";
	protected static const string CHAR_B = "flaglock-char-b";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/flag-lock";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		array<ref LF_OrgFlagRow> keptFlags = LF_OrgFlags.ExportAll();

		CheckFirstKeepsFlag(ctx);
		CheckHolderOffline(ctx);
		CheckBothOnline(ctx);
		CheckEdges(ctx);

		LF_OrganizationManager.Reset();
		kept.Restore();
		LF_OrgFlags.ResetStatics();
		LF_OrgFlagRules.ResetStatics();
		LF_OrgFlags.ApplyAll(keptFlags);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_OrganizationRecord Record(string id, string name, string founder)
	{
		LF_OrganizationRecord record = LF_OrganizationRecord.Create(id, name, 0, false, false, false);
		record.m_aRanks.Insert(LF_OrganizationRankRecord.Create(LF_OrganizationConfig.RANK_FOUNDER, "Founder", true, true, true, true));
		record.m_aMembers.Insert(LF_OrganizationMemberRecord.Create(founder, LF_OrganizationConfig.RANK_FOUNDER));
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Organizations A and B on the register, empty flag table, empty claim order.
	protected static void Register()
	{
		array<ref LF_OrganizationRecord> records = {};
		records.Insert(Record(ORG_A, "Flag Lock A", CHAR_A));
		records.Insert(Record(ORG_B, "Flag Lock B", CHAR_B));
		LF_OrganizationManager.Reset();
		LF_OrganizationManager.ApplyAll(records);
		LF_OrgFlags.ResetStatics();
		LF_OrgFlagRules.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected static array<string> Online(bool a, bool b)
	{
		array<string> online = {};
		if (a)
			online.Insert(CHAR_A);
		if (b)
			online.Insert(CHAR_B);

		return online;
	}

	//------------------------------------------------------------------------------------------------
	//! Case 1: A picks flag 5; while A is online B cannot take it, and A keeps it.
	protected void CheckFirstKeepsFlag(LF_TestContext ctx)
	{
		Register();
		array<string> aOnly = Online(true, false);
		array<string> both = Online(true, true);

		ctx.True(LF_OrgFlagRules.Pick(ORG_A, 5, aOnly), "the first gang picks a free flag");
		ctx.Equal(5, LF_OrgFlags.FlagOf(ORG_A), "the first gang flies flag 5");
		ctx.False(LF_OrgFlagRules.CanPick(ORG_B, 5, aOnly), "a second gang may not pick the flag while a member of the first is online");
		ctx.False(LF_OrgFlagRules.Pick(ORG_B, 5, aOnly), "the pick is refused");
		ctx.False(LF_OrgFlagRules.Pick(ORG_B, 5, both), "the pick is refused with both online too");
		ctx.Equal(5, LF_OrgFlags.FlagOf(ORG_A), "the first gang keeps its flag after the refused picks");
		ctx.True(LF_OrgFlags.FlagOf(ORG_B) != 5, "the second gang did not get the flag");
		ctx.True(LF_OrgFlagRules.CanPick(ORG_B, 6, aOnly), "a free flag is still open to the second gang");
		ctx.True(LF_OrgFlagRules.Pick(ORG_A, 5, aOnly), "the holder may pick its own flag again");
	}

	//------------------------------------------------------------------------------------------------
	//! Case 2: A holds flag 5 and nobody of A is online: B may take the same flag.
	protected void CheckHolderOffline(LF_TestContext ctx)
	{
		Register();
		array<string> none = Online(false, false);
		array<string> bOnly = Online(false, true);

		ctx.True(LF_OrgFlagRules.Pick(ORG_A, 5, none), "the first gang picks flag 5");
		ctx.True(LF_OrgFlagRules.CanPick(ORG_B, 5, bOnly), "the second gang may pick the flag while no member of the first is online");
		ctx.True(LF_OrgFlagRules.Pick(ORG_B, 5, bOnly), "the second gang takes the same flag");
		ctx.Equal(5, LF_OrgFlags.FlagOf(ORG_A), "the first gang still flies flag 5");
		ctx.Equal(5, LF_OrgFlags.FlagOf(ORG_B), "the second gang flies flag 5 too");
		ctx.False(LF_OrgFlagRules.FliesPlainCloth(ORG_B, bOnly), "no plain cloth while the first gang is offline");
		ctx.False(LF_OrgFlagRules.FliesPlainCloth(ORG_A, bOnly), "the first gang flies its flag");
	}

	//------------------------------------------------------------------------------------------------
	//! Case 3: both gangs hold flag 5 and both are online: the later gang flies a plain cloth.
	protected void CheckBothOnline(LF_TestContext ctx)
	{
		Register();
		array<string> none = Online(false, false);
		array<string> both = Online(true, true);
		array<string> aOnly = Online(true, false);
		array<string> bOnly = Online(false, true);

		LF_OrgFlagRules.Pick(ORG_A, 5, none);
		LF_OrgFlagRules.Pick(ORG_B, 5, none);

		ctx.True(LF_OrgFlagRules.FliesPlainCloth(ORG_B, both), "both online: the later gang flies a plain cloth");
		ctx.False(LF_OrgFlagRules.FliesPlainCloth(ORG_A, both), "both online: the first gang keeps its flag");
		ctx.False(LF_OrgFlagRules.FliesPlainCloth(ORG_B, bOnly), "only the later gang online: its flag flies");
		ctx.False(LF_OrgFlagRules.FliesPlainCloth(ORG_B, aOnly), "only the first gang online: the later gang has nobody to fly it");
		ctx.Equal(5, LF_OrgFlags.FlagOf(ORG_B), "the later gang still owns its flag number");

		// Two gangs on different flags never share a cloth.
		LF_OrgFlagRules.Pick(ORG_B, 9, none);
		ctx.False(LF_OrgFlagRules.FliesPlainCloth(ORG_B, both), "different flags: no plain cloth");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckEdges(LF_TestContext ctx)
	{
		Register();
		array<string> both = Online(true, true);

		ctx.False(LF_OrgFlagRules.Pick(ORG_A, 0, both), "no flag is not a pick");
		ctx.False(LF_OrgFlagRules.Pick(ORG_A, LF_OrgFlags.COUNT + 1, both), "a flag past the presets is refused");
		ctx.False(LF_OrgFlagRules.Pick("flaglock-no-such-org", 3, both), "an organization not on the register picks nothing");
		ctx.False(LF_OrgFlagRules.CanPick("flaglock-no-such-org", 3, both), "an unknown organization cannot pick");
		ctx.False(LF_OrgFlagRules.FliesPlainCloth("flaglock-no-such-org", both), "an unknown organization has no cloth");
	}
}
