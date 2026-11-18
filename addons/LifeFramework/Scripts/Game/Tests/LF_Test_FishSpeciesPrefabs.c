// red-proof: 2026-11-18 tools/cli test --no-build --only LF_Test_FishSpeciesPrefabs on the unfixed conf (mackerel, bass, tuna, perch and carp had "" "" "" size prefabs) FAILED on every weighted species; RedProof() blanks the mackerel small prefab and the plain run is green.
//! fishing/species-prefabs: every species of the fishing conf (so every one a table weights) hands over a real fish (#3119). The raw
//! conf (before load-time cleaning, which would hide an empty row) gives each of them three size prefabs
//! that load, spawn as inventory items and have a catalogue row on the fish counter whose sell price rises with size. At load a
//! weighted species with an empty size prefab is dropped from its tables, and the other species stay.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FishSpeciesPrefabs : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "fishing/species-prefabs";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FishingConfig raw = LoadRaw(ctx);
		if (raw)
		{
			if (RedProof() && raw.m_aSpecies.Count() > 0 && raw.m_aSpecies[0].m_aSizePrefabs.Count() > 0)
				raw.m_aSpecies[0].m_aSizePrefabs[0] = "";

			CheckWeightedSpecies(ctx, raw);
		}

		CheckLoadDropsEmptySpecies(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! The conf as written, with no Sanitize.
	protected LF_FishingConfig LoadRaw(LF_TestContext ctx)
	{
		Resource container = BaseContainerTools.LoadContainer(LF_FishingConfig.CONFIG_PATH);
		ctx.True(container && container.GetResource(), "the fishing conf resolves");
		if (!container || !container.GetResource())
			return null;

		LF_FishingConfig config = LF_FishingConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		ctx.NotNull(config, "the fishing conf has the LF_FishingConfig root");
		if (config && (!config.m_aSpecies || !config.m_aTables))
			return null;

		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckWeightedSpecies(LF_TestContext ctx, LF_FishingConfig raw)
	{
		// Every species row, weighted by a table now or not (carp: a pack table may weight it).
		array<string> weighted = {};
		foreach (LF_FishSpeciesDef row : raw.m_aSpecies)
		{
			if (row)
				weighted.Insert(row.m_sId);
		}

		ctx.True(weighted.Count() >= 6, string.Format("the conf has the six core species (%1 found)", weighted.Count()));
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(catalogue, "the item catalogue loads");

		foreach (string id : weighted)
		{
			LF_FishSpeciesDef species = raw.FindSpecies(id);
			ctx.NotNull(species, string.Format("weighted species '%1' has a row", id));
			if (!species)
				continue;

			ctx.Equal(LF_FishingConfig.SIZE_COUNT, species.m_aSizePrefabs.Count(), string.Format("'%1' has three size prefabs", id));
			int lastPrice = 0;
			for (int size = 0; size < species.m_aSizePrefabs.Count(); size++)
			{
				string label = string.Format("'%1' size %2", id, size);
				ResourceName prefab = species.m_aSizePrefabs[size];
				ctx.False(prefab.IsEmpty(), label + " has a prefab");
				if (prefab.IsEmpty())
					continue;

				CheckFish(ctx, prefab, label);
				LF_CatalogueItem row = LF_ItemCatalogue.ItemByPrefab(catalogue, prefab);
				ctx.NotNull(row, label + " has a catalogue row");
				if (!row)
					continue;

				LF_CatalogueItem offered;
				ctx.NotNull(LF_ItemCatalogue.OfferFor(catalogue, "fish_counter", prefab, offered), label + " is on the fish counter");
				ctx.True(row.m_iSellPrice > lastPrice, string.Format("%1 sells for more than the size before (%2 after %3)", label, row.m_iSellPrice, lastPrice));
				lastPrice = row.m_iSellPrice;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab loads and spawns as an inventory item; the entity is deleted.
	protected void CheckFish(LF_TestContext ctx, ResourceName prefab, string label)
	{
		Resource res = Resource.Load(prefab);
		ctx.True(res.IsValid(), label + " prefab loads");
		if (!res.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity fish = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(fish, label + " spawns");
		if (!fish)
			return;

		ctx.NotNull(InventoryItemComponent.Cast(fish.FindComponent(InventoryItemComponent)), label + " is an inventory item");
		ctx.True(fish.GetVObject() != null, label + " has a mesh");
		SCR_EntityHelper.DeleteEntityAndChildren(fish);
	}

	//------------------------------------------------------------------------------------------------
	//! A weighted species with an empty prefab is dropped from its table at load; the other entry stays.
	protected void CheckLoadDropsEmptySpecies(LF_TestContext ctx)
	{
		array<ResourceName> good = {"{ACCC8B6FEB32034C}Prefabs/Items/Fishing/LF_FishTrout.et", "{ACCC8B6FEB32034C}Prefabs/Items/Fishing/LF_FishTrout.et", "{ACCC8B6FEB32034C}Prefabs/Items/Fishing/LF_FishTrout.et"};
		array<ResourceName> bad = {"{ACCC8B6FEB32034C}Prefabs/Items/Fishing/LF_FishTrout.et", "", "{ACCC8B6FEB32034C}Prefabs/Items/Fishing/LF_FishTrout.et"};
		array<string> ocean = {"OCEAN"};
		array<int> anyHour = {};

		LF_FishingConfig config = LF_FishingConfig.MakeEmpty();
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("goodfish", good, ocean, anyHour, 0));
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("badfish", bad, ocean, anyHour, 0));
		ctx.Equal(1, config.m_aSpecies[1].EmptyPrefabSize(), "the empty medium prefab is found");

		array<ref LF_FishWeightDef> weights = {};
		weights.Insert(LF_FishWeightDef.Make("goodfish", 1));
		weights.Insert(LF_FishWeightDef.Make("badfish", 5));
		array<int> sizeWeights = {1, 1, 1};
		config.m_aTables.Insert(LF_FishTableDef.Make("open_water", weights, sizeWeights));

		int removed = config.Sanitize();
		ctx.Equal(1, removed, "one table entry is dropped");
		LF_FishTableDef table = config.FindTable("open_water");
		ctx.NotNull(table, "the table survives");
		if (!table)
			return;

		ctx.Equal(1, table.m_aSpecies.Count(), "only the species with prefabs stays in the table");
		if (table.m_aSpecies.Count() == 1)
			ctx.EqualStr("goodfish", table.m_aSpecies[0].m_sSpecies, "the good species is the one kept");
	}
}
