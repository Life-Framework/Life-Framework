// red-proof: 2026-11-18 (#2995, FISHER xp and LF_OnFishCaught assertions) tools/cli test --no-build --red LF_Test_FishingFloat,LF_Test_FishingCast RED-PROOF OK (RedProof() skips the water cast, so no fish is landed: the xp, the event and the recorder assertions fail).
// red-proof: 2026-11-18 tools/cli test --no-build --red LF_Test_FishingCast RED-OK (RedProof() makes no water cast and leaves the cast's player unset, so the cast, strike, catch and recipient assertions fail); the plain run is green.
//! fishing/cast: the server's half of a cast (#2930). A cast onto land is refused and leaves nothing
//! out; a cast onto water with the rod in the hands and bait in the pack is accepted; a second cast
//! while one is out is refused; a strike before the server's own bite is ignored (nothing spent);
//! a strike inside the window after the bite gives exactly one fish of the table (open water: small,
//! a spot: the spot's table, large) and spends one bait; a late strike gives none and spends the bait;
//! a rod that is not in the hands, a target out of range and a forged target are refused.
//! Records every LF_OnFishCaught the bus carries (#2995).
class LF_TestFishCaughtRecorder : LF_EventSubscriber
{
	int m_iCount;
	string m_sUid;
	string m_sSpecies;
	string m_sPrefab;
	string m_sWater;
	string m_sSpotTable;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnFishCaught caught = LF_OnFishCaught.Cast(busEvent);
		if (!caught)
			return true;

		m_iCount++;
		m_sUid = caught.m_sPlayerUid;
		m_sSpecies = caught.m_sSpeciesId;
		m_sPrefab = caught.m_sFishPrefab;
		m_sWater = caught.m_sWater;
		m_sSpotTable = caught.m_sSpotTable;
		return true;
	}
}

