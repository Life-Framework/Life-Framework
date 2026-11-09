// red-proof: e2e/pass1-write-save, Observed 2026-11-08 with `tools\cli test --no-build --tier all`: skipping
// manager.SaveGame() failed it with `[LFTEST] FAIL e2e/pass1-write-save`,
// `- SaveGame() accepts the manual save request`. Restored; green.
// e2e/pass2-verify-load (save version 5), observed 2026-11-07 with `tools\cli wt e2e restart-safety`:
// dropping order.Insert("jail") and writing version 4 left pass 1 green and failed
// pass 2 with `SUMMARY tier=persistence passed=7 failed=1 total=8`,
// `[LFTEST] FAIL e2e/pass2-verify-load`,
// `[LFTEST]   - e2e prisoner is still serving after the save load`. Restored; both
// passes green again.
// e2e/pass2-verify-load (save version 6, carried inventory), observed 2026-11-08 with
// `tools\cli wt e2e m1-04-carried-inventory`: with the LifeFramework.conf stack config still on
// `ParentHandling "Ignore always"` the engine saved the pocket MoneyStack as a root and
// self-spawned it on the ground, failing pass 2 with `[LFTEST] FAIL e2e/pass2-verify-load`,
// `- no 237 cash stack came back in the world on its own (no dupe) (expected 0, got 1)`.
// Stacks now save with their parent; both passes green.
// e2e/pass2-verify-load (save version 7, progression), observed 2026-11-08 with
// `tools\cli wt e2e m1-10-progression`: DeserializeStore stopping one payload early
// (`index >= version - 1`) left pass 1 green and failed pass 2 with
// `SUMMARY tier=persistence passed=10 failed=1 total=11`,
// `- e2e courier is still level 2 after the save load (expected 2, got 1)`,
// `- e2e senior officer stays senior after the save load (expected 1, got 0)`,
// `- e2e turned-in quest stays done after the save load`. Restored; both passes green.

// tier: PERSISTENCE
class LF_Test_E2E_SaveStateWrite : LF_Test
{
	protected static const string ACCOUNT_UID = "e2e-uid";
	protected static const string CARRIED_CHARACTER_ID = "e2e-carried-character";
	protected static const string COURIER_UID = "e2e-courier";
	protected static const string OFFICER_UID = "e2e-officer";
	protected static const string DUTY_UID = "e2e-duty-officer";
	protected static const string INTRO_KNOWER_UID = "e2e-intro-knower";
	protected static const string GARAGE_OWNER_UID = "e2e-garage-owner";
	protected static const string GARAGED_ID = "e2e-garaged-car";
	protected static const string STREET_ID = "e2e-street-car";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "e2e/pass1-write-save";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves for the save trigger");
		if (!gameMode)
			return;

		LF_PersistenceManagerComponent manager = LF_PersistenceManagerComponent.Cast(gameMode.FindComponent(LF_PersistenceManagerComponent));
		ctx.NotNull(manager, "the game-mode entity carries LF_PersistenceManagerComponent for the save trigger");
		if (!manager)
			return;

#ifdef LF_E2E_LOAD_BOOT
		// The reload boot must not re-seed: re-establishing the contract here would
		// mask a broken load by overwriting whatever the save restored.
		ctx.Pass("save state established by the save boot and loaded by this boot");
#else
		// The save boot seeds the contract directly instead of depending on the
		// narrative test: later all-tier tests (rob-reward, banking) reset the
		// account managers, and an async manual save can complete after those
		// resets. Seeding here plus the blocking shutdown save on graceful close
		// captures exactly the 300/1/CIVILIAN contract the loop must reach.
		// The deposit below is recorded by the money ledger (save version 14) and must survive the restart.
		LF_Ledger.EnsureSubscribed();
		LF_ATMManager atmManager = LF_ATMManager.GetInstance();
		LF_BankAccount bank = atmManager.CreateAccount(ACCOUNT_UID);
		ctx.NotNull(bank, "e2e bank account created for the save");
		if (bank)
		{
			ctx.True(atmManager.Deposit(ACCOUNT_UID, 300), "e2e bank balance 300 deposited for the save");
			ctx.Equal(300, atmManager.GetAccount(ACCOUNT_UID).GetBalance(), "e2e bank balance reads 300 before the save");
		}

