// red-proof: observed 2026-11-11 with `tools\cli test --no-build --only LF_Test_CharactersPerAccount,LF_Test_CharacterSlotsPersist`:
// LF_PlayerAccount.AddCharacter giving every character the account uid (`if (first || true)`) failed
// `- slot 2 is keyed by its own character id (expected '01a1c84b-...', got 'cpa-account-uid')` and
// `- slot 2's wanted level is kept while slot 1 plays (expected 4, got 2)`. Restored; green.
// Review of #720, observed 2026-11-11 with the pick check ignoring `offered` and
// LF_PoliceApplications.ApplicantUid returning GetPlayerUID: `- a pick the server did not offer is
// refused (expected '#LF-CharSel_Refused', got '')`, `- the application is filed under the account uid
// (expected '00bbbddd-...', got '01a1c860-...')`, `- slot 2 is on the roster after approval`. Fixed; green.

// tier: LOGIC
//! #331: with characters.maxPerAccount = 2 an account holds two characters. Slot 1 is keyed by the
//! account uid, slot 2 by its own id; wanted level and police rank belong to the character; the
//! active character's key is what LF_Utils.GetPlayerUID resolves to, and that key finds the account.
//! A third character is refused, and at maxPerAccount = 1 a second one is refused as before.
class LF_Test_CharactersPerAccount : LF_Test
{
	protected static const string UID = "cpa-account-uid";
	protected static const ResourceName PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "account/characters-per-account";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_CharacterConfig.SetMaxPerAccountOverride(2);
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = LF_PlayerAccount.Create(UID);
		accounts.AddToCache(account);

		// Standing set before any character (an account the old flow made) goes to slot 1.
		account.SetWantedLevel(2);
		ctx.EqualStr(UID, account.GetActiveCharacterKey(), "an account with no character keys records by its uid");
		ctx.EqualStr("", LF_CharacterCreationRules.RefuseCharacter(account, "Ada", "Lovelace", 36), "the first character registers");

		LF_PlayerCharacter first = LF_PlayerCharacter.Create(PREFAB, "Ada", "Lovelace", 36);
		account.AddCharacter(first, true);
		ctx.EqualStr(UID, first.GetRecordKey(), "slot 1 is keyed by the account uid");
		ctx.Equal(2, account.GetWantedLevel(), "slot 1 keeps the wanted level the account held");

		ctx.EqualStr("", LF_CharacterCreationRules.RefuseCharacter(account, "Bob", "Builder", 40), "maxPerAccount 2 lets a second character register");
		LF_PlayerCharacter second = LF_PlayerCharacter.Create(PREFAB, "Bob", "Builder", 40);
		account.AddCharacter(second, true);
		ctx.Equal(2, account.GetCharacterCount(), "the account holds two characters");
		ctx.EqualStr(second.GetId(), second.GetRecordKey(), "slot 2 is keyed by its own character id");
		ctx.False(second.GetRecordKey() == UID, "slot 2 never shares the account uid");
		ctx.EqualStr(second.GetRecordKey(), account.GetActiveCharacterKey(), "the active key follows the active character");
		ctx.EqualStr(second.GetRecordKey(), LF_PlayerAccountManager.ResolveCharacterKey(UID), "GetPlayerUID resolves the account to slot 2's key");
		ctx.Equal(0, account.GetWantedLevel(), "slot 2 starts with a clean record");

		account.SetWantedLevel(4);
		account.SetPoliceRank(LF_PoliceRanks.SENIOR);
		account.SetActiveCharacter(first);
		ctx.Equal(2, account.GetWantedLevel(), "slot 1 keeps its own wanted level");
		ctx.Equal(LF_PoliceRanks.PATROL, account.GetPoliceRank(), "slot 1 keeps its own police rank");
		ctx.EqualStr(UID, LF_PlayerAccountManager.ResolveCharacterKey(UID), "back on slot 1, GetPlayerUID is the account uid again");

		ctx.True(accounts.GetFromCache(second.GetRecordKey()) == account, "slot 2's key finds its account");
		ctx.True(accounts.FindCharacterByKey(second.GetRecordKey()) == second, "slot 2's key finds slot 2");
		ctx.True(accounts.FindStanding(UID) == first.GetStanding(), "the uid names slot 1's standing");
		LF_CharacterStanding secondStanding = accounts.FindStanding(second.GetRecordKey());
		ctx.True(secondStanding == second.GetStanding(), "slot 2's key names slot 2's standing");
		if (secondStanding)
		{
			ctx.Equal(4, secondStanding.m_iWantedLevel, "slot 2's wanted level is kept while slot 1 plays");
			ctx.Equal(LF_PoliceRanks.SENIOR, secondStanding.m_iPoliceRank, "slot 2's rank is kept while slot 1 plays");
		}

		ctx.EqualStr("#LF-Char_Error_Full", LF_CharacterCreationRules.RefuseCharacter(account, "Cy", "Clops", 30), "a third character is refused at maxPerAccount 2");

