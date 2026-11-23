// red-proof: 2026-11-23 `test --no-build --red LF_Test_EveronPharmacyLoot` RED-OK: with the `RedProof()` hook swapping the pack haul for an apple, `the haul holds narcotic units (got 0)` and `the full haul is class B or C (got '', 0 units)` fail; first run before LF_HospitalLoot existed failed to compile; green with the real conf.
//! #4447 (crime D3): the island pharmacy cabinet's loot comes from the pack conf, every line resolves, the
//! narcotic lines (morphine ampoules, pill bottles) are classed NARCOTICS for a stop search, and
//! `LF_GoodsClass` classes a cabinet's full haul as B or C. The haul goes into a backpack (the insert the
//! juice bottle failed on 0.2.1, #4369). A cabinet's own list wins over the pack conf. Deletes what it spawns.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronPharmacyLoot : LF_Test
{
	static const ResourceName PACK = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	override string GetName()
	{
		return "everon/pharmacy-loot";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_VehicleSearchGoods.ResetStatics();
		LF_HospitalLoot.ResetStatics();

		// The cabinet's own list wins; with none, the pack conf; the core default is the last resort.
		array<ResourceName> own = {APPLE};
		array<ResourceName> chosen = {};
		LF_HospitalLoot.Choose(own, chosen);
		ctx.Equal(1, chosen.Count(), "a cabinet's own list wins over the pack conf");

		array<ResourceName> loot = {};
		LF_HospitalLoot.Choose(null, loot);
		if (RedProof())
		{
			loot.Clear();
			loot.Insert(APPLE);
		}

		ctx.True(LF_HospitalLoot.HasConfig(), "the pack conf lists the cabinet loot");
		ctx.True(loot.Count() > 4, string.Format("the cabinet loot comes from the pack conf (got %1 items)", loot.Count()));

		array<ResourceName> resolved = {};
		ctx.Equal(loot.Count(), LF_Hospital.ResolveLoot(loot, resolved), "every cabinet item resolves");

		int narcotics = LF_HospitalLoot.NarcoticUnits(loot);
		LF_EGoodsClass haulClass = LF_HospitalLoot.HaulClass(loot);
		ctx.True(narcotics >= 3, string.Format("the haul holds narcotic units (got %1)", narcotics));
		ctx.True(haulClass == LF_EGoodsClass.B || haulClass == LF_EGoodsClass.C, string.Format("the full haul is class B or C (got '%1', %2 units)", LF_GoodsClass.Letter(haulClass), narcotics));

		// The narcotic items carry the stop-search class and fit a backpack.
		LF_HospitalLoot.RegisterGoods();
		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		IEntity pack = LF_Utils.SpawnEntityPrefab(PACK, robber.GetOrigin());
		ctx.True(pack != null && inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the character wears a field pack");
		BaseInventoryStorageComponent packStorage;
		if (pack)
			packStorage = BaseInventoryStorageComponent.Cast(pack.FindComponent(BaseInventoryStorageComponent));
		ctx.True(packStorage != null, "the pack has a storage");

		int classed = 0;
		foreach (ResourceName prefab : resolved)
		{
			IEntity item = LF_Utils.SpawnEntityPrefab(prefab, robber.GetOrigin());
			if (!item)
			{
				ctx.Fail("a cabinet item spawns: " + prefab);
				continue;
			}

			if (packStorage)
				ctx.True(inventory.TryInsertItemInStorage(item, packStorage), "a cabinet item goes into the backpack: " + prefab);

			if (LF_VehicleSearchGoods.ClassOf(item) == LF_EVehicleSearchGoodsClass.NARCOTICS)
				classed++;
		}

		ctx.Equal(narcotics, classed, "every narcotic unit in the haul is classed NARCOTICS for a stop search");
		LF_TestCharacterFixture.Delete(robber);
		LF_VehicleSearchGoods.ResetStatics();
		LF_HospitalLoot.ResetStatics();
	}
}
