// red-proof: with `RedProof()` on the claim order is not asked, so every answer keeps its wrong default and the "no claim",
// "claimed later", "re-pick" and "new flag" checks fail.

//! org/flag-claim-order: who flew a gang flag first (LF_OrgFlagRules.ClaimedBefore), the order the plain-cloth rule reads.
//! With no claim yet two gangs rank by id (so every machine agrees), a gang that never claimed counts as earlier than one that
//! did, the later of two claims is later, re-picking the flag a gang already flies keeps its place, taking a different flag
//! moves it to the back, and a world-end reset forgets the order. The test puts back the organizations and flags it found.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OrgFlagClaimOrder : LF_Test
{
	protected static const string ORG_A = "flagorder-test-a";
	protected static const string ORG_B = "flagorder-test-b";
	protected static const string CHAR_A = "flagorder-char-a";
	protected static const string CHAR_B = "flagorder-char-b";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/flag-claim-order";
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
	override void Run(LF_TestContext ctx)
	{
		bool noClaimAFirst = false;
		bool noClaimBFirst = true;
		bool unclaimedBeforeClaimed = false;
		bool claimedAfterUnclaimed = true;
		bool aThenB = false;
		bool bThenA = true;
		bool aStillFirst = false;
		bool aMovedBack = false;
		bool bNowFirst = false;
		bool afterReset = false;
		if (!RedProof())
		{
			LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
			array<ref LF_OrgFlagRow> keptFlags = LF_OrgFlags.ExportAll();
			array<ref LF_OrganizationRecord> records = {};
			records.Insert(Record(ORG_A, "Flag Order A", CHAR_A));
			records.Insert(Record(ORG_B, "Flag Order B", CHAR_B));
			LF_OrganizationManager.Reset();
			LF_OrganizationManager.ApplyAll(records);
			LF_OrgFlags.ResetStatics();
			LF_OrgFlagRules.ResetStatics();
			array<string> none = {};

			noClaimAFirst = LF_OrgFlagRules.ClaimedBefore(ORG_A, ORG_B);
			noClaimBFirst = LF_OrgFlagRules.ClaimedBefore(ORG_B, ORG_A);

			LF_OrgFlagRules.Pick(ORG_A, 3, none);
			unclaimedBeforeClaimed = LF_OrgFlagRules.ClaimedBefore(ORG_B, ORG_A);
			claimedAfterUnclaimed = LF_OrgFlagRules.ClaimedBefore(ORG_A, ORG_B);

			LF_OrgFlagRules.Pick(ORG_B, 4, none);
			aThenB = LF_OrgFlagRules.ClaimedBefore(ORG_A, ORG_B);
			bThenA = LF_OrgFlagRules.ClaimedBefore(ORG_B, ORG_A);

			LF_OrgFlagRules.Pick(ORG_A, 3, none);
			aStillFirst = LF_OrgFlagRules.ClaimedBefore(ORG_A, ORG_B);

			LF_OrgFlagRules.Pick(ORG_A, 6, none);
			aMovedBack = LF_OrgFlagRules.ClaimedBefore(ORG_B, ORG_A);
			bNowFirst = !LF_OrgFlagRules.ClaimedBefore(ORG_A, ORG_B);

			LF_OrgFlagRules.ResetStatics();
			afterReset = LF_OrgFlagRules.ClaimedBefore(ORG_A, ORG_B);

			LF_OrganizationManager.Reset();
			kept.Restore();
			LF_OrgFlags.ResetStatics();
			LF_OrgFlagRules.ResetStatics();
			LF_OrgFlags.ApplyAll(keptFlags);
		}

		ctx.True(noClaimAFirst, "with no claims the lower id ranks first");
		ctx.False(noClaimBFirst, "with no claims the higher id ranks second");
		ctx.True(unclaimedBeforeClaimed, "a gang that never claimed is earlier than one that did");
		ctx.False(claimedAfterUnclaimed, "a gang that claimed is later than one that never did");
		ctx.True(aThenB, "the first of two claims is the earlier one");
		ctx.False(bThenA, "the second of two claims is the later one");
		ctx.True(aStillFirst, "re-picking the flag a gang already flies keeps its place in the order");
		ctx.True(aMovedBack, "taking a different flag moves the gang to the back of the order");
		ctx.True(bNowFirst, "the other gang is now first");
		ctx.True(afterReset, "a reset forgets the claims and ranks by id again");
	}
}