		LF_PlayerAccountManager playerAccounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = playerAccounts.GetOrCreate(ACCOUNT_UID);
		ctx.NotNull(account, "e2e player account created for the save");
		if (account)
		{
			account.SetWantedLevel(1);
			account.SetFaction(LF_Faction.CIVILIAN);
			playerAccounts.SaveAndReleaseAccount(account);
		}

		// A serving sentence rides the "jail" payload (save version 5) across the restart.
		array<ref LF_JailRecord> sentences = LF_Jail.ExportAll();
		sentences.Insert(LF_JailRecord.Create(ACCOUNT_UID, 3600, true));
		LF_Jail.ApplyAll(sentences);
		ctx.True(LF_Jail.IsServing(ACCOUNT_UID), "e2e prisoner is serving before the save");

		// An incident rides the "incidents" payload (save version 13); the reload boot queries it.
		LF_IncidentLog.Append(LF_IncidentConfig.KIND_DEATH, System.GetUnixTime(), ACCOUNT_UID, "e2e-killer", Vector(4321, 0, 1234), 0, "e2e-rifle", true);

		// A carried inventory rides the "carried" payload (save version 6). The character stays
		// alive in the world through the shutdown save, so the reload boot also sees whether the
		// engine brings its items back on its own (a dupe next to the restored snapshot).
		IEntity carrier = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (carrier)
			ctx.True(LF_StorageSnapshot.CaptureCarried(CARRIED_CHARACTER_ID, carrier), "e2e carried inventory captured before the save");

