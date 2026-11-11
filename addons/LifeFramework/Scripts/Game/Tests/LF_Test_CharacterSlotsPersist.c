// red-proof: observed 2026-11-11 with `tools\cli test --no-build --only LF_Test_CharactersPerAccount,LF_Test_CharacterSlotsPersist`:
// LF_PlayerAccountManager.ApplyAll keying slot 1 by its own id instead of the account uid
// (`&& false`) failed `- slot 1 is keyed by the account uid (expected 'cps-legacy-uid', got
// '01a1c84b-...')`, `- the old police rank is slot 1's (expected 1, got 0)` and
// `- slot 1's bank holds 100 (load 1) (expected 100, got 350)`. Restored; green.

// tier: PERSISTENCE
//! #331 through the real save path ("characters" payload, save version 24). A two-character account
//! saves and loads with each character's key, wanted level, police rank and bank balance where they
//! were, twice without change. A version 23 save (the format before #331) loads its one character
//! into slot 1, keyed by the account uid, with its bank, rank and wanted level. A roster whose first
//! character is gone keeps its saved key, and its rank follows the key.
class LF_Test_CharacterSlotsPersist : LF_Test
{
	protected static const string UID = "cps-two-uid";
	protected static const string LEGACY_UID = "cps-legacy-uid";
	protected static const string MOVED_UID = "cps-moved-uid";
	protected static const ResourceName PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/character-slots";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		// A full load replaces the carried store; keep the live snapshots for the tests after this.
		array<ref LF_StorageSnapshotRecord> liveSnapshots = LF_StorageSnapshot.ExportAll();
		LF_CharacterConfig.SetMaxPerAccountOverride(2);

