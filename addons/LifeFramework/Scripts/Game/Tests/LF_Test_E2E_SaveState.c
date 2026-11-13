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

// e2e/pass2-verify-load (save version 27, organization ranks, #750a), observed 2026-11-12 with
// `tools\cli wt e2e m3-org-rights`: the rank masks did not survive (persist/character-slots, before
// this test, re-applied a version 23 save of the live store) and pass 2 failed with
// `- e2e officer keeps the territory right only orgranks holds`, `- e2e member stays without the stash right`.
// The legacy-loading tests now put the organizations back; both passes green, and green with --legacy-save.

// e2e/pass2-verify-load (save version 28, job XP, #749), observed 2026-11-12 with
// `tools\cli wt e2e m3-job-levels`: the shutdown save held 2 job xp rows instead of 4 (tests after
// this one reloaded "progression" or an older save, which seeds only CIVILIAN and DELIVERY) and
// pass 2 failed with `- e2e farmer xp survives the restart (expected 120, got 0)` and
// `- e2e miner xp survives the restart (expected 650, got 0)`. LF_TestLegacySnapshot and the
// progression tests now put the job XP back; both passes green, and green with --legacy-save.
// tier: PERSISTENCE
class LF_Test_E2E_SaveStateWrite : LF_Test
{
	protected static const string ACCOUNT_UID = "e2e-uid";
	protected static const string SLOTS_UID = "e2e-slots-uid";
	protected static const string CARRIED_CHARACTER_ID = "e2e-carried-character";
	protected static const string SERIAL_BUYER_UID = "e2e-serial-buyer";
	protected static const string COURIER_UID = "e2e-courier";
	protected static const string OFFICER_UID = "e2e-officer";
	protected static const string DUTY_UID = "e2e-duty-officer";
	protected static const string MEDIC_UID = "e2e-duty-medic";
	protected static const string INTRO_KNOWER_UID = "e2e-intro-knower";
	protected static const string GARAGE_OWNER_UID = "e2e-garage-owner";
	protected static const string GARAGED_ID = "e2e-garaged-car";
	protected static const string STREET_ID = "e2e-street-car";
	//! Playtime (#692, save version 26): 90 min on police duty in two stretches and 30 min civilian.
	static const string PLAYTIME_UID = "e2e-playtime-account";
	//! Job levels (#749, "joblevels" payload, save version 28): XP in three jobs, one of which
	//! (CIVILIAN) the progression record's pre-#749 single level also carries.
	static const string JOBS_UID = "e2e-job-levels";
	//! Organization ranks (#750a, "orgranks" payload, save version 27): a founder, an officer
	//! promoted face to face, and a member, with masks the four saved bools cannot hold.
	static const string ORG_NAME = "E2E Ladder";
	static const string ORG_FOUNDER = "e2e-org-founder";
	static const string ORG_OFFICER = "e2e-org-officer";
	static const string ORG_MEMBER = "e2e-org-member";
	static const ResourceName JUICE_PREFAB = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";

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
		// A stamped drill rides in the backpack (#470): its serial must survive the restart on the item.
		if (carrier)
		{
			ctx.Equal(1, LF_InventoryUtils.AddAmount(carrier, LF_Bank.DRILL_PREFAB, 1), "e2e carried drill added before the save");
			InventoryStorageManagerComponent carrierManager = LF_InventoryUtils.GetResponsibleStorageManager(carrier);
			array<IEntity> carriedDrills = LF_InventoryUtils.FindItemsByPrefab(carrierManager, LF_Bank.DRILL_PREFAB);
			LF_SerialComponent drillStamp;
			if (!carriedDrills.IsEmpty())
				drillStamp = LF_SerialComponent.Cast(carriedDrills[0].FindComponent(LF_SerialComponent));

			ctx.NotNull(drillStamp, "e2e carried drill has a serial component");
			if (drillStamp)
			{
				string drillSerial = LF_FenceSerials.Issue(SERIAL_BUYER_UID, LF_Bank.DRILL_PREFAB);
				ctx.True(drillStamp.SetSerial(drillSerial), "e2e carried drill is stamped before the save");
			}

			LF_WantedReports.Add(SERIAL_BUYER_UID, LF_WantedReports.OFFENCE_ROBBERY, "", "", "Bank", false);
		}

		// An unpaid tag rides "storeunpaid" (#479): the carrier pockets a juice off the general store
		// shelf and stays inside the store through the shutdown save, so the exit sweep leaves it alone.
		LF_StoreComponent tagStore = LF_Stores.Find("general_store");
		ctx.NotNull(tagStore, "e2e general store is placed for the unpaid tag");
		if (carrier && tagStore)
			PocketUnpaidJuice(ctx, tagStore, carrier);

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

