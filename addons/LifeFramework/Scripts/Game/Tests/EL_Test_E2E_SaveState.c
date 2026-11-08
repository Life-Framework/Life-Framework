// red-proof: e2e/pass1-write-save, Observed 2026-11-08 with `tools\cli test --no-build --tier all`: skipping
// manager.SaveGame() failed it with `[ELTEST] FAIL e2e/pass1-write-save`,
// `- SaveGame() accepts the manual save request`. Restored; green.
// e2e/pass2-verify-load (save version 5), observed 2026-11-07 with `tools\cli wt e2e restart-safety`:
// dropping order.Insert("jail") and writing version 4 left pass 1 green and failed
// pass 2 with `SUMMARY tier=persistence passed=7 failed=1 total=8`,
// `[ELTEST] FAIL e2e/pass2-verify-load`,
// `[ELTEST]   - e2e prisoner is still serving after the save load`. Restored; both
// passes green again.
// e2e/pass2-verify-load (save version 6, carried inventory), observed 2026-11-08 with
// `tools\cli wt e2e m1-04-carried-inventory`: with the LifeFramework.conf stack config still on
// `ParentHandling "Ignore always"` the engine saved the pocket MoneyStack as a root and
// self-spawned it on the ground, failing pass 2 with `[ELTEST] FAIL e2e/pass2-verify-load`,
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
class EL_Test_E2E_SaveStateWrite : EL_Test
{
	protected static const string ACCOUNT_UID = "e2e-uid";
	protected static const string CARRIED_CHARACTER_ID = "e2e-carried-character";
	protected static const string COURIER_UID = "e2e-courier";
	protected static const string OFFICER_UID = "e2e-officer";
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
	override void Run(EL_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves for the save trigger");
		if (!gameMode)
			return;

		EL_PersistenceManagerComponent manager = EL_PersistenceManagerComponent.Cast(gameMode.FindComponent(EL_PersistenceManagerComponent));
		ctx.NotNull(manager, "the game-mode entity carries EL_PersistenceManagerComponent for the save trigger");
		if (!manager)
			return;

#ifdef EL_E2E_LOAD_BOOT
		// The reload boot must not re-seed: re-establishing the contract here would
		// mask a broken load by overwriting whatever the save restored.
		ctx.Pass("save state established by the save boot and loaded by this boot");
#else
		// The save boot seeds the contract directly instead of depending on the
		// narrative test: later all-tier tests (rob-reward, banking) reset the
		// account managers, and an async manual save can complete after those
		// resets. Seeding here plus the blocking shutdown save on graceful close
		// captures exactly the 300/1/CIVILIAN contract the loop must reach.
		EL_ATMManager atmManager = EL_ATMManager.GetInstance();
		EL_BankAccount bank = atmManager.CreateAccount(ACCOUNT_UID);
		ctx.NotNull(bank, "e2e bank account created for the save");
		if (bank)
		{
			ctx.True(atmManager.Deposit(ACCOUNT_UID, 300), "e2e bank balance 300 deposited for the save");
			ctx.Equal(300, atmManager.GetAccount(ACCOUNT_UID).GetBalance(), "e2e bank balance reads 300 before the save");
		}

		EL_PlayerAccountManager playerAccounts = EL_PlayerAccountManager.GetInstance();
		EL_PlayerAccount account = playerAccounts.GetOrCreate(ACCOUNT_UID);
		ctx.NotNull(account, "e2e player account created for the save");
		if (account)
		{
			account.SetWantedLevel(1);
			account.SetFaction(EL_Faction.CIVILIAN);
			playerAccounts.SaveAndReleaseAccount(account);
		}

		// A serving sentence rides the "jail" payload (save version 5) across the restart.
		array<ref LF_JailRecord> sentences = LF_Jail.ExportAll();
		sentences.Insert(LF_JailRecord.Create(ACCOUNT_UID, 3600, true));
		LF_Jail.ApplyAll(sentences);
		ctx.True(LF_Jail.IsServing(ACCOUNT_UID), "e2e prisoner is serving before the save");

		// A carried inventory rides the "carried" payload (save version 6). The character stays
		// alive in the world through the shutdown save, so the reload boot also sees whether the
		// engine brings its items back on its own (a dupe next to the restored snapshot).
		IEntity carrier = EL_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (carrier)
			ctx.True(LF_StorageSnapshot.CaptureCarried(CARRIED_CHARACTER_ID, carrier), "e2e carried inventory captured before the save");

		// Progression rides the "progression" payload (save version 7): a level 2 courier, a
		// senior officer, and a turned-in quest.
		EL_TestProgressionKit.SeedCourier(ctx, COURIER_UID);
		EL_TestProgressionKit.SeedSeniorOfficer(ctx, OFFICER_UID);
		LF_Progression.NoteTurnedIn(COURIER_UID, EL_TestProgressionKit.QUEST_ID);
		// An introduction rides the "introductions" payload (save version 8): Bea still knows Ada.
		LF_Introductions.Introduce(INTRO_KNOWER_UID, ACCOUNT_UID);
		ctx.True(LF_Introductions.Knows(INTRO_KNOWER_UID, ACCOUNT_UID), "e2e introduction known before the save");
		// Garage it to keep it (vehicles payload, save version 4): one car garaged, one left on the street.
		EL_VehicleOwnership.Register(GARAGED_ID, GARAGE_OWNER_UID, GARAGED_ID, EL_VehicleCatalog.S105, EL_VehicleCatalog.S105_LOCKED, "roadmap-lot", "EE GA 1601", "#EL-Vehicle_S105", true);
		EL_VehicleOwnership.Register(STREET_ID, GARAGE_OWNER_UID, STREET_ID, EL_VehicleCatalog.S105, EL_VehicleCatalog.S105_LOCKED, "roadmap-lot", "EE ST 1602", "#EL-Vehicle_S105", true);
		ctx.True(EL_VehicleOwnership.Store(GARAGED_ID, GARAGE_OWNER_UID), "e2e car garaged before the save");

		// The async manual save is the belt; the blocking shutdown save on game
		// close (OnGameEnd) is the suspenders the reload boot loads.
		manager.SaveGame();
		ctx.True(manager.IsSaveInProgress(), "SaveGame() accepts the manual save request");
#endif
	}
}

