// red-proof: 2026-11-17, cli test --no-build --red LF_Test_OrgMarkCharacterId,LF_Test_HouseIdentifierInit,LF_Test_WhitelistRemoveUnloaded: RED PROOF OK with the hook resolving by record key ("the mark asker resolves to the character id (expected 'orgmark-charid-account', got '01a1e959-...')", "the resolved id finds the character's organization"). Green with the fix.
// tier: LOGIC
//! map/org-marks-character-id (review 2026-11-17): the org mark RPCs resolve the asking player to the
//! id organizations list members under. An account's first character has the account uid as its record
//! key, so resolving by LF_Utils.GetPlayerUID found no organization and refused every mark that
//! character asked for. The organization manager's singleton is also dropped at world end.
class LF_Test_OrgMarkCharacterId : LF_Test
{
	protected static const string ACCOUNT_UID = "orgmark-charid-account";
	protected static const string ORG_NAME = "Charid Mark Crew";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/org-marks-character-id";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_OrganizationRecord> orgsBefore = LF_OrganizationManager.ExportAll();
		array<ref LF_OrganizationRankRow> ranksBefore = LF_OrganizationManager.ExportRanks();
		LF_OrganizationManager.Reset();

		LF_PlayerAccount account = LF_PlayerAccount.Create(ACCOUNT_UID);
		LF_PlayerCharacter character = LF_PlayerCharacter.Create("", "Ann", "Mark", 30);
		account.AddCharacter(character, true);
		ctx.EqualStr(account.GetActiveCharacterKey(), ACCOUNT_UID, "the first character's record key is the account uid");

		LF_Organization org = LF_OrganizationManager.Create(character.GetId(), ORG_NAME);
		ctx.NotNull(org, "the character founds an organization");

		string resolved = LF_OrgMapMarks.CharacterIdOfAccount(account);
		if (RedProof())
			resolved = account.GetActiveCharacterKey();

		ctx.EqualStr(resolved, character.GetId(), "the mark asker resolves to the character id");
		LF_Organization found = LF_OrganizationManager.GetByCharacter(resolved);
		ctx.True(found && found == org, "the resolved id finds the character's organization");
		ctx.EqualStr(LF_OrgMapMarks.CharacterIdOfAccount(null), "", "no account resolves to no character");
		ctx.True(LF_StaticReset.Registered().Contains("LF_OrganizationManager"), "a world end resets the organization manager");

		LF_OrganizationManager.Reset();
		LF_OrganizationManager.ApplyAll(orgsBefore);
		LF_OrganizationManager.ApplyRanks(ranksBefore);
	}
}