		// The medic duty inventory rides the same carried payload in its own slot (medic_duty, #258).
		LF_PlayerAccount medicAccount = LF_TestPoliceKit.Account(MEDIC_UID, false);
		IEntity medicBody = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (medicBody)
		{
			ctx.EqualStr("", LF_DutyLocker.ClockIn(medicAccount, medicBody, LF_Faction.MEDIC), "e2e medic clocks in before the save");
			ctx.Equal(2, LF_InventoryUtils.AddAmount(medicBody, LF_TestCarriedInventoryKit.APPLE_PREFAB, 2), "e2e medic picks up 2 apples on shift");
			ctx.EqualStr("", LF_DutyLocker.ClockOut(medicAccount, medicBody, LF_Faction.MEDIC), "e2e medic clocks out before the save");
		}

		LF_WantedReports.Add(ACCOUNT_UID, LF_WantedReports.OFFENCE_ROBBERY, "#AR-Jacket_M88", "", "Fuel station", true);

		// A logged evidence bag (#269) must be on file after the restart.
		LF_EvidenceLockerRow lockerRow = new LF_EvidenceLockerRow();
		lockerRow.m_iNumber = 1;
		lockerRow.m_iReport = 77;
		lockerRow.m_sOffence = LF_WantedReports.OFFENCE_ROBBERY;
		lockerRow.m_sPlace = "Fuel station";
		lockerRow.m_sKind = LF_Evidence.KIND_CASING;
		lockerRow.m_sOfficerUid = OFFICER_UID;
		array<ref LF_EvidenceLockerRow> lockerRows = {};
		lockerRows.Insert(lockerRow);
		LF_EvidenceLocker.ApplyAll(lockerRows);

		// A half-finished chain batch (#417) must come back: barrel 3 of the Annex_Chains_Site, 3 of 8 units in.
		LF_ChainStationComponent chainBarrel = LF_ChainStations.Find(LF_TestAnnex.CHAINS_BARREL_3);
		ctx.NotNull(chainBarrel, "e2e chain barrel 3 is placed before the save");
		if (chainBarrel)
		{
			LF_ChainStationRow chainRow = new LF_ChainStationRow();
			chainRow.m_sStationKey = LF_TestAnnex.CHAINS_BARREL_3;
			chainRow.m_sChainId = "cider";
			chainRow.m_sStepId = "press";
			chainRow.m_iWork = 3;
			chainRow.m_iNeeded = 8;
			chainBarrel.Import(chainRow);
		}

		// The general store (#202) sells two apples and a restock is not due: the saved stock is 2 of 6.
		LF_StoreComponent saveStore = LF_Stores.Find("general_store");
		ctx.NotNull(saveStore, "e2e general store is placed before the save");
		if (saveStore)
		{
			saveStore.FirstFill();
			saveStore.ForceStock("{C9D661E5B0714711}Prefabs/Items/Food/Apple.et", 2);
			ctx.Equal(2, saveStore.GetStock("{C9D661E5B0714711}Prefabs/Items/Food/Apple.et"), "e2e store apple stock reads 2 before the save");
		}

		// Farm plots (#614, "farmplots" payload): plot 2 is ripe, plot 3 is 80 s in (half grown), plot 1 is empty.
		LF_FarmPlotComponent ripePlot = LF_FarmPlots.Find("farm_plot_2");
		LF_FarmPlotComponent growingPlot = LF_FarmPlots.Find("farm_plot_3");
		LF_FarmPlotComponent emptyPlot = LF_FarmPlots.Find("farm_plot_1");
		ctx.NotNull(ripePlot, "e2e farm plot 2 is placed before the save");
		ctx.NotNull(growingPlot, "e2e farm plot 3 is placed before the save");
		ctx.NotNull(emptyPlot, "e2e farm plot 1 is placed before the save");
		if (ripePlot && growingPlot && emptyPlot)
		{
			LF_FarmPlotRow ripeRow = new LF_FarmPlotRow();
			ripeRow.m_sPlotKey = "farm_plot_2";
			ripeRow.m_sCropId = ripePlot.GetCropId();
			ripeRow.m_iGrownSeconds = 600;
			ripePlot.Import(ripeRow);
			LF_FarmPlotRow growingRow = new LF_FarmPlotRow();
			growingRow.m_sPlotKey = "farm_plot_3";
			growingRow.m_sCropId = growingPlot.GetCropId();
			growingRow.m_iGrownSeconds = 80;
			growingPlot.Import(growingRow);
			emptyPlot.Import(null);
			ctx.True(ripePlot.IsRipe(), "e2e plot 2 is ripe before the save");
			ctx.Equal(1, growingPlot.GetStage(), "e2e plot 3 is half grown before the save");
		}