		// Progression rides the "progression" payload (save version 7): a level 2 courier, a
		// senior officer, and a turned-in quest.
		// SeedCourier leaves the courier signed up for parcels: the "jobsignups" payload (save version 11).
		LF_TestProgressionKit.SeedCourier(ctx, COURIER_UID);
		LF_TestProgressionKit.SeedSeniorOfficer(ctx, OFFICER_UID);
		LF_Progression.NoteTurnedIn(COURIER_UID, LF_TestProgressionKit.QUEST_ID);
		// An introduction rides the "introductions" payload (save version 8): Bea still knows Ada.
		LF_Introductions.Introduce(INTRO_KNOWER_UID, ACCOUNT_UID);
		ctx.True(LF_Introductions.Knows(INTRO_KNOWER_UID, ACCOUNT_UID), "e2e introduction known before the save");
		// The hideout stash rides the same payload under hideout:<id>. Its three items stay in the
		// placed crate through the shutdown save; the reload boot counts them there.
		IEntity stash = LF_TestHideoutStashKit.FindStash();
		ctx.NotNull(stash, "e2e hideout stash is placed");
		if (stash)
			LF_TestHideoutStashKit.SeedThree(ctx, stash);
		// Garage it to keep it (vehicles payload, save version 4): one car garaged, one left on the street.
		LF_VehicleOwnership.Register(GARAGED_ID, GARAGE_OWNER_UID, GARAGED_ID, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", "EE GA 1601", "#EL-Vehicle_S105", true);
		LF_VehicleOwnership.Register(STREET_ID, GARAGE_OWNER_UID, STREET_ID, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", "EE ST 1602", "#EL-Vehicle_S105", true);
		ctx.True(LF_VehicleOwnership.Store(GARAGED_ID, GARAGE_OWNER_UID), "e2e car garaged before the save");

		// Both duty inventories ride the carried payload (police_duty, civilian worn) and the
		// magazine rounds the itemstate payload (save version 9): clock in, pick up 2 apples,
		// leave 9 rounds, clock out. A wanted report with its identified flag rides "wanted" (10).
		LF_PlayerAccount dutyAccount = LF_TestPoliceKit.Account(DUTY_UID, true);
		IEntity dutyBody = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (dutyBody)
		{
			ctx.EqualStr("", LF_DutyLocker.ClockIn(dutyAccount, dutyBody), "e2e officer clocks in before the save");
			ctx.Equal(2, LF_InventoryUtils.AddAmount(dutyBody, LF_TestCarriedInventoryKit.APPLE_PREFAB, 2), "e2e officer picks up 2 apples on shift");
			BaseMagazineComponent magazine = LF_TestPoliceKit.FindMagazine(dutyBody);
			ctx.NotNull(magazine, "e2e police kit carries a magazine");
			if (magazine)
				magazine.SetAmmoCount(9);
			ctx.EqualStr("", LF_DutyLocker.ClockOut(dutyAccount, dutyBody), "e2e officer clocks out before the save");
		}

		LF_WantedReports.Add(ACCOUNT_UID, LF_WantedReports.OFFENCE_ROBBERY, "#AR-Jacket_M88", "", "Fuel station", true);

		// The async manual save is the belt; the blocking shutdown save on game
		// close (OnGameEnd) is the suspenders the reload boot loads.
		manager.SaveGame();
		ctx.True(manager.IsSaveInProgress(), "SaveGame() accepts the manual save request");
#endif
	}
}

//------------------------------------------------------------------------------------------------
// tier: PERSISTENCE
class LF_Test_E2E_SaveStateVerify : LF_Test
{
	protected static const string ACCOUNT_UID = "e2e-uid";
	protected static const string CARRIED_CHARACTER_ID = "e2e-carried-character";
	protected static const string COURIER_UID = "e2e-courier";
	protected static const string OFFICER_UID = "e2e-officer";
	protected static const string DUTY_UID = "e2e-duty-officer";
	protected static const string INTRO_KNOWER_UID = "e2e-intro-knower";
	protected static const string GARAGE_OWNER_UID = "e2e-garage-owner";
	protected static const string GARAGED_ID = "e2e-garaged-car";
	protected static const string STREET_ID = "e2e-street-car";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "e2e/pass2-verify-load";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
#ifdef LF_E2E_LOAD_BOOT
		// The reload boot passes -scrDefine LF_E2E_LOAD_BOOT (and boots with
		// -loadSessionSave). In the save boot or a normal all-tier pass the live
		// state could satisfy these assertions with no save/load behind them, so
		// they must only run when the save was actually loaded.
		LF_BankAccount bank = LF_ATMManager.GetInstance().GetAccount(ACCOUNT_UID);
		ctx.NotNull(bank, "bank account e2e-uid present after the save load");
		if (bank)
			ctx.Equal(300, bank.GetBalance(), "bank balance 300 after the save load");

		array<ref LF_LedgerEntry> ledgerLines = {};
		LF_Ledger.Last(ACCOUNT_UID, LF_Ledger.ACCOUNT_BANK, 20, ledgerLines);
		ctx.True(ledgerLines.Count() >= 1, "the money ledger kept the e2e deposit across the restart");
		if (ledgerLines.Count() >= 1)
		{
			ctx.Equal(300, ledgerLines.Get(0).m_iDelta, "the ledger line is the 300 deposit");
			ctx.EqualStr("bank-deposit", ledgerLines.Get(0).m_sReason, "the ledger line keeps its reason");
		}

		if (bank)
			ctx.True(LF_Ledger.VerifyBank(ACCOUNT_UID, bank.GetBalance()), "the ledger rebuilds the stored bank balance after the restart");

		LF_PlayerAccount cached = LF_PlayerAccountManager.GetInstance().GetFromCache(ACCOUNT_UID);
		ctx.NotNull(cached, "player account e2e-uid present after the save load");
		if (cached)
		{
			ctx.Equal(1, cached.GetWantedLevel(), "wanted level 1 after the save load");
			ctx.Equal(LF_Faction.CIVILIAN, cached.GetFaction(), "account faction CIVILIAN after the save load");
		}

