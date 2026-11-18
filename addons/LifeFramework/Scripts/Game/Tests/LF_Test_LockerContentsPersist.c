// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_LockerContentsPersist`: `RED-OK lockers/contents-persist`: the handgun store skipped, `- the locker snapshot holds the handgun (apply 1) (expected 1, got 0)`. The restart half runs in `tools\cli wt e2e` (the save boot stores, the reload boot opens and finds the handgun). Review 2026-11-18 (#3043): the e2e also leaves an LF_LockerContents container holding a handgun open through the shutdown save and checks the reload boot has no container and no loose handgun (vanilla StorageHolder tracks storages); green on the PR code, kept as the open-at-save dupe guard.
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
	//! Where the e2e save boot leaves a locker open through the shutdown save, from the fixture gate.
	protected static const vector OPEN_OFFSET = "6 0 6";
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
		vector openAt = LF_TestDebrisProbe.Gate() + OPEN_OFFSET;
		LF_TestCharacterFixture.Delete(renter);
		ctx.True(LF_StorageSnapshot.Has(E2E_KEY, LF_LockerStorageManagerComponent.SlotFor(E2E_SITE)), "e2e the locker snapshot is stored before the save");

		// A locker still open at the save (a renter browsing it during an autosave or the shutdown): the
		// transient container holds a handgun while the snapshot holds the same handgun. The vanilla
		// StorageHolder rule tracks every entity with an inventory storage; if it saved and self-spawned this
		// container, the restart would bring back a second handgun beside the snapshot's.
		IEntity live = LF_Utils.SpawnEntityPrefab(LF_LockerStorageManagerComponent.CONTENTS_PREFAB, openAt);
		ctx.NotNull(live, "e2e a locker container stands open through the shutdown save");
		if (live)
		{
			ctx.Equal(1, LF_InventoryUtils.AddAmount(live, HANDGUN, 1), "e2e the open locker holds a handgun at the save");
			LF_TestDebrisProbe.Keep(live);
			// The reload boot's "none came back" counts with this same query; here it must see the live one.
			ctx.Equal(1, CountNear(openAt, 2, LF_LockerStorageManagerComponent.CONTENTS_PREFAB, false), "e2e the world query finds the open locker container");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Entities of a prefab within `radius` of `at`; rootsOnly counts only those lying loose (no parent).
	static int CountNear(vector at, float radius, ResourceName prefab, bool rootsOnly)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return -1;

		s_aFound = {};
		s_sWanted = prefab;
		s_bRootsOnly = rootsOnly;
		world.QueryEntitiesBySphere(at, radius, CollectWanted, null, EQueryEntitiesFlags.ALL);
		int count = s_aFound.Count();
		s_aFound = null;
		return count;
	}

	protected static ref array<EntityID> s_aFound;
	protected static ResourceName s_sWanted;
	protected static bool s_bRootsOnly;

	//------------------------------------------------------------------------------------------------
	protected static bool CollectWanted(IEntity entity)
	{
		if (!entity || LF_Utils.GetPrefabName(entity) != s_sWanted)
			return true;

		if (!s_bRootsOnly || !entity.GetParent())
			s_aFound.Insert(entity.GetID());

		return true;
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
		ctx.Equal(0, CountNear(LF_TestDebrisProbe.Gate(), 20000, LF_LockerStorageManagerComponent.CONTENTS_PREFAB, false), "no locker container came back from the save: an open locker is its snapshot, not a world entity");
		ctx.Equal(0, CountNear(LF_TestDebrisProbe.Gate() + OPEN_OFFSET, 10, HANDGUN, true), "the handgun of the locker open at the save did not come back loose");
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