		// Characters per account (#331): slot 1 keyed by the account uid, with its bank under that
		// uid, wanted 1 and senior rank. A normal save adds slot 2 (its own key, bank 480, wanted 4)
		// and plays it ("characters" payload, save version 24). The legacy save (--legacy-save) is
		// written at version 23 and has one character, as every save before #331 did.
		LF_PlayerAccount slotsAccount = LF_PlayerAccount.Create(SLOTS_UID);
		playerAccounts.AddToCache(slotsAccount);
		slotsAccount.AddCharacter(LF_PlayerCharacter.Create("{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et", "Ada", "First", 36), true);
		slotsAccount.SetWantedLevel(1);
		slotsAccount.SetPoliceRank(LF_PoliceRanks.SENIOR);
		atmManager.CreateAccount(SLOTS_UID);
		atmManager.Deposit(SLOTS_UID, 120);
#ifndef LF_E2E_LEGACY_SAVE
		LF_PlayerCharacter slot2 = LF_PlayerCharacter.Create("{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et", "Bob", "Second", 40);
		slotsAccount.AddCharacter(slot2, true);
		slotsAccount.SetWantedLevel(4);
		atmManager.CreateAccount(slot2.GetRecordKey());
		atmManager.Deposit(slot2.GetRecordKey(), 480);
		ctx.False(slot2.GetRecordKey() == SLOTS_UID, "e2e slot 2 has its own key before the save");
#endif

		// Playtime accrues per account and job through the clock's write path; the restart must keep it.
		LF_Playtime.Credit(PLAYTIME_UID, "POLICE", 3600);
		LF_Playtime.Credit(PLAYTIME_UID, "POLICE", 1800);
		LF_Playtime.Credit(PLAYTIME_UID, LF_Playtime.JOB_CIVILIAN, 1800);
		ctx.Equal(7200, LF_Playtime.Total(PLAYTIME_UID), "e2e playtime account has 2 h before the save");

		SeedOrgLadder(ctx);

		LF_JobLevels.Forget(JOBS_UID);
		ctx.Equal(340, LF_JobLevels.AddXp(JOBS_UID, LF_JobLevels.JOB_CIVILIAN, 340, "e2e"), "e2e worker earns 340 civilian xp before the save");
		ctx.Equal(120, LF_JobLevels.AddXp(JOBS_UID, LF_JobLevels.JOB_FARMER, 120, "e2e"), "e2e worker earns 120 farmer xp before the save");
		ctx.Equal(650, LF_JobLevels.AddXp(JOBS_UID, "MINER", 650, "e2e"), "e2e worker earns 650 miner xp before the save");