		ctx.True(LF_Jail.IsServing(ACCOUNT_UID), "e2e prisoner is still serving after the save load");
		ctx.EqualFloat(3600, LF_Jail.GetRemaining(ACCOUNT_UID), 0.01, "e2e sentence resumes with the same remaining time (an offline prisoner serves no time)");

		array<ref LF_IncidentRecord> incidents = LF_IncidentLog.Query(Vector(4321, 0, 1234), 5, 0, -1, LF_IncidentConfig.KIND_DEATH);
		ctx.True(incidents.Count() >= 1, "e2e incident from boot 1 is queryable by position after the save load");

		LF_StorageSnapshotRecord carried = LF_StorageSnapshot.Get(CARRIED_CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(carried, "e2e carried snapshot present after the save load");
		ctx.Equal(0, LF_TestLooseStackCount.Count(LF_TestCarriedInventoryKit.MONEY_PREFAB, 237), "no 237 cash stack came back in the world on its own (no dupe)");
		if (carried)
		{
			IEntity restored = LF_TestCharacterFixture.Spawn(ctx);
			if (restored)
			{
				LF_StorageSnapshot.Apply(restored, carried);
				ctx.Equal(237, LF_MoneyUtils.GetCash(restored), "e2e 237 cash carried across the restart");
				ctx.Equal(3, LF_InventoryUtils.GetAmount(restored, LF_TestCarriedInventoryKit.APPLE_PREFAB), "e2e 3 apples carried across the restart");
				ctx.Equal(1, LF_TestCarriedInventoryKit.CountInside(restored, LF_TestCarriedInventoryKit.BACKPACK_PREFAB, LF_TestCarriedInventoryKit.WATER_PREFAB), "e2e water bottle still inside the backpack after the restart");
				ctx.Equal(0, LF_StorageSnapshot.Apply(restored, carried), "e2e second apply spawns nothing");
				ctx.Equal(237, LF_MoneyUtils.GetCash(restored), "e2e cash unchanged by the second apply");
				LF_TestCharacterFixture.Delete(restored);
			}
		}

		ctx.Equal(2, LF_CivilianJob.GetLevel(COURIER_UID), "e2e courier is still level 2 after the save load");
		ctx.True(LF_CivilianJob.CanSignUp(COURIER_UID, LF_EJobType.FUEL_DELIVERY), "e2e courier level 2 still opens fuel delivery");
		ctx.Equal(LF_EJobType.PARCEL, LF_CivilianJob.GetSignedUpJob(COURIER_UID), "e2e courier is still signed up for parcels after the save load");
		LF_PlayerAccount officer = LF_PlayerAccountManager.GetInstance().GetFromCache(OFFICER_UID);
		ctx.NotNull(officer, "e2e officer account present after the save load");
		if (officer)
		{
			ctx.Equal(LF_PoliceRanks.SENIOR, officer.GetPoliceRank(), "e2e senior officer stays senior after the save load");
			ctx.Equal(LF_PoliceRanks.DUTY_SECONDS, officer.GetOnDutySeconds(), "e2e on-duty seconds survive the save load");
		}

		ctx.True(LF_Progression.HasTurnedIn(COURIER_UID, LF_TestProgressionKit.QUEST_ID), "e2e turned-in quest stays done after the save load");

		VerifyDutyInventories(ctx);
		ctx.Equal(1, LF_WantedReports.CountFor(ACCOUNT_UID), "e2e wanted report present after the save load");
		ctx.True(LF_WantedReports.IsIdentified(ACCOUNT_UID), "e2e wanted report stays identified after the save load");
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (body)
		{
			LF_Progression.ApplyToCharacter(COURIER_UID, body);
			LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(body);
			ctx.True(tracker && tracker.HasTurnedIn(LF_TestProgressionKit.QUEST_ID), "e2e new body has the quest turned in");
			LF_TestCharacterFixture.Delete(body);
		}
		ctx.True(LF_Introductions.Knows(INTRO_KNOWER_UID, ACCOUNT_UID), "e2e introduction still known after the save load");
		ctx.False(LF_Introductions.Knows(ACCOUNT_UID, INTRO_KNOWER_UID), "e2e introduction stays one way after the save load");

		IEntity stash = LF_TestHideoutStashKit.FindStash();
		ctx.NotNull(stash, "e2e hideout stash is placed in the reload boot");
		ctx.NotNull(LF_StorageSnapshot.Get(LF_TestHideoutStashKit.STASH_KEY, LF_PersistentStorageComponent.SLOT_PLACED), "e2e stash snapshot present after the save load");
		if (stash)
		{
			ctx.Equal(3, LF_TestHideoutStashKit.ItemCount(stash), "e2e the stash holds its 3 items after the restart");
			ctx.Equal(LF_TestHideoutStashKit.SEEDED_CASH, LF_TestHideoutStashKit.Count(stash, LF_TestHideoutStashKit.MONEY_PREFAB), "e2e 151 cash in the stash after the restart");
			ctx.Equal(1, LF_TestHideoutStashKit.Count(stash, LF_TestHideoutStashKit.APPLE_PREFAB), "e2e the apple is in the stash after the restart");
			ctx.Equal(1, LF_TestHideoutStashKit.Count(stash, LF_TestHideoutStashKit.WATER_PREFAB), "e2e the water bottle is in the stash after the restart");
			ctx.Equal(0, LF_TestLooseStackCount.Count(LF_TestHideoutStashKit.MONEY_PREFAB, LF_TestHideoutStashKit.SEEDED_CASH), "no 151 cash stack came back loose in the world (no dupe)");
			ctx.Equal(0, LF_PersistentStorageComponent.RestoreAllPlaced(), "e2e restoring the stash again spawns nothing");
		}

		ctx.True(LF_VehicleOwnership.IsStored(GARAGED_ID), "e2e garaged car is still in the garage after the restart");
		ctx.False(LF_VehicleOwnership.IsImpounded(GARAGED_ID), "e2e garaged car is not impounded");
		ctx.True(LF_VehicleOwnership.IsImpounded(STREET_ID), "e2e street car is at the impound lot after the restart");
		ctx.True(LF_VehicleOwnership.GetVehicle(STREET_ID) == null, "e2e street car was not spawned on the street");
		IEntity payer = LF_TestCharacterFixture.Spawn(ctx);
		if (payer)
		{
			int fee = LF_GarageConfig.ImpoundFee();
			LF_MoneyUtils.AddCash(payer, fee);
			string reasonKey;
			ctx.True(LF_VehicleOwnership.CollectFromImpound(STREET_ID, GARAGE_OWNER_UID, payer, fee, "roadmap-impound", reasonKey), "e2e street car collected from the impound for the fee");
			ctx.Equal(0, LF_MoneyUtils.GetCash(payer), "e2e impound fee taken from carried cash");
			ctx.False(LF_VehicleOwnership.CollectFromImpound(STREET_ID, GARAGE_OWNER_UID, payer, fee, "roadmap-impound", reasonKey), "e2e street car is collected only once");
			ctx.True(LF_VehicleOwnership.GetVehicle(STREET_ID) != null, "e2e collected car is in the impound yard");
			LF_VehicleOwnership.Store(STREET_ID, GARAGE_OWNER_UID);
			LF_TestCharacterFixture.Delete(payer);
		}

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves in the reload boot");
		if (gameMode)
		{
			LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
			ctx.NotNull(persistence, "the game-mode entity carries LF_PersistenceComponent for survival checks");
		}
#else
		ctx.Pass("runs in the reload boot");
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! After the restart: the civilian inventory is what the officer carries, the police one waits
	//! at the desk, and clocking in brings it back with the rounds left in the magazine.
	protected void VerifyDutyInventories(LF_TestContext ctx)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetInstance().GetFromCache(DUTY_UID);
		ctx.NotNull(account, "e2e duty officer account present after the save load");
		if (!account || !account.GetActiveCharacter())
			return;

		ctx.False(account.IsOnDuty(), "e2e officer is off duty after the save load");
		string characterId = account.GetActiveCharacter().GetId();
		LF_StorageSnapshotRecord carried = LF_StorageSnapshot.Get(characterId, LF_StorageSnapshot.SLOT_CARRIED);
		LF_StorageSnapshotRecord police = LF_StorageSnapshot.Get(characterId, LF_DutyLocker.SLOT_POLICE);
		ctx.NotNull(carried, "e2e civilian inventory carried after the save load");
		ctx.NotNull(police, "e2e police inventory stored at the desk after the save load");
		if (!carried || !police)
			return;

		ctx.Equal(2, police.CountPrefab(LF_TestCarriedInventoryKit.APPLE_PREFAB), "e2e stored police inventory keeps the 2 apples");
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		LF_StorageSnapshot.Apply(body, carried);
		ctx.Equal(3, LF_InventoryUtils.GetAmount(body, LF_TestCarriedInventoryKit.APPLE_PREFAB), "e2e officer carries the civilian 3 apples after the restart");
		LF_WhitelistManager.AddFactionWhitelist(DUTY_UID, LF_EFactionType.POLICE);
		ctx.EqualStr("", LF_DutyLocker.ClockIn(account, body), "e2e officer clocks in after the restart");
		ctx.True(LF_TestCharacterFixture.HasItem(body, LF_TestPoliceKit.M9_PREFAB), "e2e the M9 comes back after the restart");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(body, LF_TestCarriedInventoryKit.APPLE_PREFAB), "e2e the police 2 apples come back after the restart");
		BaseMagazineComponent magazine = LF_TestPoliceKit.FindMagazine(body);
		ctx.NotNull(magazine, "e2e the magazine comes back after the restart");
		if (magazine)
			ctx.Equal(9, magazine.GetAmmoCount(), "e2e the magazine keeps 9 rounds across the restart");