		TwoCharacters(ctx, persistence);
		LegacySave(ctx, persistence);
		LostFirstCharacter(ctx, persistence);

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		accounts.RemoveFromCache(UID);
		accounts.RemoveFromCache(LEGACY_UID);
		accounts.RemoveFromCache(MOVED_UID);
		LF_CharacterConfig.SetMaxPerAccountOverride(0);
		LF_StorageSnapshot.ApplyAll(liveSnapshots);
	}

	//------------------------------------------------------------------------------------------------
	//! The balance exactly (a bank account of an earlier boot can survive in the e2e save).
	protected static void SetBank(string key, int balance)
	{
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		LF_BankAccount bank = atm.GetAccount(key);
		if (!bank)
			bank = atm.CreateAccount(key);

		if (bank && bank.GetBalance() > 0)
			atm.Withdraw(key, bank.GetBalance());

		atm.Deposit(key, balance);
	}

	//------------------------------------------------------------------------------------------------
	protected void TwoCharacters(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = LF_PlayerAccount.Create(UID);
		accounts.AddToCache(account);
		LF_PlayerCharacter first = LF_PlayerCharacter.Create(PREFAB, "Ada", "Lovelace", 36);
		account.AddCharacter(first, true);
		account.SetWantedLevel(1);
		LF_PlayerCharacter second = LF_PlayerCharacter.Create(PREFAB, "Bob", "Builder", 40);
		account.AddCharacter(second, true);
		account.SetWantedLevel(3);
		account.SetPoliceRank(LF_PoliceRanks.SENIOR);
		string secondKey = second.GetRecordKey();
		string secondId = second.GetId();

		LF_ATMManager atm = LF_ATMManager.GetInstance();
		SetBank(UID, 100);
		SetBank(secondKey, 250);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(secondKey), "the save carries slot 2's key");

		// A restart: the account comes back empty and the balances are off.
		accounts.AddToCache(LF_PlayerAccount.Create(UID));
		atm.Deposit(UID, 1);
		atm.Deposit(secondKey, 1);

		for (int pass = 1; pass <= 2; pass++)
		{
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses");
			ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the version 24 save loads");

			LF_PlayerAccount loaded = accounts.GetFromCache(UID);
			ctx.NotNull(loaded, "the two-character account is back");
			if (!loaded)
				return;

			string tag = string.Format(" (load %1)", pass);
			ctx.Equal(2, loaded.GetCharacterCount(), "both characters are back" + tag);
			LF_PlayerCharacter slot1 = loaded.GetCharacterAt(0);
			LF_PlayerCharacter slot2 = loaded.GetCharacterAt(1);
			ctx.True(slot1 != null && slot2 != null, "both slots resolve" + tag);
			if (!slot1 || !slot2)
				return;

			ctx.EqualStr(UID, slot1.GetRecordKey(), "slot 1 keeps the account uid as its key" + tag);
			ctx.EqualStr(secondKey, slot2.GetRecordKey(), "slot 2 keeps its own key" + tag);
			ctx.EqualStr(secondId, slot2.GetId(), "slot 2 keeps its id" + tag);
			ctx.EqualStr(secondKey, loaded.GetActiveCharacterKey(), "slot 2 is still the one being played" + tag);
			ctx.Equal(1, slot1.GetStanding().m_iWantedLevel, "slot 1 is wanted 1" + tag);
			ctx.Equal(3, slot2.GetStanding().m_iWantedLevel, "slot 2 is wanted 3" + tag);
			ctx.Equal(LF_PoliceRanks.PATROL, slot1.GetStanding().m_iPoliceRank, "slot 1 stays a patrol officer" + tag);
			ctx.Equal(LF_PoliceRanks.SENIOR, slot2.GetStanding().m_iPoliceRank, "slot 2 stays senior" + tag);
			ctx.Equal(100, atm.GetAccount(UID).GetBalance(), "slot 1's bank holds 100" + tag);
			ctx.Equal(250, atm.GetAccount(secondKey).GetBalance(), "slot 2's bank holds 250" + tag);
		}

		LF_Progression.Forget(secondKey);
	}

	//------------------------------------------------------------------------------------------------
	//! A save in the format before #331: version 23, no character slots. The account's one
	//! character and everything keyed by the uid must come back as slot 1.
	protected void LegacySave(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = LF_PlayerAccount.Create(LEGACY_UID);
		accounts.AddToCache(account);
		account.AddCharacter(LF_PlayerCharacter.Create(PREFAB, "Old", "Timer", 60), true);
		account.SetWantedLevel(2);
		account.SetPoliceRank(LF_PoliceRanks.SENIOR);
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		SetBank(LEGACY_UID, 400);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, saveContext, 23) == ESerializeResult.OK, "the store serializes in the version 23 format");
		string saved = saveContext.SaveToString();

		accounts.AddToCache(LF_PlayerAccount.Create(LEGACY_UID));
		atm.Deposit(LEGACY_UID, 1);

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the version 23 save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(23, version, "the save is a version 23 save");

		JsonLoadContext fullLoad = new JsonLoadContext(false);
		fullLoad.LoadFromString(saved);
		ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, fullLoad), "the version 23 save loads");

		LF_PlayerAccount loaded = accounts.GetFromCache(LEGACY_UID);
		ctx.NotNull(loaded, "the old account is back");
		if (!loaded)
			return;

		ctx.Equal(1, loaded.GetCharacterCount(), "the old character is the only one");
		LF_PlayerCharacter slot1 = loaded.GetCharacterAt(0);
		ctx.True(slot1 != null, "the old character is in slot 1");
		if (slot1)
		{
			ctx.EqualStr("Old Timer", slot1.GetFullName(), "slot 1 is the old character");
			ctx.EqualStr(LEGACY_UID, slot1.GetRecordKey(), "slot 1 is keyed by the account uid");
		}

		ctx.EqualStr(LEGACY_UID, LF_PlayerAccountManager.ResolveCharacterKey(LEGACY_UID), "GetPlayerUID still answers the account uid");
		ctx.Equal(2, loaded.GetWantedLevel(), "the old wanted level is slot 1's");
		ctx.Equal(LF_PoliceRanks.SENIOR, loaded.GetPoliceRank(), "the old police rank is slot 1's");
		ctx.Equal(400, atm.GetAccount(LEGACY_UID).GetBalance(), "the old bank balance is slot 1's");
		LF_Progression.Forget(LEGACY_UID);
	}

	//------------------------------------------------------------------------------------------------
	//! A roster whose first character was removed: the remaining one keeps its own id as its key
	//! (the default would give it the uid), so its saved rank must follow the saved key.
	protected void LostFirstCharacter(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = LF_PlayerAccount.Create(MOVED_UID);
		accounts.AddToCache(account);
		LF_PlayerCharacter gone = LF_PlayerCharacter.Create(PREFAB, "Gone", "Before", 50);
		account.AddCharacter(gone, true);
		LF_PlayerCharacter kept = LF_PlayerCharacter.Create(PREFAB, "Kept", "After", 30);
		account.AddCharacter(kept, true);
		account.SetPoliceRank(LF_PoliceRanks.SENIOR);
		account.RemoveCharacter(gone);
		string keptKey = kept.GetRecordKey();
		ctx.EqualStr(kept.GetId(), keptKey, "the remaining character keeps its own key");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext);
		string saved = saveContext.SaveToString();
		accounts.AddToCache(LF_PlayerAccount.Create(MOVED_UID));

		JsonLoadContext loadContext = new JsonLoadContext(false);
		loadContext.LoadFromString(saved);
		ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext), "the save loads");
		LF_PlayerAccount loaded = accounts.GetFromCache(MOVED_UID);
		ctx.NotNull(loaded, "the account is back");
		if (!loaded)
			return;

		LF_PlayerCharacter slot1 = loaded.GetCharacterAt(0);
		ctx.True(slot1 != null, "the remaining character is in slot 1");
		if (!slot1)
			return;

		ctx.EqualStr(keptKey, slot1.GetRecordKey(), "it keeps its saved key, not the account uid");
		ctx.Equal(LF_PoliceRanks.SENIOR, slot1.GetStanding().m_iPoliceRank, "its rank follows the saved key");
		LF_Progression.Forget(keptKey);
	}
}