		// The async manual save is the belt; the blocking shutdown save on game
		// close (OnGameEnd) is the suspenders the reload boot loads.
		manager.SaveGame();
		ctx.True(manager.IsSaveInProgress(), "SaveGame() accepts the manual save request");
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! #750a: the founder promotes the officer through the server path; the pack-style masks (the
	//! officer may manage territory, the member may not open the stash) only the "orgranks" rows hold.
	protected void SeedOrgLadder(LF_TestContext ctx)
	{
		LF_Organization org = LF_OrganizationManager.Create(ORG_FOUNDER, ORG_NAME);
		ctx.NotNull(org, "e2e organization founded before the save");
		if (!org)
			return;

		string orgId = org.GetId();
		ctx.True(LF_OrganizationManager.Invite(orgId, ORG_FOUNDER, ORG_OFFICER), "e2e officer-to-be joins before the save");
		ctx.True(LF_OrganizationManager.Invite(orgId, ORG_FOUNDER, ORG_MEMBER), "e2e member joins before the save");
		ctx.True(LF_OrganizationManager.Promote(orgId, ORG_FOUNDER, ORG_OFFICER), "e2e founder promotes the officer before the save");

		int officerRights = LF_OrganizationConfig.DefaultRights(LF_OrganizationConfig.RANK_OFFICER) | LF_OrgRights.Bit(LF_EOrganizationPermission.MANAGE_TERRITORY);
		array<ref LF_OrganizationRankRow> rows = {};
		rows.Insert(LF_OrganizationRankRow.Create(orgId, LF_OrganizationConfig.RANK_OFFICER, 1, officerRights));
		rows.Insert(LF_OrganizationRankRow.Create(orgId, LF_OrganizationConfig.RANK_MEMBER, 2, 0));
		LF_OrganizationManager.ApplyRanks(rows);
		ctx.True(org.Allows(ORG_OFFICER, LF_EOrganizationPermission.MANAGE_TERRITORY), "e2e officer may manage territory before the save");
		ctx.False(org.Allows(ORG_MEMBER, LF_EOrganizationPermission.USE_STASH), "e2e member may not use the stash before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! The carrier takes a juice off the shelf inside the store; the tag is keyed by the carried
	//! snapshot's character id (the fixture body has no player to give it a key).
	protected void PocketUnpaidJuice(LF_TestContext ctx, LF_StoreComponent store, IEntity carrier)
	{
		store.FirstFill();
		carrier.SetOrigin(store.GetOwner().GetOrigin() + "0 0.2 1.2");
		ctx.True(store.Contains(carrier.GetOrigin()), "e2e the carrier stands in the general store");
		IEntity juice = store.GetShelfItem(JUICE_PREFAB, 0);
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(carrier);
		ctx.True(juice != null && storage != null, "e2e a shelf juice and the carrier's inventory resolve");
		if (!juice || !storage)
			return;

		ctx.True(storage.TryInsertItem(juice), "e2e the carrier pockets a juice off the shelf");
		store.DetectTaken();
		LF_Stores.RefreshAll();
		array<LF_StoreUnpaidItem> tags = {};
		LF_Stores.CollectFor(store.GetStoreKey(), carrier, tags);
		ctx.Equal(1, tags.Count(), "e2e the juice is tagged unpaid before the save");
		foreach (LF_StoreUnpaidItem tag : tags)
			tag.m_sCarrierKey = CARRIED_CHARACTER_ID;
	}
}

//------------------------------------------------------------------------------------------------
// tier: PERSISTENCE
class LF_Test_E2E_SaveStateVerify : LF_Test
{
	protected static int s_iLooseApples;

	//------------------------------------------------------------------------------------------------
	protected static bool CountLooseApple(IEntity entity)
	{
		if (entity && LF_Utils.GetPrefabName(entity) == "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et" && LF_InventoryUtils.GetStorageHierachyRoot(entity) == entity)
			s_iLooseApples++;

		return true;
	}

	protected static const string ACCOUNT_UID = "e2e-uid";
	protected static const string SLOTS_UID = "e2e-slots-uid";
	protected static const string CARRIED_CHARACTER_ID = "e2e-carried-character";
	protected static const string SERIAL_BUYER_UID = "e2e-serial-buyer";
	protected static const string COURIER_UID = "e2e-courier";
	protected static const string OFFICER_UID = "e2e-officer";
	protected static const string DUTY_UID = "e2e-duty-officer";
	protected static const string MEDIC_UID = "e2e-duty-medic";
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
	//! #331 after the restart. Slot 1 always comes back keyed by the account uid with its bank,
	//! wanted level and rank. A version 24 save also brings slot 2 back, still the one played, with
	//! its own key, bank and wanted level; a version 23 save (--legacy-save) has only slot 1.
	protected void VerifyCharacterSlots(LF_TestContext ctx)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetInstance().GetFromCache(SLOTS_UID);
		ctx.NotNull(account, "e2e character-slots account present after the save load");
		if (!account)
			return;

		LF_PlayerCharacter slot1 = account.GetCharacterAt(0);
		ctx.True(slot1 != null, "e2e slot 1 present after the save load");
		if (!slot1)
			return;

		ctx.EqualStr("Ada First", slot1.GetFullName(), "e2e slot 1 is the first character");
		ctx.EqualStr(SLOTS_UID, slot1.GetRecordKey(), "e2e slot 1 is keyed by the account uid after the restart");
		ctx.Equal(1, slot1.GetStanding().m_iWantedLevel, "e2e slot 1 keeps wanted 1 across the restart");
		ctx.Equal(LF_PoliceRanks.SENIOR, slot1.GetStanding().m_iPoliceRank, "e2e slot 1 stays senior across the restart");
		LF_BankAccount slot1Bank = LF_ATMManager.GetInstance().GetAccount(SLOTS_UID);
		ctx.NotNull(slot1Bank, "e2e slot 1 bank present after the save load");
		if (slot1Bank)
			ctx.Equal(120, slot1Bank.GetBalance(), "e2e slot 1 bank holds 120 after the restart");

#ifdef LF_E2E_LEGACY_SAVE
		ctx.Equal(1, account.GetCharacterCount(), "e2e the version 23 save loads one character");
		ctx.EqualStr(SLOTS_UID, account.GetActiveCharacterKey(), "e2e the old character is the one played, under the account uid");
#else
		ctx.Equal(2, account.GetCharacterCount(), "e2e both characters back after the restart");
		LF_PlayerCharacter slot2 = account.GetCharacterAt(1);
		ctx.True(slot2 != null, "e2e slot 2 present after the save load");
		if (!slot2)
			return;

		ctx.EqualStr("Bob Second", slot2.GetFullName(), "e2e slot 2 is the second character");
		ctx.EqualStr(slot2.GetId(), slot2.GetRecordKey(), "e2e slot 2 keeps its own key after the restart");
		ctx.EqualStr(slot2.GetRecordKey(), account.GetActiveCharacterKey(), "e2e slot 2 is still the one played");
		ctx.Equal(4, slot2.GetStanding().m_iWantedLevel, "e2e slot 2 keeps wanted 4 across the restart");
		ctx.Equal(LF_PoliceRanks.PATROL, slot2.GetStanding().m_iPoliceRank, "e2e slot 2 has no rank of slot 1's");
		LF_BankAccount slot2Bank = LF_ATMManager.GetInstance().GetAccount(slot2.GetRecordKey());
		ctx.NotNull(slot2Bank, "e2e slot 2 bank present after the save load");
		if (slot2Bank)
			ctx.Equal(480, slot2Bank.GetBalance(), "e2e slot 2 bank holds 480 after the restart");
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! #692 after the restart: the playtime accrued before the save is back per job, not doubled. A
	//! version 23 save (--legacy-save) has no playtime payload, so the account starts at zero.
	protected void VerifyPlaytime(LF_TestContext ctx)
	{
		string uid = LF_Test_E2E_SaveStateWrite.PLAYTIME_UID;
#ifdef LF_E2E_LEGACY_SAVE
		ctx.Equal(0, LF_Playtime.Total(uid), "e2e the version 23 save has no playtime");
#else
		ctx.Equal(5400, LF_Playtime.JobSeconds(uid, "POLICE"), "e2e 90 min of police duty survive the restart");
		ctx.Equal(1800, LF_Playtime.JobSeconds(uid, LF_Playtime.JOB_CIVILIAN), "e2e 30 min civilian survive the restart");
		ctx.Equal(7200, LF_Playtime.Total(uid), "e2e the playtime total is 2 h after the restart");
		LF_Debug.Info(LF_Playtime.FEATURE, string.Format("e2e restart: %1 has %2 s (POLICE %3 s, CIVILIAN %4 s)", uid, LF_Playtime.Total(uid), LF_Playtime.JobSeconds(uid, "POLICE"), LF_Playtime.JobSeconds(uid, LF_Playtime.JOB_CIVILIAN)));
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! #749 after the restart: the job XP is back and the levels derive from it. A version 23 save
	//! (--legacy-save) has no "joblevels" rows: CIVILIAN migrates from the progression record's
	//! single level, and the jobs that level never held start at 0.
	protected void VerifyJobLevels(LF_TestContext ctx)
	{
		string uid = LF_Test_E2E_SaveStateWrite.JOBS_UID;
		ctx.Equal(340, LF_JobLevels.XpOf(uid, LF_JobLevels.JOB_CIVILIAN), "e2e civilian xp survives the restart");
		ctx.Equal(3, LF_JobLevels.LevelOf(uid, LF_JobLevels.JOB_CIVILIAN), "e2e worker is civilian level 3 after the restart");
#ifdef LF_E2E_LEGACY_SAVE
		ctx.Equal(0, LF_JobLevels.XpOf(uid, LF_JobLevels.JOB_FARMER), "e2e the version 23 save has no farmer xp");
		ctx.Equal(0, LF_JobLevels.XpOf(uid, "MINER"), "e2e the version 23 save has no miner xp");
#else
		ctx.Equal(120, LF_JobLevels.XpOf(uid, LF_JobLevels.JOB_FARMER), "e2e farmer xp survives the restart");
		ctx.Equal(2, LF_JobLevels.LevelOf(uid, LF_JobLevels.JOB_FARMER), "e2e worker is farmer level 2 after the restart");
		ctx.Equal(650, LF_JobLevels.XpOf(uid, "MINER"), "e2e miner xp survives the restart");
		ctx.Equal(4, LF_JobLevels.LevelOf(uid, "MINER"), "e2e worker is miner level 4 after the restart");
#endif
		LF_Debug.Info(LF_JobLevels.FEATURE, string.Format("e2e restart: %1 CIVILIAN %2 xp level %3, FARMER %4 xp, MINER %5 xp; courier DELIVERY %6 xp level %7", uid, LF_JobLevels.XpOf(uid, LF_JobLevels.JOB_CIVILIAN), LF_JobLevels.LevelOf(uid, LF_JobLevels.JOB_CIVILIAN), LF_JobLevels.XpOf(uid, LF_JobLevels.JOB_FARMER), LF_JobLevels.XpOf(uid, "MINER"), LF_CivilianJob.GetExperience(COURIER_UID), LF_CivilianJob.GetLevel(COURIER_UID)));
	}

	//------------------------------------------------------------------------------------------------
	//! #750a after the restart: the three members on their rungs, rights checked through the
	//! manager. A version 27 save brings the whole masks back from "orgranks"; a version 23 save
	//! (--legacy-save) has record version 1 organizations, so the masks come from the four bools.
	protected void VerifyOrgLadder(LF_TestContext ctx)
	{
		LF_Organization org = LF_OrganizationManager.GetByCharacter(LF_Test_E2E_SaveStateWrite.ORG_FOUNDER);
		ctx.NotNull(org, "e2e organization is back after the restart");
		if (!org)
			return;

		string founder = LF_Test_E2E_SaveStateWrite.ORG_FOUNDER;
		string officer = LF_Test_E2E_SaveStateWrite.ORG_OFFICER;
		string member = LF_Test_E2E_SaveStateWrite.ORG_MEMBER;
		ctx.EqualStr(LF_Test_E2E_SaveStateWrite.ORG_NAME, org.GetName(), "e2e organization keeps its name");
		ctx.Equal(3, org.GetMemberCount(), "e2e organization keeps its three members");
		ctx.Equal(3, org.GetRankCount(), "e2e organization keeps its three rungs");
		ctx.EqualStr(LF_OrganizationConfig.RANK_FOUNDER, org.GetRankId(founder), "e2e founder is still founder");
		ctx.EqualStr(LF_OrganizationConfig.RANK_OFFICER, org.GetRankId(officer), "e2e promoted officer is still officer");
		ctx.EqualStr(LF_OrganizationConfig.RANK_MEMBER, org.GetRankId(member), "e2e member is still member");
		ctx.Equal(0, org.RankPosition(founder), "e2e founder tops the ladder");
		ctx.Equal(1, org.RankPosition(officer), "e2e officer is the second rung");
		ctx.Equal(2, org.RankPosition(member), "e2e member is the bottom rung");
		ctx.Equal(LF_OrgRights.ALL, org.RightsOf(founder), "e2e founder holds every right");
		ctx.True(org.Allows(officer, LF_EOrganizationPermission.INVITE), "e2e officer may still invite");
		ctx.False(org.Allows(officer, LF_EOrganizationPermission.WITHDRAW), "e2e officer still may not withdraw");
		ctx.False(org.Allows(officer, LF_EOrganizationPermission.MANAGE_RANKS), "e2e officer still may not manage ranks");
#ifdef LF_E2E_LEGACY_SAVE
		ctx.False(org.Allows(officer, LF_EOrganizationPermission.MANAGE_TERRITORY), "e2e the version 23 save has no mask past the four bools");
		ctx.True(org.Allows(member, LF_EOrganizationPermission.USE_STASH), "e2e the version 23 member gets the stash every member had");
#else
		ctx.True(org.Allows(officer, LF_EOrganizationPermission.MANAGE_TERRITORY), "e2e officer keeps the territory right only orgranks holds");
		ctx.False(org.Allows(member, LF_EOrganizationPermission.USE_STASH), "e2e member stays without the stash right");
#endif
		LF_Debug.Info("Organizations", string.Format("e2e restart: %1 founder=%2 officer=%3 member=%4", org.GetName(), LF_OrgRights.Describe(org.RightsOf(founder)), LF_OrgRights.Describe(org.RightsOf(officer)), LF_OrgRights.Describe(org.RightsOf(member))));
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
				array<IEntity> backDrills = LF_InventoryUtils.FindItemsByPrefab(LF_InventoryUtils.GetResponsibleStorageManager(restored), LF_Bank.DRILL_PREFAB);
				ctx.Equal(1, backDrills.Count(), "e2e carried drill is back after the restart");
				if (!backDrills.IsEmpty())
				{
					LF_SerialComponent backStamp = LF_SerialComponent.Cast(backDrills[0].FindComponent(LF_SerialComponent));
					ctx.True(backStamp != null && !backStamp.GetSerial().IsEmpty(), "e2e carried drill keeps its serial after the restart");
					if (backStamp)
					{
						LF_FenceSerialRow bookRow = LF_FenceSerials.Find(backStamp.GetSerial());
						ctx.NotNull(bookRow, "e2e the serial book still names the carried drill's serial");
						if (bookRow)
							ctx.EqualStr(SERIAL_BUYER_UID, bookRow.m_sBuyerUid, "e2e the serial still leads to its buyer");
					}

					ctx.False(LF_WantedReports.IsIdentified(SERIAL_BUYER_UID), "e2e the buyer's report is not identified before the locker checks the drill");
					ctx.Equal(1, LF_FenceSerials.IdentifyItem(backDrills[0], "e2e evidence locker"), "e2e the evidence locker names the buyer from the restored drill");
					ctx.True(LF_WantedReports.IsIdentified(SERIAL_BUYER_UID), "e2e the buyer's report is identified by the restored drill");
				}

				ctx.Equal(0, LF_StorageSnapshot.Apply(restored, carried), "e2e second apply spawns nothing");
				ctx.Equal(237, LF_MoneyUtils.GetCash(restored), "e2e cash unchanged by the second apply");
				VerifyUnpaidJuice(ctx, restored);
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
		VerifyMedicInventory(ctx);
		ctx.Equal(1, LF_WantedReports.CountFor(ACCOUNT_UID), "e2e wanted report present after the save load");
		ctx.True(LF_WantedReports.IsIdentified(ACCOUNT_UID), "e2e wanted report stays identified after the save load");
		ctx.Equal(1, LF_EvidenceLocker.CountFor(77), "e2e evidence locker keeps its logged bag after the restart");
		LF_ChainStationComponent chainBarrel = LF_ChainStations.Find(LF_TestAnnex.CHAINS_BARREL_3);
		ctx.NotNull(chainBarrel, "e2e chain barrel 3 is placed after the restart");
		if (chainBarrel)
		{
			ctx.Equal(3, chainBarrel.GetProgress().GetWork(), "e2e half-finished chain batch keeps its work after the restart");
			ctx.Equal(37, chainBarrel.GetPercent(), "e2e half-finished chain batch reads 37 percent after the restart");
		}
		// The store stock rides "storestock" (#202): two apples left, so two stand on the shelf, not a fresh four.
		LF_StoreComponent loadedStore = LF_Stores.Find("general_store");
		ctx.NotNull(loadedStore, "e2e general store is placed after the restart");
		if (loadedStore)
		{
			loadedStore.FirstFill();
			ResourceName e2eApple = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
			ctx.Equal(2, loadedStore.GetStock(e2eApple), "e2e store apple stock stays 2 after the restart");
			ctx.Equal(2, loadedStore.GetShelfCount(e2eApple), "e2e the shelf shows the two apples left, not a doubled or fresh shelf");
			// No ground copy: loose apples around the counter are exactly the shelf's two (the engine did not
			// bring back an apple the save no longer owns, and nothing was spawned twice).
			s_iLooseApples = 0;
			GetGame().GetWorld().QueryEntitiesBySphere(loadedStore.GetOwner().GetOrigin(), 4, CountLooseApple, null, EQueryEntitiesFlags.ALL);
			ctx.Equal(2, s_iLooseApples, "e2e exactly two loose apples stand around the store after the restart");
		}

		// Farm plots ride "farmplots" (#614). Growth counts while a server runs, so plot 3 has grown at
		// least its saved 80 s; it may have ripened during the save boot's shutdown.
		LF_FarmPlotComponent loadedRipe = LF_FarmPlots.Find("farm_plot_2");
		LF_FarmPlotComponent loadedGrowing = LF_FarmPlots.Find("farm_plot_3");
		LF_FarmPlotComponent loadedEmpty = LF_FarmPlots.Find("farm_plot_1");
		ctx.NotNull(loadedRipe, "e2e farm plot 2 is placed after the restart");
		ctx.NotNull(loadedGrowing, "e2e farm plot 3 is placed after the restart");
		ctx.NotNull(loadedEmpty, "e2e farm plot 1 is placed after the restart");
		if (loadedRipe && loadedGrowing && loadedEmpty)
		{
			ctx.True(loadedRipe.IsRipe(), "e2e the ripe cabbages are still ripe after the restart");
			ctx.False(loadedGrowing.IsEmpty(), "e2e the half-grown plot is still planted after the restart");
			ctx.True(loadedGrowing.GetGrownSeconds() >= 80, string.Format("e2e the half-grown plot kept its growth after the restart (%1 s)", loadedGrowing.GetGrownSeconds()));
			ctx.True(loadedGrowing.GetStage() >= 1, "e2e the half-grown plot is at least half grown after the restart");
			ctx.True(loadedEmpty.IsEmpty(), "e2e the empty plot is still empty after the restart");
		}

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

		VerifyCharacterSlots(ctx);
		VerifyPlaytime(ctx);
		VerifyJobLevels(ctx);
		VerifyOrgLadder(ctx);

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
	//! #479 after the restart: the juice taken off the shelf before the save comes back in the carried
	//! inventory still unpaid. Its tag waited under the carried character id and goes back on the juice
	//! when the body is restored; the spawn point is not a walk out; the till takes the same price.
	protected void VerifyUnpaidJuice(LF_TestContext ctx, IEntity restored)
	{
		LF_StoreComponent store = LF_Stores.Find("general_store");
		ctx.NotNull(store, "e2e general store is placed for the unpaid tag after the restart");
		if (!store)
			return;

#ifdef LF_E2E_LEGACY_SAVE
		ctx.Equal(0, LF_Stores.ReattachUnpaid(CARRIED_CHARACTER_ID, restored), "e2e a version 23 save has no unpaid tags: the juice comes back untagged");
#else
		ctx.True(LF_Stores.ParkedCount() >= 1, "e2e the unpaid tag waits for its carrier after the save load");
		ctx.Equal(1, LF_Stores.ReattachUnpaid(CARRIED_CHARACTER_ID, restored), "e2e the unpaid tag goes back on the carried juice after the restart");
		array<IEntity> juices = LF_InventoryUtils.FindItemsByPrefab(LF_InventoryUtils.GetResponsibleStorageManager(restored), LF_Test_E2E_SaveStateWrite.JUICE_PREFAB);
		ctx.Equal(1, juices.Count(), "e2e the juice is carried after the restart");
		if (!juices.IsEmpty())
			ctx.True(LF_Stores.HoldsApart(juices[0]), "e2e the carried juice is still flagged unpaid after the restart");

		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(store.GetOwner());
		int price;
		if (shop)
			price = shop.GetBuyPrice(LF_Test_E2E_SaveStateWrite.JUICE_PREFAB);

		ctx.True(price > 0, "e2e a juice has a price");
		ctx.Equal(price, LF_Stores.OwedBy(store.GetStoreKey(), restored), "e2e the carrier still owes for the juice after the restart");
		ctx.False(store.Contains(restored.GetOrigin()), "e2e the restored body spawned outside the store");
		ctx.Equal(0, LF_Stores.SweepExits(), "e2e spawning away from the store after the restart is not shoplifting");

		LF_MoneyUtils.AddCash(restored, price);
		int paid;
		ctx.Equal(LF_EStorePay.PAID, LF_Stores.Pay(store, restored, paid), "e2e the till takes the restored bill");
		ctx.Equal(price, paid, "e2e the till charges the saved price");
		ctx.Equal(0, LF_Stores.OwedBy(store.GetStoreKey(), restored), "e2e nothing is owed once paid");
		LF_Stores.DeleteLooseReceipt();
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! After the restart: the medic inventory waits in its own slot and clocking in brings it back.
	protected void VerifyMedicInventory(LF_TestContext ctx)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetInstance().GetFromCache(MEDIC_UID);
		ctx.NotNull(account, "e2e duty medic account present after the save load");
		if (!account || !account.GetActiveCharacter())
			return;

		ctx.False(account.IsOnDuty(), "e2e medic is off duty after the save load");
		string characterId = account.GetActiveCharacter().GetId();
		LF_StorageSnapshotRecord stored = LF_StorageSnapshot.Get(characterId, LF_DutyLocker.SLOT_MEDIC);
		ctx.NotNull(stored, "e2e medic inventory stored at the desk after the save load");
		if (stored)
			ctx.Equal(2, stored.CountPrefab(LF_TestCarriedInventoryKit.APPLE_PREFAB), "e2e stored medic inventory keeps the 2 apples");

		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (body)
		{
			ctx.EqualStr("", LF_DutyLocker.ClockIn(account, body, LF_Faction.MEDIC), "e2e medic clocks in after the restart");
			ctx.Equal(2, LF_InventoryUtils.GetAmount(body, LF_TestCarriedInventoryKit.APPLE_PREFAB), "e2e the medic 2 apples come back after the restart");
			LF_DutyLocker.ClockOut(account, body, LF_Faction.MEDIC);
		}

		LF_StorageSnapshot.Clear(characterId, LF_DutyLocker.SLOT_MEDIC);
		LF_TestPoliceKit.Release(MEDIC_UID);
		LF_TestCharacterFixture.Delete(body);
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