		LF_StorageSnapshotRecord civilian = LF_StorageSnapshot.Get(characterId, LF_DutyLocker.SLOT_CIVILIAN);
		ctx.NotNull(civilian, "e2e the civilian inventory is stored at the desk again");
		if (civilian)
			ctx.Equal(3, civilian.CountPrefab(LF_TestCarriedInventoryKit.APPLE_PREFAB), "e2e the stored civilian inventory holds the 3 apples");

		LF_DutyLocker.ClockOut(account, body);
		LF_TestPoliceKit.Release(DUTY_UID);
		LF_TestCharacterFixture.Delete(body);
	}
}

//------------------------------------------------------------------------------------------------
//! Counts stacks of a prefab with an exact quantity anywhere in the world.
class LF_TestLooseStackCount
{
	protected static ResourceName s_Prefab;
	protected static int s_iQuantity;
	protected static int s_iFound;

	//------------------------------------------------------------------------------------------------
	static int Count(ResourceName prefab, int quantity)
	{
		s_Prefab = prefab;
		s_iQuantity = quantity;
		s_iFound = 0;
		vector mins, maxs;
		GetGame().GetWorld().GetBoundBox(mins, maxs);
		GetGame().GetWorld().QueryEntitiesByAABB(mins, maxs, Collect, null, EQueryEntitiesFlags.ALL);
		return s_iFound;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Collect(IEntity entity)
	{
		if (!entity || LF_Utils.GetPrefabName(entity) != s_Prefab)
			return true;

		LF_QuantityComponent quantity = LF_QuantityComponent.Cast(entity.FindComponent(LF_QuantityComponent));
		if (!quantity || quantity.GetQuantity() != s_iQuantity)
			return true;

		s_iFound++;
		IEntity root = entity.GetRootParent();
		LF_Debug.Info("CarriedInventory", string.Format("e2e found a %1 stack of %2 at %3, root %4", s_iQuantity, s_Prefab, entity.GetOrigin(), LF_Utils.GetPrefabName(root)));

		return true;
	}
}
