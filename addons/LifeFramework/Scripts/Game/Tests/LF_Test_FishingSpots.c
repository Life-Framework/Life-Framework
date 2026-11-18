// red-proof: 2026-11-18 tools/cli test --no-build --red LF_Test_FishingSpots RED-OK (RedProof() spawns no spot and reads no catalogue, so every check fails); the plain run is green.
//! fishing/spots: a fishing spot a map maker drops registers in LF_Places as kind FishingSpot and the cast
//! finds it by position (#2929): the spot whose radius covers the target, none outside it, the nearest marker
//! when two overlap; its stock thins per catch. The core catalogue puts the three trout sizes on the fish
//! counter at their prices (below the market ceiling where the market curves the fish) and the rod and the
//! bait tin on the chandler.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FishingSpots : LF_Test
{
	protected const ResourceName SPOT = "{E8C5BD1716B703E0}Prefabs/Systems/Fishing/LF_FishingSpot.et";
	protected const ResourceName ROD = "{0327968D17D249FD}Prefabs/Items/Fishing/LF_FishingRod.et";
	protected const ResourceName TIN = "{53E0B27FA119A866}Prefabs/Items/Fishing/LF_BaitTin.et";
	protected const ResourceName TROUT_SMALL = "{FAEF245300848A2C}Prefabs/Items/Fishing/LF_FishTroutSmall.et";
	protected const ResourceName TROUT = "{ACCC8B6FEB32034C}Prefabs/Items/Fishing/LF_FishTrout.et";
	protected const ResourceName TROUT_LARGE = "{368CD7CE4669CC29}Prefabs/Items/Fishing/LF_FishTroutLarge.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "fishing/spots";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckSpots(ctx);
		CheckCatalogue(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSpots(LF_TestContext ctx)
	{
		vector origin = Vector(40, 0, 40);
		IEntity quay = SpawnSpot(ctx, origin, "the quay spot");
		IEntity cove = SpawnSpot(ctx, origin + Vector(30, 0, 0), "the cove spot");
		if (!quay || !cove)
		{
			Delete(quay);
			Delete(cove);
			return;
		}

		array<IEntity> places = {};
		int found = LF_Places.FindAll(LF_FishingSpotComponent.KIND, places);
		ctx.True(found >= 2, string.Format("both spots are found by kind FishingSpot (%1 found)", found));
		ctx.True(places.Contains(quay) && places.Contains(cove), "the registry holds the two spots just placed");

		LF_FishingSpotComponent quaySpot = LF_FishingSpotComponent.Of(quay);
		LF_FishingSpotComponent coveSpot = LF_FishingSpotComponent.Of(cove);
		ctx.NotNull(quaySpot, "the quay carries LF_FishingSpotComponent");
		ctx.NotNull(coveSpot, "the cove carries LF_FishingSpotComponent");
		if (!quaySpot || !coveSpot)
		{
			Delete(quay);
			Delete(cove);
			return;
		}

		// Prefab defaults: radius 12 m, so a target 5 m from the quay is inside it, 20 m from both is outside.
		ctx.True(LF_FishingSpotComponent.FindSpotFor(origin + Vector(5, 0, 0)) == quaySpot, "a cast 5 m from the quay fishes the quay");
		ctx.True(LF_FishingSpotComponent.FindSpotFor(origin + Vector(30, 0, 4)) == coveSpot, "a cast next to the cove fishes the cove");
		ctx.True(LF_FishingSpotComponent.FindSpotFor(origin + Vector(15, 0, 0)) == null, "a cast 15 m from the quay and 15 m from the cove is in no spot's radius: open water");
		ctx.True(LF_FishingSpotComponent.FindSpotFor(origin + Vector(0, 0, 200)) == null, "a cast far from every spot is open water");

		// The height of the target does not move it out of the spot (flat distance).
		ctx.True(LF_FishingSpotComponent.FindSpotFor(origin + Vector(3, -4, 0)) == quaySpot, "the cast target's height does not matter");

		// Two spots that overlap at a target: the nearer marker wins.
		cove.SetOrigin(origin + Vector(16, 0, 0));
		ctx.True(LF_FishingSpotComponent.FindSpotFor(origin + Vector(9, 0, 0)) == coveSpot, "where two spots cover a target, the nearer marker wins");
		ctx.True(LF_FishingSpotComponent.FindSpotFor(origin + Vector(6, 0, 0)) == quaySpot, "and the other way round");

		// Stock: starts full, one fewer per catch.
		int full = quaySpot.StockLeft(1000);
		ctx.True(full > 0, string.Format("a new spot holds fish (%1)", full));
		ctx.True(quaySpot.TakeFish(1000), "a catch takes a fish");
		ctx.Equal(full - 1, quaySpot.StockLeft(1000), "the stock is one fewer after a catch");
		ctx.Equal(full, coveSpot.StockLeft(1000), "another spot's stock is untouched");
		ctx.EqualStr("harbour", quaySpot.GetTable(), "the prefab's default table is the harbour's");

		Delete(quay);
		Delete(cove);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnSpot(LF_TestContext ctx, vector position, string label)
	{
		if (RedProof())
		{
			ctx.Fail(label + " red proof: nothing spawned");
			return null;
		}

		Resource res = Resource.Load(SPOT);
		ctx.True(res.IsValid(), label + " prefab loads");
		if (!res.IsValid())
			return null;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		IEntity spot = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(spot, label + " spawns");
		return spot;
	}

	//------------------------------------------------------------------------------------------------
	protected void Delete(IEntity entity)
	{
		if (entity)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckCatalogue(LF_TestContext ctx)
	{
		if (RedProof())
		{
			ctx.Fail("red proof: the catalogue is not read");
			return;
		}

		LF_ItemCatalogueConfig config = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(config, "the item catalogue loads");
		if (!config)
			return;

		int small = CounterPays(ctx, config, TROUT_SMALL, "small trout");
		int medium = CounterPays(ctx, config, TROUT, "trout");
		int large = CounterPays(ctx, config, TROUT_LARGE, "large trout");
		ctx.True(small > 0 && small < medium && medium < large, string.Format("the counter pays more for a bigger fish (%1, %2, %3)", small, medium, large));

		LF_CatalogueItem item;
		ctx.True(LF_ItemCatalogue.OfferFor(config, "chandler", ROD, item) != null, "the chandler sells the rod");
		ctx.True(LF_ItemCatalogue.OfferFor(config, "chandler", TIN, item) != null, "the chandler sells the bait tin");
		ctx.True(LF_ItemCatalogue.OfferFor(config, "fish_counter", ROD, item) == null, "the fish counter does not sell the rod");
		ctx.True(LF_ItemCatalogue.OfferFor(config, "chandler", TROUT, item) == null, "the chandler does not deal in fish");
	}

	//------------------------------------------------------------------------------------------------
	//! Checks the fish counter's row for one fish and returns what it pays for it: it pays something, never
	//! more than it asks (a profit loop), and under the market's ceiling when the market curves that fish.
	protected int CounterPays(LF_TestContext ctx, LF_ItemCatalogueConfig config, ResourceName prefab, string label)
	{
		LF_CatalogueItem item;
		LF_OutletOffer offer = LF_ItemCatalogue.OfferFor(config, "fish_counter", prefab, item);
		ctx.True(offer != null && item != null, "the fish counter prices the " + label);
		if (!offer || !item)
			return 0;

		ctx.True(item.m_iSellPrice > 0, string.Format("the counter pays for the %1 (%2)", label, item.m_iSellPrice));
		ctx.True(item.m_iSellPrice <= item.m_iPrice, string.Format("the %1 never sells back above its price (%2 vs %3)", label, item.m_iSellPrice, item.m_iPrice));
		LF_MarketConfig market = LF_MarketConfig.Get();
		if (market)
		{
			LF_MarketGoodDef good = market.FindGood(prefab);
			if (good)
				ctx.True(item.m_iSellPrice <= good.m_iCeiling, string.Format("the %1's price stays under the market ceiling %2", label, good.m_iCeiling));
		}

		return item.m_iSellPrice;
	}
}
