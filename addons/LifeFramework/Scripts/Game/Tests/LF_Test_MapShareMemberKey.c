// red-proof: 2026-11-18, cli test --no-build --red (batched): RED-OK, "a PLAYER row under the record key reaches the first character" fails with the member keyed by character id. Green with MemberOf keying by record key.
//------------------------------------------------------------------------------------------------
//! map/share-member-key (review 2026-11-17, docs/reviews/2026-11-17-map.md): the job marks put a PLAYER
//! row under the runner's record key (LF_Utils.GetPlayerUID). The share table's member used the
//! character id, which differs from the record key for an account's first character (its record key
//! is the account uid), so that character never got its job stop mark or waypoint. The organization
//! must still resolve by character id (LF_Test_OrgMarkCharacterId).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapShareMemberKey : LF_Test
{
	protected static const string ACCOUNT_UID = "sharekey-account";
	protected static const string ORG_NAME = "Share Key Crew";

	override string GetName()
	{
		return "map/share-member-key";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_OrganizationRecord> orgsBefore = LF_OrganizationManager.ExportAll();
		array<ref LF_OrganizationRankRow> ranksBefore = LF_OrganizationManager.ExportRanks();
		LF_OrganizationManager.Reset();

		LF_PlayerAccount account = LF_PlayerAccount.Create(ACCOUNT_UID);
		LF_PlayerCharacter character = LF_PlayerCharacter.Create("", "Jo", "Route", 30);
		account.AddCharacter(character, true);
		LF_Organization org = LF_OrganizationManager.Create(character.GetId(), ORG_NAME);
		ctx.NotNull(org, "the character founds an organization");

		LF_MapShareMember member = LF_MapShareGameFacts.MemberOf(7, account);
		ctx.NotNull(member, "an account with a character is a member");
		string recordKey = account.GetActiveCharacterKey();
		ctx.EqualStr(recordKey, ACCOUNT_UID, "the first character's record key is the account uid");

		// The job marks' row: PLAYER, keyed by the runner's record key.
		bool reached = LF_MapShareRules.InAudience(LF_EMapShareAudience.PLAYER, recordKey, member);
		if (RedProof())
			reached = LF_MapShareRules.InAudience(LF_EMapShareAudience.PLAYER, recordKey, LF_MapShareMember.Make(7, character.GetId(), null, ""));

		ctx.True(reached, "a PLAYER row under the record key reaches the first character");
		ctx.True(org && member.m_sOrgId == org.GetId(), "the organization still resolves by character id");
		ctx.True(LF_MapShareRules.InAudience(LF_EMapShareAudience.ORG, member.m_sOrgId, member), "the org's marks reach the member");
		ctx.True(!LF_MapShareGameFacts.MemberOf(7, null), "no account is no member");

		LF_OrganizationManager.Reset();
		LF_OrganizationManager.ApplyAll(orgsBefore);
		LF_OrganizationManager.ApplyRanks(ranksBefore);
	}
}
