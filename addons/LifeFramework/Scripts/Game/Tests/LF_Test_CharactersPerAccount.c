// red-proof: observed 2026-11-11 with `tools\cli test --no-build --only LF_Test_CharactersPerAccount,LF_Test_CharacterSlotsPersist`:
// LF_PlayerAccount.AddCharacter giving every character the account uid (`if (first || true)`) failed
// `- slot 2 is keyed by its own character id (expected '01a1c84b-...', got 'cpa-account-uid')` and
// `- slot 2's wanted level is kept while slot 1 plays (expected 4, got 2)`. Restored; green.

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
		ctx.EqualStr("", LF_CharacterCreationRules.RefusePick(account, 1, false), "slot 2 can be picked");
		ctx.EqualStr("#LF-CharSel_Refused", LF_CharacterCreationRules.RefusePick(account, 2, false), "a slot past the roster is refused");
		ctx.EqualStr("#LF-CharSel_Refused", LF_CharacterCreationRules.RefusePick(account, 0, true), "a pick from inside a living body is refused");
		ctx.EqualStr("#LF-Char_Error_Full", LF_CharacterCreationRules.RefusePick(account, LF_CharacterSelectSheet.SLOT_NEW, false), "registering a third from the pick is refused");

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
}
