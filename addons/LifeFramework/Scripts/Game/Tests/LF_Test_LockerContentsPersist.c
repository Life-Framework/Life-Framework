// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_LockerContentsPersist`: `RED-OK lockers/contents-persist`: the handgun store skipped, `- the locker snapshot holds the handgun (apply 1) (expected 1, got 0)`. The restart half runs in `tools\cli wt e2e` (the save boot stores, the reload boot opens and finds the handgun).
// tier: PERSISTENCE
//! lockers/contents-persist (#2661): what a renter stored in their locker is a `locker:<siteId>` snapshot in the
//! carried payload store, so it survives the store's export and re-apply (the same path a save and load take) and,
//! in `tools\cli wt e2e`, a real restart: the save boot stores a handgun, the reload boot opens the locker and
//! finds it.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_LockerContentsPersist : LF_Test
{
	protected static const string KEY = "locker-contents-persist-a";
	protected static const string E2E_SITE = "locker-e2e-site-2661";
	protected static const string E2E_KEY = "locker-e2e-character-2661";
	protected static const string SITE = "locker-contents-persist-site";
	protected static const ResourceName HANDGUN = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lockers/contents-persist";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_LockerRentalRow> kept = LF_LockerBank.ExportAll();
		IEntity renter = LF_TestCharacterFixture.Spawn(ctx);
		if (renter)
		{
			StoreHandgun(ctx, renter, KEY, SITE, RedProof());
			LF_TestCharacterFixture.Delete(renter);
		}

		// The store's export and re-apply is the save and load: the locker is still there, twice.
		string slot = LF_LockerStorageManagerComponent.SlotFor(SITE);
		for (int pass = 1; pass <= 2; pass++)
		{
			LF_StorageSnapshot.ApplyAll(LF_StorageSnapshot.ExportAll());
			LF_StorageSnapshotRecord stored = LF_StorageSnapshot.Get(KEY, slot);
			ctx.NotNull(stored, string.Format("the locker snapshot survives the store round trip (apply %1)", pass));
			if (stored)
				ctx.Equal(1, stored.CountPrefab(HANDGUN), string.Format("the locker snapshot holds the handgun (apply %1)", pass));
		}

		LF_StorageSnapshot.Clear(KEY, slot);
		LF_LockerBank.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot (called by LF_Test_E2E_SaveStateWrite): one rental and a stored handgun, left for the shutdown save.
	static void SeedE2E(LF_TestContext ctx)
	{
		IEntity renter = LF_TestCharacterFixture.Spawn(ctx);
		if (!renter)
			return;

		StoreHandgun(ctx, renter, E2E_KEY, E2E_SITE, false);
		LF_TestCharacterFixture.Delete(renter);
		ctx.True(LF_StorageSnapshot.Has(E2E_KEY, LF_LockerStorageManagerComponent.SlotFor(E2E_SITE)), "e2e the locker snapshot is stored before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! Rents a locker for the character's key and puts a handgun in it through a real open and close.
	protected static void StoreHandgun(LF_TestContext ctx, IEntity renter, string key, string site, bool skipStore)
	{
		array<ref LF_LockerRentalRow> rows = LF_LockerBank.ExportAll();
		rows.Insert(LF_LockerRentalRow.Create(site, key, LF_LockerRules.NEVER));
		LF_LockerBank.ApplyAll(rows);

		vector at = renter.GetOrigin();
		IEntity container;
		ctx.Equal(LF_ELockerOpenResult.OPENED, LF_LockerStorageManagerComponent.OpenAt(renter, key, site, at, at + "1 0 0", 2.5, container), "the renter opens their locker");
		if (container && !skipStore)
			ctx.Equal(1, LF_InventoryUtils.AddAmount(container, HANDGUN, 1), "a handgun goes into the locker");

		LF_LockerStorageManagerComponent open = LF_LockerStorageManagerComponent.FindOpen(site, key);
		if (open)
			open.Release("persist test");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot (called by LF_Test_E2E_SaveStateVerify): the rental and the handgun came back from the saved carried payload.
	static void VerifyE2E(LF_TestContext ctx)
	{
		ctx.True(LF_LockerBank.IsRenting(E2E_SITE, E2E_KEY), "the rental is back after the restart");
		IEntity renter = LF_TestCharacterFixture.Spawn(ctx);
		if (!renter)
			return;

		vector at = renter.GetOrigin();
		IEntity container;
		ctx.Equal(LF_ELockerOpenResult.OPENED, LF_LockerStorageManagerComponent.OpenAt(renter, E2E_KEY, E2E_SITE, at, at + "1 0 0", 2.5, container), "the renter opens their locker after the restart");
		if (container)
			ctx.Equal(1, LF_Test_LockerRoundtrip.Count(container, HANDGUN), "the handgun is in the locker after the restart");

		LF_LockerStorageManagerComponent open = LF_LockerStorageManagerComponent.FindOpen(E2E_SITE, E2E_KEY);
		if (open)
			open.Release("e2e load boot");

		LF_TestCharacterFixture.Delete(renter);
	}
}
