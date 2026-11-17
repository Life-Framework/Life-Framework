// red-proof: 2026-11-17 `tools/cli test --no-build --only LF_Test_PersistenceKeepStash` on the unfixed LF_PersistenceKeep (log test-20261117-200027): FAIL "keepCarriedGear off drops the civilian stash", "keepCash off leaves no cash in civilian_stash (expected 0, got 237)", police_duty 40, medic_duty 15, "no pre-wipe civilian apple comes back (expected 0, got 3)" (the stash now holds 7 apples, since the civilian kit carries 2). Fixed: green. `--red LF_Test_PersistenceKeepStash` (log test-20261117-201308): RED-OK, fails "drops the carried snapshot", "drops the civilian stash", "the pre-wipe stash apples do not come back" with the wipe skipped.
// tier: WORLD
//! persistence/keep-stash (#2547): the carried-gear and cash switches reach the duty desk's stored
//! inventories. With keepCarriedGear off the civilian stash goes with the carried snapshot, so an
//! officer on duty at save time does not get pre-wipe gear back at clock-out; with keepCash off no
//! slot keeps cash. The duty kits keep their gear (the armory book holds their serials). An officer
//! whose stash was dropped clocks out into the civilian kit and can clock in and draw a kit again.
class LF_Test_PersistenceKeepStash : LF_Test
{
	protected static const string UID = "test-persist-keep-stash";
	protected static const int T0 = 1800000000;
	//! The stash's apples; the civilian kit carries fewer.
	protected static const int STASH_APPLES = 7;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persistence/keep-stash";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_StorageSnapshotRecord> snapshots = LF_StorageSnapshot.ExportAll();
		array<ref LF_ArmoryKitRow> books = LF_ArmoryKits.ExportAll();
		LF_ArmoryKits.ApplyAll(null);

		LF_PlayerAccount account = LF_TestPoliceKit.Account(UID, true);
		string characterId = account.GetActiveCharacter().GetId();

		RunCarriedGear(ctx, characterId);
		RunCash(ctx, characterId);
		RunClockOut(ctx, account, characterId);

