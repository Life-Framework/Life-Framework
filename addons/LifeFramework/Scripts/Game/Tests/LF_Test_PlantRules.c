// red-proof: observed 2026-11-17 with `tools\cli test --no-build --red LF_Test_PlantRules` (RED PROOF OK): with the hook on
// the watering is skipped, so the plant dies and `- a rotten plant shows the last stage (expected 2, got 0)` and the
// ripe and rot assertions fail. With the hook off the same test passes (`--only`, 2026-11-17).

//! The planting and growing rules of a catalogue crop (#2663): stage from seconds grown, a missed
//! water window kills the plant, watering inside the window resets it, a ripe crop rots after its
//! lifetime, and a seed is refused too close to another plant, out of reach, from a vehicle, in a safe
//! zone and past the per-character or server cap.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PlantRules : LF_Test
{
	override string GetName()
	{
		return "farm/plant-rules";
	}

	protected static LF_CropRow Row(int waterMax)
	{
		LF_CropRow row = new LF_CropRow();
		row.m_sCropId = "test_crop";
		row.m_iGrowthSeconds = 600;
		row.m_iWaterMinSeconds = 60;
		row.m_iWaterMaxSeconds = waterMax;
		row.m_iLifetimeAfterRipe = 1800;
		row.m_iYield = 3;
		row.m_aStagePrefabs = new array<ResourceName>();
		row.m_aStagePrefabs.Insert("a");
		row.m_aStagePrefabs.Insert("b");
		row.m_aStagePrefabs.Insert("c");
		return row;
	}

	override void Run(LF_TestContext ctx)
	{
		LF_CropRow thirsty = Row(240);
		LF_CropRow dry = Row(0);

		// Stage from seconds grown: three stages, two growing ones of 300 s, the ripe one holds.
		ctx.Equal(0, LF_PlantRules.StageForSeconds(thirsty, 0), "a fresh planting is the first stage");
		ctx.Equal(0, LF_PlantRules.StageForSeconds(thirsty, 299), "still the first stage before 300 s");
		ctx.Equal(1, LF_PlantRules.StageForSeconds(thirsty, 300), "the second stage at 300 s");
		ctx.Equal(1, LF_PlantRules.StageForSeconds(thirsty, 599), "still the second stage before 600 s");
		ctx.Equal(2, LF_PlantRules.StageForSeconds(thirsty, 600), "ripe at the growth time");
		ctx.Equal(2, LF_PlantRules.StageForSeconds(thirsty, 100000), "a ripe crop never runs past the last stage");
		ctx.Equal(LF_FarmPlotRules.EMPTY, LF_PlantRules.StageForSeconds(null, 10), "no row, no stage");
		LF_CropRow bare = Row(240);
		bare.m_aStagePrefabs = null;
		ctx.Equal(1, LF_PlantRules.StageForSeconds(bare, 600), "a row without stage prefabs still ripens in two stages");

		// A missed water window kills the plant.
		LF_PlantState missed = new LF_PlantState();
		LF_PlantRules.Advance(thirsty, missed, 240);
		ctx.True(missed.m_eStatus == LF_EPlantStatus.GROWING, "at the end of the window the plant is alive");
		LF_PlantRules.Advance(thirsty, missed, 1);
		ctx.True(missed.m_eStatus == LF_EPlantStatus.DEAD, "one second past the window the plant is dead");
		ctx.True(LF_PlantRules.Water(thirsty, missed) == LF_EPlantWater.NOT_GROWING, "a dead plant takes no water");
		float deadGrown = missed.m_fGrownSeconds;
		LF_PlantRules.Advance(thirsty, missed, 1000);
		ctx.EqualFloat(deadGrown, missed.m_fGrownSeconds, 0.001, "a dead plant stops growing");

		LF_PlantState gap = new LF_PlantState();
		LF_PlantRules.Advance(thirsty, gap, 5000);
		ctx.True(gap.m_eStatus == LF_EPlantStatus.DEAD, "one long gap past the window kills the plant, it does not ripen on the way");
		ctx.True(gap.m_fGrownSeconds <= 240.001, "a plant that died in a gap grew only to the end of its window");

		// Watering inside the window resets it; before the window nothing changes.
		LF_PlantState early = new LF_PlantState();
		LF_PlantRules.Advance(thirsty, early, 59);
		ctx.True(LF_PlantRules.Water(thirsty, early) == LF_EPlantWater.TOO_EARLY, "watering before the window is refused");
		ctx.EqualFloat(59, early.m_fSinceWaterSeconds, 0.001, "a refused watering leaves the water clock alone");

		LF_PlantState tended = new LF_PlantState();
		for (int tick = 0; tick < 5; tick++)
		{
			LF_PlantRules.Advance(thirsty, tended, 100);
			if (!RedProof())
				ctx.True(LF_PlantRules.Water(thirsty, tended) == LF_EPlantWater.OK, "watering inside the window is accepted");
		}

		ctx.True(tended.m_eStatus == LF_EPlantStatus.GROWING, "a plant watered every 100 s is still growing");
		ctx.EqualFloat(0, tended.m_fSinceWaterSeconds, 0.001, "watering resets the water clock");
		ctx.EqualFloat(500, tended.m_fGrownSeconds, 0.001, "watering does not change the growth");

		// Ripe, then rots after lifetimeAfterRipe; a ripe plant needs no water.
		LF_PlantRules.Advance(thirsty, tended, 100);
		ctx.True(tended.m_eStatus == LF_EPlantStatus.RIPE, "ripe once the growth time is up");
		ctx.True(LF_PlantRules.Water(thirsty, tended) == LF_EPlantWater.NOT_GROWING, "a ripe plant takes no water");
		LF_PlantRules.Advance(thirsty, tended, 1799);
		ctx.True(tended.m_eStatus == LF_EPlantStatus.RIPE, "still ripe just before the lifetime ends, with no water");
		LF_PlantRules.Advance(thirsty, tended, 1);
		ctx.True(tended.m_eStatus == LF_EPlantStatus.ROTTEN, "rotten when the lifetime after ripe is up");
		LF_PlantRules.Advance(thirsty, tended, 100);
		ctx.True(tended.m_eStatus == LF_EPlantStatus.ROTTEN, "a rotten plant stays rotten");

		LF_PlantState slow = new LF_PlantState();
		LF_PlantRules.Advance(dry, slow, 700);
		ctx.True(slow.m_eStatus == LF_EPlantStatus.RIPE, "a crop that needs no water ripens unwatered");
		ctx.EqualFloat(100, slow.m_fRipeSeconds, 0.001, "the time past ripe in one step counts toward the lifetime");
		LF_PlantRules.Advance(dry, slow, 100000);
		ctx.True(slow.m_eStatus == LF_EPlantStatus.ROTTEN, "a long gap rots a ripe crop");

		LF_CropRow keeps = Row(0);
		keeps.m_iLifetimeAfterRipe = 0;
		LF_PlantState evergreen = new LF_PlantState();
		LF_PlantRules.Advance(keeps, evergreen, 1000000);
		ctx.True(evergreen.m_eStatus == LF_EPlantStatus.RIPE, "a lifetime of 0 never rots");

		ctx.Equal(2, LF_PlantRules.StageOf(thirsty, tended), "a rotten plant shows the last stage");
		ctx.Equal(LF_FarmPlotRules.EMPTY, LF_PlantRules.StageOf(thirsty, null), "no state, no stage");

		// Placement.
		ctx.True(LF_PlantRules.CanPlace(-1, 2, false, false, 0, 0) == LF_EPlantPlace.OK, "a clear spot in reach is plantable");
		ctx.True(LF_PlantRules.CanPlace(1.5, 4, false, false, 11, 299) == LF_EPlantPlace.OK, "exactly 1.5 m from a plant and 4 m away is the edge and plantable");
		ctx.True(LF_PlantRules.CanPlace(1.4, 2, false, false, 0, 0) == LF_EPlantPlace.TOO_CLOSE, "within 1.5 m of another plant is refused");
		ctx.True(LF_PlantRules.CanPlace(-1, 4.1, false, false, 0, 0) == LF_EPlantPlace.TOO_FAR, "beyond 4 m reach is refused");
		ctx.True(LF_PlantRules.CanPlace(-1, 2, true, false, 0, 0) == LF_EPlantPlace.IN_VEHICLE, "planting from a vehicle is refused");
		ctx.True(LF_PlantRules.CanPlace(-1, 2, false, true, 0, 0) == LF_EPlantPlace.SAFE_ZONE, "planting in a safe zone is refused");
		ctx.True(LF_PlantRules.CanPlace(-1, 2, false, false, 12, 12) == LF_EPlantPlace.CHARACTER_CAP, "a 13th plant for one character is refused");
		ctx.True(LF_PlantRules.CanPlace(-1, 2, false, false, 0, 300) == LF_EPlantPlace.SERVER_CAP, "a 301st plant on the server is refused");
		ctx.True(LF_PlantRules.CanPlace(-1, 9, true, true, 12, 300) == LF_EPlantPlace.IN_VEHICLE, "the vehicle rule is told first");

		ctx.EqualStr("", LF_PlantRules.PlaceRefusalKey(LF_EPlantPlace.OK), "OK needs no words");
		ctx.EqualStr("#LF-Plant_TooClose", LF_PlantRules.PlaceRefusalKey(LF_EPlantPlace.TOO_CLOSE), "too close has words");
		ctx.EqualStr("#LF-Plant_ServerCap", LF_PlantRules.PlaceRefusalKey(LF_EPlantPlace.SERVER_CAP), "the server cap has words");

		// The catalogue finds a row by id and by seed.
		LF_CropCatalogue catalogue = new LF_CropCatalogue();
		catalogue.m_aCrops = new array<ref LF_CropRow>();
		thirsty.m_SeedPrefab = "{0000000000000001}Prefabs/Seed.et";
		catalogue.m_aCrops.Insert(thirsty);
		ctx.True(catalogue.FindIn("test_crop") == thirsty, "a crop is found by its id");
		ctx.True(catalogue.FindIn("nope") == null, "an unknown crop id finds nothing");
		ctx.True(catalogue.FindBySeed("{0000000000000001}Prefabs/Seed.et") == thirsty, "a crop is found by its seed");
		ctx.True(catalogue.FindBySeed("") == null, "an empty seed finds nothing");
	}
}
