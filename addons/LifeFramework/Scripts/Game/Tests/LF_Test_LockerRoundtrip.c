// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_LockerRoundtrip,LF_Test_LockerIsolation`: `RED PROOF OK` with the first store skipped; it failed `- the second open holds two items, not four (expected 2, got 0)`, `- the respawned renter finds both items (expected 2, got 0)`, `- the handgun survived the death (expected 1, got 0)`. The first green run also showed a handgun brings its magazine, so the snapshot holds three records, not two.
// tier: WORLD
//! lockers/roundtrip (#2661): a renter opens their locker, stores a handgun and an apple, closes it, opens it
//! again and finds the same two items. Death drops the carried snapshot but never the locker snapshot, and a
//! respawned character with the same record key finds the items. A second open of the same site releases the
//! first (captured) one, so nothing is lost or doubled.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LockerRoundtrip : LF_Test
{
	protected static const string KEY = "locker-roundtrip-test-a";
	protected static const string SITE = "locker-roundtrip-test-site";
	protected static const ResourceName HANDGUN = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lockers/roundtrip";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_LockerRentalRow> kept = LF_LockerBank.ExportAll();
		LF_LockerBank.ResetStatics();

		IEntity first = LF_TestCharacterFixture.Spawn(ctx);
		IEntity second = LF_TestCharacterFixture.Spawn(ctx);
		if (first && second)
			Check(ctx, first, second);

		if (first)
			LF_TestCharacterFixture.Delete(first);
		if (second)
			LF_TestCharacterFixture.Delete(second);

		LF_StorageSnapshot.Clear(KEY, LF_LockerStorageManagerComponent.SlotFor(SITE));
		LF_StorageSnapshot.Clear(KEY, LF_StorageSnapshot.SLOT_CARRIED);
		LF_LockerBank.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity renter, IEntity respawned)
	{
		vector at = renter.GetOrigin();
		vector bank = at + "1 0 0";
		float reach = 2.5;
		string slot = LF_LockerStorageManagerComponent.SlotFor(SITE);

		LF_MoneyUtils.RemoveCash(renter, LF_MoneyUtils.GetCash(renter));
		LF_MoneyUtils.AddCash(renter, 100);
		ctx.Equal(LF_ELockerBankResult.RENTED, LF_LockerBank.RentAt(1800000000, KEY, renter, at, SITE, bank, reach, 100, 7), "the renter rents the locker");

		IEntity container;
		ctx.Equal(LF_ELockerOpenResult.OPENED, LF_LockerStorageManagerComponent.OpenAt(renter, KEY, SITE, at, bank, reach, container), "the renter opens their locker");
		ctx.NotNull(container, "an open locker is a container");
		if (!container)
			return;

		ctx.Equal(1, LF_LockerStorageManagerComponent.OpenCount(), "one locker is open");
		ctx.Equal(0, CountItems(container), "a fresh locker is empty");
		if (!RedProof())
		{
			ctx.Equal(1, LF_InventoryUtils.AddAmount(container, HANDGUN, 1), "a handgun goes into the locker");
			ctx.Equal(1, LF_InventoryUtils.AddAmount(container, APPLE, 1), "an apple goes into the locker");
		}

		LF_LockerStorageManagerComponent manager = LF_LockerStorageManagerComponent.FindOpen(SITE, KEY);
		ctx.NotNull(manager, "the open locker is found by site and renter");
		if (!manager)
			return;

		ctx.True(manager.Release("test close"), "closing the locker releases it");
		ctx.Equal(0, LF_LockerStorageManagerComponent.OpenCount(), "no locker is open after the close");

		LF_StorageSnapshotRecord stored = LF_StorageSnapshot.Get(KEY, slot);
		ctx.NotNull(stored, "the close stored a snapshot under locker:<siteId>");
		if (!stored)
			return;

		ctx.Equal(1, stored.CountPrefab(APPLE), "the apple is in the stored locker");
		ctx.Equal(1, stored.CountPrefab(HANDGUN), "the handgun is in the stored locker");

		IEntity reopened;
		ctx.Equal(LF_ELockerOpenResult.OPENED, LF_LockerStorageManagerComponent.OpenAt(renter, KEY, SITE, at, bank, reach, reopened), "the renter reopens their locker");
		ctx.Equal(2, CountItems(reopened), "the reopened locker holds the same two items");
		ctx.Equal(1, Count(reopened, HANDGUN), "the handgun is back in the reopened locker");
		ctx.Equal(1, Count(reopened, APPLE), "the apple is back in the reopened locker");

		// A second open while open captures the first and builds a fresh one: still two items, not four.
		IEntity again;
		ctx.Equal(LF_ELockerOpenResult.OPENED, LF_LockerStorageManagerComponent.OpenAt(renter, KEY, SITE, at, bank, reach, again), "opening again while open is allowed");
		ctx.Equal(1, LF_LockerStorageManagerComponent.OpenCount(), "still one locker open");
		ctx.Equal(2, CountItems(again), "the second open holds two items, not four");

		// Death drops what is carried, never what is in a locker.
		LF_StorageSnapshotRecord carried = LF_StorageSnapshotRecord.Create(KEY, LF_StorageSnapshot.SLOT_CARRIED);
		carried.m_aItems.Insert(LF_StorageItemRecord.Create(APPLE, 1, -1, 0, 0));
		LF_StorageSnapshot.Set(carried);
		ctx.True(LF_StorageSnapshot.Has(KEY, LF_StorageSnapshot.SLOT_CARRIED), "something is carried before death");

		LF_LockerStorageManagerComponent open = LF_LockerStorageManagerComponent.FindOpen(SITE, KEY);
		if (open)
			open.Release("test death");
		LF_StorageSnapshot.OnCharacterDied(KEY);
		ctx.False(LF_StorageSnapshot.Has(KEY, LF_StorageSnapshot.SLOT_CARRIED), "death clears the carried snapshot");
		ctx.True(LF_StorageSnapshot.Has(KEY, slot), "death leaves the locker snapshot");

		// A respawn is a new entity with the same record key: it finds the items.
		IEntity afterRespawn;
		ctx.Equal(LF_ELockerOpenResult.OPENED, LF_LockerStorageManagerComponent.OpenAt(respawned, KEY, SITE, respawned.GetOrigin(), respawned.GetOrigin() + "1 0 0", reach, afterRespawn), "the respawned renter opens their locker");
		ctx.Equal(2, CountItems(afterRespawn), "the respawned renter finds both items");
		ctx.Equal(1, Count(afterRespawn, HANDGUN), "the handgun survived the death");

		LF_LockerStorageManagerComponent last = LF_LockerStorageManagerComponent.FindOpen(SITE, KEY);
		if (last)
			last.Release("test end");
		ctx.Equal(0, LF_LockerStorageManagerComponent.OpenCount(), "nothing is left open");
	}

	//------------------------------------------------------------------------------------------------
	static BaseInventoryStorageComponent StorageOf(IEntity container)
	{
		return BaseInventoryStorageComponent.Cast(container.FindComponent(BaseInventoryStorageComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! \return Items directly in the container's storage.
	static int CountItems(IEntity container)
	{
		BaseInventoryStorageComponent storage = StorageOf(container);
		if (!storage)
			return -1;

		array<IEntity> items = {};
		storage.GetAll(items, false);
		return items.Count();
	}

	//------------------------------------------------------------------------------------------------
	static int Count(IEntity container, ResourceName prefab)
	{
		BaseInventoryStorageComponent storage = StorageOf(container);
		if (!storage)
			return -1;

		array<IEntity> items = {};
		storage.GetAll(items, false);
		int total;
		foreach (IEntity item : items)
		{
			if (LF_Utils.GetPrefabName(item) == prefab)
				total++;
		}

		return total;
	}
}