//------------------------------------------------------------------------------------------------
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FishingCast : LF_Test
{
	protected const ResourceName ROD = "{0327968D17D249FD}Prefabs/Items/Fishing/LF_FishingRod.et";
	protected const ResourceName TIN = "{53E0B27FA119A866}Prefabs/Items/Fishing/LF_BaitTin.et";
	protected const ResourceName TROUT_SMALL = "{FAEF245300848A2C}Prefabs/Items/Fishing/LF_FishTroutSmall.et";
	protected const ResourceName TROUT = "{ACCC8B6FEB32034C}Prefabs/Items/Fishing/LF_FishTrout.et";
	protected const ResourceName TROUT_LARGE = "{368CD7CE4669CC29}Prefabs/Items/Fishing/LF_FishTroutLarge.et";
	protected const ResourceName SPOT = "{E8C5BD1716B703E0}Prefabs/Systems/Fishing/LF_FishingSpot.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "fishing/cast";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_FishingConfig MakeConfig()
	{
		LF_FishingConfig config = LF_FishingConfig.MakeEmpty();
		config.m_aRods.Insert(LF_FishingRodDef.Make(ROD, 20, 1, 0));
		array<string> noBoost = {};
		config.m_aBaits.Insert(LF_FishingBaitDef.Make(TIN, 1, noBoost));

		array<ResourceName> sizes = {TROUT_SMALL, TROUT, TROUT_LARGE};
		array<string> both = {LF_FishingConfig.WATER_OCEAN, LF_FishingConfig.WATER_POND};
		array<int> anyHour = {};
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("trout", sizes, both, anyHour, 0));

		array<ref LF_FishWeightDef> weights = {};
		weights.Insert(LF_FishWeightDef.Make("trout", 1));
		array<int> smallOnly = {1, 0, 0};
		array<int> largeOnly = {0, 0, 1};
		config.m_aTables.Insert(LF_FishTableDef.Make("open_water", weights, smallOnly));
		config.m_aTables.Insert(LF_FishTableDef.Make("harbour", weights, largeOnly));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! A character with a pack, the rod and `tins` tins of bait in it.
	protected IEntity Angler(LF_TestContext ctx, int tins, out IEntity rod)
	{
		rod = null;
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return null;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(character);
		IEntity pack = LF_Utils.SpawnEntityPrefab(LF_TestVaultDrillFixture.PACK_PREFAB, character.GetOrigin());
		if (!pack || !inventory || !inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY))
		{
			ctx.Fail("fixture: the angler cannot wear a pack");
			LF_TestCharacterFixture.Delete(character);
			return null;
		}

		bool ok = LF_InventoryUtils.AddItem(inventory, ROD);
		for (int i = 0; i < tins; i++)
		{
			if (!LF_InventoryUtils.AddItem(inventory, TIN))
				ok = false;
		}

		rod = LF_HandTool.Carried(character, LF_FishingSession.TOOL_ID);
		if (!ok || !rod)
		{
			ctx.Fail("fixture: the rod and the bait do not fit the pack, or the rod is not a hand tool with id rod");
			LF_TestCharacterFixture.Delete(character);
			return null;
		}

		return character;
	}

	//------------------------------------------------------------------------------------------------
	protected int Count(IEntity character, ResourceName prefab)
	{
		return LF_InventoryUtils.GetAmount(character, prefab);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FishingConfig.UseForTest(MakeConfig());

		IEntity rod;
		IEntity angler = Angler(ctx, 4, rod);
		if (!angler)
		{
			LF_FishingConfig.Reload();
			return;
		}

		string key = LF_FishingSession.KeyOf(angler);
		LF_FishingSession.End(key);
		vector water = angler.GetOrigin() + Vector(8, 0, 0);
		float now = 1000;
		LF_TestFishCaughtRecorder caught = new LF_TestFishCaughtRecorder();
		LF_EventBus.Subscribe(LF_OnFishCaught, caught);
		int xpBefore = LF_JobLevels.XpOf(key, LF_FishingSession.JOB);

		// The cast's events go to the player who cast, not to whoever drives the body now: a body that
		// died mid-cast maps to no player, and its lost ENDED left the client's line out for good.
		LF_FishingCast orphan = new LF_FishingCast();
		if (!RedProof())
			orphan.m_iPlayerId = 7;

		ctx.Equal(0, LF_FishingSession.PlayerIdOf(angler), "fixture: the test body has no player");
		ctx.Equal(7, LF_FishingSession.RecipientOf(orphan, angler), "a cast's events reach the caster after the body lost its player");

		// The rod is in the pack, not in the hands: refused, nothing out.
		ctx.True(LF_FishingSession.CastOn(angler, water, "OCEAN", now) == LF_EFishingRefusal.NO_ROD, "a rod in the pack does not cast");
		ctx.True(LF_FishingSession.Open(key) == null, "a refused cast leaves nothing out");

		LF_TestVaultDrillFixture.Hold(angler, rod, true);
		ctx.True(LF_HandTool.Holds(angler, LF_FishingSession.TOOL_ID), "fixture: the rod is held");

		// Land: the real water query at the angler's feet finds none.
		ctx.True(LF_FishingSession.CastAt(angler, water, now) == LF_EFishingRefusal.NOT_WATER, "a cast onto land is refused");
		ctx.True(LF_FishingSession.Open(key) == null, "a cast onto land leaves nothing out");
		ctx.True(LF_FishingSession.CastOn(angler, angler.GetOrigin() + Vector(60, 0, 0), "OCEAN", now + 1) == LF_EFishingRefusal.RANGE, "a cast beyond the rod's range is refused");
		ctx.False(LF_FishingSession.ValidTarget(Vector(0, 0, 1000000)), "a forged huge target is not valid");
		ctx.True(LF_FishingSession.ValidTarget(water), "a real target is valid");
		ctx.Equal(4, Count(angler, TIN), "no refusal spent bait");

		// Water: the cast goes out.
		LF_EFishingRefusal first = LF_EFishingRefusal.OFF;
		if (!RedProof())
			first = LF_FishingSession.CastOn(angler, water, "OCEAN", now + 2);

		ctx.True(first == LF_EFishingRefusal.NONE, "a cast onto water with the rod and bait goes out");
		LF_FishingCast cast = LF_FishingSession.Open(key);
		ctx.NotNull(cast, "the server holds the open cast");
		if (!cast)
		{
			LF_EventBus.Unsubscribe(LF_OnFishCaught, caught);
			Finish(angler, null, key);
			return;
		}

		ctx.False(cast.m_bBitten, "a new cast has not bitten");
		ctx.True(LF_FishingSession.CastOn(angler, water, "OCEAN", now + 3) == LF_EFishingRefusal.ALREADY_OUT, "a second cast while one is out is refused");
		ctx.Equal(cast.m_iCastId, LF_FishingSession.Open(key).m_iCastId, "the refused second cast left the first one alone");

		// A strike with no server bite is ignored: nothing given, nothing spent, the cast stays.
		ctx.True(LF_FishingSession.Strike(angler, cast.m_iCastId, now + 4) == LF_EFishingStrike.IGNORED, "a strike before the server's bite is ignored");
		ctx.Equal(0, Count(angler, TROUT_SMALL), "an early strike gave no fish");
		ctx.Equal(4, Count(angler, TIN), "an early strike spent no bait");
		ctx.True(LF_FishingSession.Open(key) != null, "the cast is still out after an early strike");
		ctx.True(LF_FishingSession.Strike(angler, cast.m_iCastId + 99, now + 4) == LF_EFishingStrike.IGNORED, "a strike for another cast id is ignored");

		// The bite, then a strike inside the window: one small fish (open water), one bait.
		ctx.True(LF_FishingSession.Bite(key, cast.m_iCastId, now + 20), "the server's bite happens");
		ctx.True(cast.m_bBitten, "the cast has bitten");
		ctx.True(LF_FishingSession.Strike(angler, cast.m_iCastId, now + 21) == LF_EFishingStrike.CAUGHT, "a strike inside the window lands");
		ctx.Equal(1, Count(angler, TROUT_SMALL), "open water gave exactly one small fish");
		ctx.Equal(0, Count(angler, TROUT_LARGE), "open water gave no large fish");
		ctx.Equal(3, Count(angler, TIN), "the catch spent one bait");
		ctx.True(LF_FishingSession.Open(key) == null, "the cast is over after the catch");

		// The catch pays FISHER XP (the fish_catch row of the jobs config) and is published once (#2995).
		ctx.True(LF_JobLevels.XpOf(key, LF_FishingSession.JOB) > xpBefore, "a landed fish books FISHER xp");
		ctx.Equal(1, caught.m_iCount, "the bus saw one fish caught");
		ctx.EqualStr(key, caught.m_sUid, "the event names the angler's record key");
		ctx.EqualStr("trout", caught.m_sSpecies, "the event names the species");
		ctx.EqualStr(TROUT_SMALL, caught.m_sPrefab, "the event names the fish prefab");
		ctx.EqualStr("OCEAN", caught.m_sWater, "the event names the water");
		ctx.EqualStr("", caught.m_sSpotTable, "open water has no spot table");
		ctx.True(LF_FishingSession.Strike(angler, cast.m_iCastId, now + 21.1) == LF_EFishingStrike.IGNORED, "a second strike on the finished cast is ignored");
		ctx.Equal(1, Count(angler, TROUT_SMALL), "the second strike gave nothing");

		// A spot under the target: its table (large only) and its stock.
		IEntity spotEntity = LF_Utils.SpawnEntityPrefab(SPOT, water);
		LF_FishingSpotComponent spot = LF_FishingSpotComponent.Of(spotEntity);
		ctx.NotNull(spot, "fixture: the spot spawns");
		if (spot)
		{
			int full = spot.StockLeft(now + 30);
			ctx.True(LF_FishingSession.CastOn(angler, water, "POND", now + 30) == LF_EFishingRefusal.NONE, "a cast at a spot goes out");
			LF_FishingCast atSpot = LF_FishingSession.Open(key);
			ctx.NotNull(atSpot, "the spot cast is open");
			if (atSpot)
			{
				ctx.True(LF_FishingSession.Bite(key, atSpot.m_iCastId, now + 40), "the spot cast bites");
				ctx.True(LF_FishingSession.Strike(angler, atSpot.m_iCastId, now + 41) == LF_EFishingStrike.CAUGHT, "a strike lands at the spot");
				ctx.Equal(1, Count(angler, TROUT_LARGE), "the spot's table gave one large fish");
				ctx.Equal(1, Count(angler, TROUT_SMALL), "and no more small ones");
				ctx.Equal(full - 1, spot.StockLeft(now + 41), "the catch thinned the spot by one");
				ctx.Equal(2, Count(angler, TIN), "the second catch spent a second bait");
				ctx.Equal(2, caught.m_iCount, "the spot catch was published too");
				ctx.EqualStr("harbour", caught.m_sSpotTable, "the spot catch names the spot's table");
				ctx.EqualStr("POND", caught.m_sWater, "the spot catch names the water");
			}
		}

		// A late strike: no fish, the bait is gone.
		ctx.True(LF_FishingSession.CastOn(angler, water, "OCEAN", now + 50) == LF_EFishingRefusal.NONE, "a third cast goes out");
		LF_FishingCast late = LF_FishingSession.Open(key);
		ctx.NotNull(late, "the third cast is open");
		if (late)
		{
			ctx.True(LF_FishingSession.Bite(key, late.m_iCastId, now + 55), "the third cast bites");
			ctx.True(LF_FishingSession.Strike(angler, late.m_iCastId, now + 55 + 10) == LF_EFishingStrike.LATE, "a strike ten seconds after the bite is late");
			ctx.Equal(1, Count(angler, TROUT_SMALL), "the late strike gave no fish");
			ctx.Equal(1, Count(angler, TROUT_LARGE), "the late strike gave no fish of the other size either");
			ctx.Equal(1, Count(angler, TIN), "the late strike spent the bait");
			ctx.Equal(2, caught.m_iCount, "a late strike publishes no catch");
			ctx.True(LF_FishingSession.Open(key) == null, "the cast is over after the late strike");
		}

		// The rate limit: six casts a minute (the config). Every attempt that reached the check since
		// `now` counts (land, range, the second cast, the three that went out): the seventh is refused.
		ctx.True(LF_FishingSession.CastOn(angler, water, "OCEAN", now + 56) == LF_EFishingRefusal.RATE, "the cast over the minute's limit is refused");
		ctx.True(LF_FishingSession.Open(key) == null, "the rate refusal left nothing out");

		LF_EventBus.Unsubscribe(LF_OnFishCaught, caught);
		Finish(angler, spotEntity, key);
	}

	//------------------------------------------------------------------------------------------------
	protected void Finish(IEntity angler, IEntity spot, string key)
	{
		LF_FishingSession.End(key);
		if (spot)
			SCR_EntityHelper.DeleteEntityAndChildren(spot);

		LF_TestCharacterFixture.Delete(angler);
		LF_FishingConfig.Reload();
	}
}