		LF_StorageSnapshot.Clear(characterId, LF_DutyLocker.SLOT_MEDIC);
		LF_TestPoliceKit.Release(UID);
		LF_StorageSnapshot.ApplyAll(snapshots);
		LF_ArmoryKits.ApplyAll(books);
		LF_ConfigService.Reset();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PersistenceConfig Keep(string persistenceSection)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"persistence\":" + persistenceSection + "}", "test");
		return LF_PersistenceConfig.ResolveKeep();
	}

	//------------------------------------------------------------------------------------------------
	//! The officer saved on duty: police gear carried, the civilian inventory (apples, cash) at the
	//! desk, a medic kit left from an earlier shift. Every slot holds some cash.
	protected static void Seed(string characterId)
	{
		ResourceName cash = LF_MoneyUtils.CashPrefab();
		LF_StorageSnapshotRecord carried = LF_StorageSnapshotRecord.Create(characterId, LF_StorageSnapshot.SLOT_CARRIED);
		carried.m_aItems.Insert(LF_StorageItemRecord.Create(LF_TestPoliceKit.M9_PREFAB, 1, -1, 0, 0));
		carried.m_aItems.Insert(LF_StorageItemRecord.Create(cash, 10, -1, 1, 0));
		LF_StorageSnapshot.Set(carried);

		LF_StorageSnapshotRecord stash = LF_StorageSnapshotRecord.Create(characterId, LF_DutyLocker.SLOT_CIVILIAN);
		stash.m_aItems.Insert(LF_StorageItemRecord.Create(LF_TestCarriedInventoryKit.APPLE_PREFAB, STASH_APPLES, -1, 0, 0));
		stash.m_aItems.Insert(LF_StorageItemRecord.Create(cash, 237, -1, 0, 1));
		LF_StorageSnapshot.Set(stash);

		LF_StorageSnapshotRecord police = LF_StorageSnapshotRecord.Create(characterId, LF_DutyLocker.SLOT_POLICE);
		police.m_aItems.Insert(LF_StorageItemRecord.Create(LF_TestPoliceKit.M9_PREFAB, 1, -1, 0, 0));
		police.m_aItems.Insert(LF_StorageItemRecord.Create(cash, 40, -1, 1, 0));
		LF_StorageSnapshot.Set(police);

		LF_StorageSnapshotRecord medic = LF_StorageSnapshotRecord.Create(characterId, LF_DutyLocker.SLOT_MEDIC);
		medic.m_aItems.Insert(LF_StorageItemRecord.Create(cash, 15, -1, 0, 0));
		LF_StorageSnapshot.Set(medic);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCarriedGear(LF_TestContext ctx, string characterId)
	{
		Seed(characterId);
		if (!RedProof())
			LF_PersistenceKeep.ApplyAfterLoad(null, Keep("{\"keepCarriedGear\":false}"));

		ctx.False(LF_StorageSnapshot.Has(characterId, LF_StorageSnapshot.SLOT_CARRIED), "keepCarriedGear off drops the carried snapshot");
		ctx.False(LF_StorageSnapshot.Has(characterId, LF_DutyLocker.SLOT_CIVILIAN), "keepCarriedGear off drops the civilian stash");
		LF_StorageSnapshotRecord police = LF_StorageSnapshot.Get(characterId, LF_DutyLocker.SLOT_POLICE);
		ctx.NotNull(police, "the police duty kit stays (the armory book holds its serials)");
		if (police)
			ctx.Equal(1, police.CountPrefab(LF_TestPoliceKit.M9_PREFAB), "the duty kit keeps its pistol");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCash(LF_TestContext ctx, string characterId)
	{
		Seed(characterId);
		ctx.Equal(1, LF_PersistenceKeep.ApplyAfterLoad(null, Keep("{\"keepCash\":false}")), "keepCash off resets one field");

		ResourceName cash = LF_MoneyUtils.CashPrefab();
		array<string> slots = {LF_StorageSnapshot.SLOT_CARRIED, LF_DutyLocker.SLOT_CIVILIAN, LF_DutyLocker.SLOT_POLICE, LF_DutyLocker.SLOT_MEDIC};
		foreach (string slot : slots)
		{
			LF_StorageSnapshotRecord record = LF_StorageSnapshot.Get(characterId, slot);
			ctx.NotNull(record, string.Format("keepCash off keeps the %1 snapshot", slot));
			if (record)
				ctx.Equal(0, record.CountPrefab(cash), string.Format("keepCash off leaves no cash in %1", slot));
		}

		LF_StorageSnapshotRecord stash = LF_StorageSnapshot.Get(characterId, LF_DutyLocker.SLOT_CIVILIAN);
		if (stash)
			ctx.Equal(STASH_APPLES, stash.CountPrefab(LF_TestCarriedInventoryKit.APPLE_PREFAB), "the civilian stash keeps its apples");

		LF_StorageSnapshotRecord police = LF_StorageSnapshot.Get(characterId, LF_DutyLocker.SLOT_POLICE);
		if (police)
			ctx.Equal(1, police.CountPrefab(LF_TestPoliceKit.M9_PREFAB), "the duty kit keeps its pistol");

		Seed(characterId);
		LF_PersistenceKeep.ApplyAfterLoad(null, Keep("{\"keepCarriedGear\":false,\"keepCash\":false}"));
		LF_StorageSnapshotRecord duty = LF_StorageSnapshot.Get(characterId, LF_DutyLocker.SLOT_POLICE);
		ctx.NotNull(duty, "with both switches off the duty kit stays");
		if (duty)
			ctx.Equal(0, duty.CountPrefab(cash), "with both switches off the duty kit holds no cash");
	}

	//------------------------------------------------------------------------------------------------
	//! The officer loaded on duty after a gear wipe: no civilian stash. Clock-out issues the civilian
	//! kit, and the next shift draws a kit from the armory.
	protected void RunClockOut(LF_TestContext ctx, LF_PlayerAccount account, string characterId)
	{
		Seed(characterId);
		if (!RedProof())
			LF_PersistenceKeep.ApplyAfterLoad(null, Keep("{\"keepCarriedGear\":false}"));
		LF_ConfigService.Reset();

		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		account.SetPoliceRank(LF_PoliceRanks.SENIOR);
		LF_PoliceRanks.BindOfficer(body, UID);

		ctx.EqualStr("", LF_DutyLocker.ClockOut(account, body), "the officer clocks out with no civilian stash");
		ctx.False(account.IsOnDuty(), "off duty after clock-out");
		ctx.True(LF_InventoryUtils.GetAmount(body, LF_TestCarriedInventoryKit.APPLE_PREFAB) < STASH_APPLES, "the pre-wipe stash apples do not come back (the civilian kit carries fewer)");
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(body);
		array<IEntity> items = {};
		if (manager)
			manager.GetItems(items);
		ctx.True(items.Count() > 0, "the officer wears the civilian kit, not nothing");

		ctx.EqualStr("", LF_DutyLocker.ClockIn(account, body), "the officer clocks in again");
		ctx.True(LF_PoliceRanks.DrawAt(UID, body, T0), "the officer can still draw a kit");
		ctx.True(LF_ArmoryKits.Outstanding(account.GetPersistentId()) > 0, "the drawn kit is in the armory book");

		LF_DutyLocker.ClockOut(account, body);
		LF_TestCharacterFixture.Delete(body);
	}
}
