// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_CharacterSwitchRules,LF_Test_CharacterIsolation` (one batch with the pack test): RED-OK, with the hook skipping the switch the second character read the first's records: `- money: the second character starts with its own empty bank (expected 0, got 750)`, `- licences: the second character holds none (expected 0, got 1)`, `- record: the second character is not wanted (expected 0, got 2)`; green without it.
//! Two characters of one account share nothing a character owns (#331 made real): after a switch the
//! player's record key (LF_Utils.GetPlayerUID) is the new character's, so the new character starts
//! with its own empty bank, no licences, no carried inventory, a clean record and no ID card, while
//! the first character keeps its money, licences, pockets and warrant for when the player comes back.
//! The switch is the same SetActiveCharacter the clerk's pick runs (LF_CharacterCreationManager).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CharacterIsolation : LF_Test
{
	protected static const ResourceName PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	//! A player id with no connected player: on the -noBackend test server its account uid is the
	//! name-derived identity, and GetPlayerUID resolves it through the account cache.
	protected static const int PLAYER_ID = 9342;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "character/isolation";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string accountUid = LF_Utils.GetAccountUID(PLAYER_ID);
		ctx.False(accountUid.IsEmpty(), "the test player id has an account uid");
		if (accountUid.IsEmpty())
			return;

		LF_CharacterConfig.SetMaxPerAccountOverride(3);
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = LF_PlayerAccount.Create(accountUid);
		accounts.AddToCache(account);
		LF_PlayerCharacter first = LF_PlayerCharacter.Create(PREFAB, "Carl", "Copper", 34);
		account.AddCharacter(first, true);
		LF_PlayerCharacter second = LF_PlayerCharacter.Create(PREFAB, "Bea", "Bandit", 29);
		account.AddCharacter(second, false);
		account.SetActiveCharacter(first);

		string firstKey = LF_Utils.GetPlayerUID(PLAYER_ID);
		ctx.EqualStr(first.GetRecordKey(), firstKey, "the first character plays");

		// The first character's life: money, a licence, a warrant and what it carries.
		LF_BankAccount firstBank = LF_ATMManager.GetOrCreate(firstKey);
		ctx.NotNull(firstBank, "the first character's bank opens");
		if (firstBank)
			firstBank.Deposit(750, "test");

		array<LF_ELicenseType> licences = {};
		licences.Insert(LF_ELicenseType.FARMER_APPLE);
		LF_Progression.NoteLicenses(firstKey, licences);
		account.SetWantedLevel(2);

		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (body)
			ctx.True(LF_StorageSnapshot.CaptureCarried(first.GetId(), body), "the first character's pockets are captured");

		// The switch.
		if (!RedProof())
			account.SetActiveCharacter(second);

		string secondKey = LF_Utils.GetPlayerUID(PLAYER_ID);
		ctx.EqualStr(second.GetRecordKey(), secondKey, "after the switch the second character plays");
		ctx.False(secondKey == firstKey, "the two characters never share a record key");

		LF_BankAccount secondBank = LF_ATMManager.GetOrCreate(secondKey);
		ctx.NotNull(secondBank, "the second character's bank opens");
		if (secondBank)
			ctx.Equal(0, secondBank.GetBalance(), "money: the second character starts with its own empty bank");

		if (firstBank)
			ctx.Equal(750, firstBank.GetBalance(), "money: the first character keeps its balance");

		array<LF_ELicenseType> secondLicences = {};
		ctx.Equal(0, LF_Progression.GetLicenses(secondKey, secondLicences), "licences: the second character holds none");
		array<LF_ELicenseType> firstLicences = {};
		ctx.Equal(1, LF_Progression.GetLicenses(firstKey, firstLicences), "licences: the first character keeps its licence");

		ctx.Equal(0, account.GetWantedLevel(), "record: the second character is not wanted");
		ctx.False(LF_StorageSnapshot.Has(second.GetId(), LF_StorageSnapshot.SLOT_CARRIED), "inventory: the second character carries nothing of the first's");
		if (body)
			ctx.True(LF_StorageSnapshot.Has(first.GetId(), LF_StorageSnapshot.SLOT_CARRIED), "inventory: the first character's pockets wait for it");

		ctx.True(LF_PhotoIdCard.Current(second.GetId()) == null, "papers: the second character has no ID card of the first's");

		// Back to the first: everything is where it was left.
		account.SetActiveCharacter(first);
		ctx.EqualStr(firstKey, LF_Utils.GetPlayerUID(PLAYER_ID), "switching back plays the first character again");
		ctx.Equal(2, account.GetWantedLevel(), "record: the first character's warrant is still there");

		LF_TestCharacterFixture.Delete(body);
		LF_StorageSnapshot.Clear(first.GetId(), LF_StorageSnapshot.SLOT_CARRIED);
		LF_StorageSnapshot.Clear(second.GetId(), LF_StorageSnapshot.SLOT_CARRIED);
		LF_Progression.Forget(firstKey);
		LF_Progression.Forget(secondKey);
		LF_CharacterConfig.SetMaxPerAccountOverride(0);
		accounts.RemoveFromCache(accountUid);
	}
}