		// The pick: offered with a free choice, never to a living body or an officer on duty.
		ctx.True(LF_CharacterCreationRules.OffersCharacterPick(account, false), "a returning player with two characters picks one");
		ctx.False(LF_CharacterCreationRules.OffersCharacterPick(account, true), "no pick while a living body is controlled");
		ctx.EqualStr("", LF_CharacterCreationRules.RefusePick(account, 1, false, true), "slot 2 can be picked");
		ctx.EqualStr("#LF-CharSel_Refused", LF_CharacterCreationRules.RefusePick(account, 2, false, true), "a slot past the roster is refused");
		ctx.EqualStr("#LF-CharSel_Refused", LF_CharacterCreationRules.RefusePick(account, -7, false, true), "a negative slot from the client is refused");
		ctx.EqualStr("#LF-CharSel_Refused", LF_CharacterCreationRules.RefusePick(account, 0, true, true), "a pick from inside a living body is refused");
		ctx.EqualStr("#LF-Char_Error_Full", LF_CharacterCreationRules.RefusePick(account, LF_CharacterSelectSheet.SLOT_NEW, false, true), "registering a third from the pick is refused");
		// A pick the server never sent (a dead player mid-session) must not switch character.
		ctx.EqualStr("#LF-CharSel_Refused", LF_CharacterCreationRules.RefusePick(account, 0, false, false), "a pick the server did not offer is refused");
		ctx.EqualStr("#LF-CharSel_Refused", LF_CharacterCreationRules.RefuseFurtherCharacter(account, false, false), "a further character without the pick's new-resident line is refused");
		ctx.EqualStr("", LF_CharacterCreationRules.RefuseFurtherCharacter(account, false, true), "a further character chosen on the pick registers");
		ctx.EqualStr("#LF-CharSel_Refused", LF_CharacterCreationRules.RefuseFurtherCharacter(account, true, true), "never from inside a living body");
		ctx.EqualStr("", LF_CharacterCreationRules.RefuseFurtherCharacter(LF_PlayerAccount.Create("cpa-empty-uid"), false, false), "the first character needs no pick");

		// The police roster is a whitelist, so per account: an application filed while slot 2 plays
		// is filed under the account uid, and its approval shows on the roster bit of any character.
		PoliceApplicationIsPerAccount(ctx);

		// The sheet's parts and the part names it sends back.
		ctx.Equal(5, LF_CharacterSelectSheet.BuildParts("Ada Lovelace\nBob Builder", true).Count(), "title, intro, two characters and the new line");
		ctx.Equal(4, LF_CharacterSelectSheet.BuildParts("Ada Lovelace\nBob Builder", false).Count(), "no new line when the slots are full");
		ctx.Equal(1, LF_CharacterSelectSheet.SlotOfPart("CharSlot1"), "CharSlot1 is slot 2");
		ctx.Equal(LF_CharacterSelectSheet.SLOT_NEW, LF_CharacterSelectSheet.SlotOfPart("CharNew"), "CharNew registers a new one");
		ctx.True(LF_CharacterSelectSheet.SlotOfPart("Other") < LF_CharacterSelectSheet.SLOT_NEW, "any other part is no pick");

		// The flagship rule: one character per account.
		LF_CharacterConfig.SetMaxPerAccountOverride(1);
		LF_PlayerAccount single = LF_PlayerAccount.Create("cpa-single-uid");
		single.AddCharacter(LF_PlayerCharacter.Create(PREFAB, "Ann", "Single", 30), true);
		ctx.EqualStr("#EL-CC_Error_Exists", LF_CharacterCreationRules.RefuseCharacter(single, "Bob", "Builder", 40), "maxPerAccount 1 refuses a second character");
		ctx.False(LF_CharacterCreationRules.OffersCharacterPick(single, false), "maxPerAccount 1 never shows the pick");

		LF_CharacterConfig.SetMaxPerAccountOverride(0);
		ctx.Equal(1, LF_CharacterConfig.MaxPerAccount(), "the shipped config allows one character per account");
		accounts.RemoveFromCache(UID);
	}

	//------------------------------------------------------------------------------------------------
	//! A player id with no connected player: on the -noBackend test server its account uid is the
	//! name-derived identity (LF_Utils.BuildLocalIdentity), and GetPlayerUID resolves it through the
	//! account cache like any player's.
	protected void PoliceApplicationIsPerAccount(LF_TestContext ctx)
	{
		const int playerId = 9331;
		string accountUid = LF_Utils.GetAccountUID(playerId);
		ctx.False(accountUid.IsEmpty(), "the test player id has an account uid");
		if (accountUid.IsEmpty())
			return;

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = LF_PlayerAccount.Create(accountUid);
		accounts.AddToCache(account);
		account.AddCharacter(LF_PlayerCharacter.Create(PREFAB, "Pam", "First", 30), true);
		LF_PlayerCharacter second = LF_PlayerCharacter.Create(PREFAB, "Pat", "Second", 30);
		account.AddCharacter(second, true);
		ctx.EqualStr(second.GetRecordKey(), LF_Utils.GetPlayerUID(playerId), "slot 2 plays");

		LF_PoliceApplications.SetSavePath("$profile:LifeFramework/test_cpa_police_applications.txt");
		LF_WhitelistManager.SetSavePath("$profile:LifeFramework/test_cpa_police_whitelist.txt");
		LF_WhitelistManager.Reset();
		FileIO.DeleteFile("$profile:LifeFramework/test_cpa_police_applications.txt");

		string applicant = LF_PoliceApplications.ApplicantUid(playerId);
		ctx.EqualStr(accountUid, applicant, "the application is filed under the account uid");
		ctx.EqualStr("", LF_PoliceApplications.Submit(applicant, "Pat Second", "To help", "Evenings", false), "the application is filed");
		ctx.True(LF_PoliceApplications.Approve(applicant, true), "an admin approves it");
		ctx.True(LF_AccountStateBits.IsOnRoster(LF_AccountStateBits.FromAccount(account)), "slot 2 is on the roster after approval");

		LF_PoliceApplications.SetSavePath("");
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
		FileIO.DeleteFile("$profile:LifeFramework/test_cpa_police_applications.txt");
		FileIO.DeleteFile("$profile:LifeFramework/test_cpa_police_whitelist.txt");
		accounts.RemoveFromCache(accountUid);
	}
}