//------------------------------------------------------------------------------------------------
// tier: PERSISTENCE
class EL_Test_E2E_SaveStateVerify : EL_Test
{
	protected static const string ACCOUNT_UID = "e2e-uid";
	protected static const string CARRIED_CHARACTER_ID = "e2e-carried-character";
	protected static const string COURIER_UID = "e2e-courier";
	protected static const string OFFICER_UID = "e2e-officer";
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
	override void Run(EL_TestContext ctx)
	{
#ifdef EL_E2E_LOAD_BOOT
		// The reload boot passes -scrDefine EL_E2E_LOAD_BOOT (and boots with
		// -loadSessionSave). In the save boot or a normal all-tier pass the live
		// state could satisfy these assertions with no save/load behind them, so
		// they must only run when the save was actually loaded.
		EL_BankAccount bank = EL_ATMManager.GetInstance().GetAccount(ACCOUNT_UID);
		ctx.NotNull(bank, "bank account e2e-uid present after the save load");
		if (bank)
			ctx.Equal(300, bank.GetBalance(), "bank balance 300 after the save load");

		EL_PlayerAccount cached = EL_PlayerAccountManager.GetInstance().GetFromCache(ACCOUNT_UID);
		ctx.NotNull(cached, "player account e2e-uid present after the save load");
		if (cached)
		{
			ctx.Equal(1, cached.GetWantedLevel(), "wanted level 1 after the save load");
			ctx.Equal(EL_Faction.CIVILIAN, cached.GetFaction(), "account faction CIVILIAN after the save load");
		}

		ctx.True(LF_Jail.IsServing(ACCOUNT_UID), "e2e prisoner is still serving after the save load");
		ctx.EqualFloat(3600, LF_Jail.GetRemaining(ACCOUNT_UID), 0.01, "e2e sentence resumes with the same remaining time (an offline prisoner serves no time)");

		LF_StorageSnapshotRecord carried = LF_StorageSnapshot.Get(CARRIED_CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(carried, "e2e carried snapshot present after the save load");
		ctx.Equal(0, EL_TestLooseStackCount.Count(EL_TestCarriedInventoryKit.MONEY_PREFAB, 237), "no 237 cash stack came back in the world on its own (no dupe)");
		if (carried)
		{
			IEntity restored = EL_TestCharacterFixture.Spawn(ctx);
			if (restored)
			{
				LF_StorageSnapshot.Apply(restored, carried);
				ctx.Equal(237, EL_MoneyUtils.GetCash(restored), "e2e 237 cash carried across the restart");
				ctx.Equal(3, EL_InventoryUtils.GetAmount(restored, EL_TestCarriedInventoryKit.APPLE_PREFAB), "e2e 3 apples carried across the restart");
				ctx.Equal(1, EL_TestCarriedInventoryKit.CountInside(restored, EL_TestCarriedInventoryKit.BACKPACK_PREFAB, EL_TestCarriedInventoryKit.WATER_PREFAB), "e2e water bottle still inside the backpack after the restart");
				ctx.Equal(0, LF_StorageSnapshot.Apply(restored, carried), "e2e second apply spawns nothing");
				ctx.Equal(237, EL_MoneyUtils.GetCash(restored), "e2e cash unchanged by the second apply");
				EL_TestCharacterFixture.Delete(restored);
			}
		}

		ctx.Equal(2, LF_CivilianJob.GetLevel(COURIER_UID), "e2e courier is still level 2 after the save load");
		ctx.True(LF_CivilianJob.CanSignUp(COURIER_UID, EL_EJobType.FUEL_DELIVERY), "e2e courier level 2 still opens fuel delivery");
		EL_PlayerAccount officer = EL_PlayerAccountManager.GetInstance().GetFromCache(OFFICER_UID);
		ctx.NotNull(officer, "e2e officer account present after the save load");
		if (officer)
		{
			ctx.Equal(LF_PoliceRanks.SENIOR, officer.GetPoliceRank(), "e2e senior officer stays senior after the save load");
			ctx.Equal(LF_PoliceRanks.DUTY_SECONDS, officer.GetOnDutySeconds(), "e2e on-duty seconds survive the save load");
		}

		ctx.True(LF_Progression.HasTurnedIn(COURIER_UID, EL_TestProgressionKit.QUEST_ID), "e2e turned-in quest stays done after the save load");
		IEntity body = EL_TestCharacterFixture.Spawn(ctx);
		if (body)
		{
			LF_Progression.ApplyToCharacter(COURIER_UID, body);
			LF_QuestTrackerComponent tracker = EL_Component<LF_QuestTrackerComponent>.Find(body);
			ctx.True(tracker && tracker.HasTurnedIn(EL_TestProgressionKit.QUEST_ID), "e2e new body has the quest turned in");
			EL_TestCharacterFixture.Delete(body);
		}
		ctx.True(LF_Introductions.Knows(INTRO_KNOWER_UID, ACCOUNT_UID), "e2e introduction still known after the save load");
		ctx.False(LF_Introductions.Knows(ACCOUNT_UID, INTRO_KNOWER_UID), "e2e introduction stays one way after the save load");

		ctx.True(EL_VehicleOwnership.IsStored(GARAGED_ID), "e2e garaged car is still in the garage after the restart");
		ctx.False(EL_VehicleOwnership.IsImpounded(GARAGED_ID), "e2e garaged car is not impounded");
		ctx.True(EL_VehicleOwnership.IsImpounded(STREET_ID), "e2e street car is at the impound lot after the restart");
		ctx.True(EL_VehicleOwnership.GetVehicle(STREET_ID) == null, "e2e street car was not spawned on the street");
		IEntity payer = EL_TestCharacterFixture.Spawn(ctx);
		if (payer)
		{
			int fee = LF_GarageConfig.ImpoundFee();
			EL_MoneyUtils.AddCash(payer, fee);
			string reasonKey;
			ctx.True(EL_VehicleOwnership.CollectFromImpound(STREET_ID, GARAGE_OWNER_UID, payer, fee, "roadmap-impound", reasonKey), "e2e street car collected from the impound for the fee");
			ctx.Equal(0, EL_MoneyUtils.GetCash(payer), "e2e impound fee taken from carried cash");
			ctx.False(EL_VehicleOwnership.CollectFromImpound(STREET_ID, GARAGE_OWNER_UID, payer, fee, "roadmap-impound", reasonKey), "e2e street car is collected only once");
			ctx.True(EL_VehicleOwnership.GetVehicle(STREET_ID) != null, "e2e collected car is in the impound yard");
			EL_VehicleOwnership.Store(STREET_ID, GARAGE_OWNER_UID);
			EL_TestCharacterFixture.Delete(payer);
		}

		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves in the reload boot");
		if (gameMode)
		{
			EL_PersistenceComponent persistence = EL_PersistenceComponent.Cast(gameMode.FindComponent(EL_PersistenceComponent));
			ctx.NotNull(persistence, "the game-mode entity carries EL_PersistenceComponent for survival checks");
		}
#else
		ctx.Pass("runs in the reload boot");
#endif
	}
}

//------------------------------------------------------------------------------------------------
//! Counts stacks of a prefab with an exact quantity anywhere in the world.
class EL_TestLooseStackCount
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
		if (!entity || EL_Utils.GetPrefabName(entity) != s_Prefab)
			return true;

		EL_QuantityComponent quantity = EL_QuantityComponent.Cast(entity.FindComponent(EL_QuantityComponent));
		if (!quantity || quantity.GetQuantity() != s_iQuantity)
			return true;

		s_iFound++;
		IEntity root = entity.GetRootParent();
		EL_Debug.Info("CarriedInventory", string.Format("e2e found a %1 stack of %2 at %3, root %4", s_iQuantity, s_Prefab, entity.GetOrigin(), EL_Utils.GetPrefabName(root)));

		return true;
	}
}
