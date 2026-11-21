// red-proof: 2026-11-21, with the floor snap skipped (`cli test --no-build --red LF_Test_ResidentSpawnSpot` skips the
//   sweep that spawns the two residents) both residents are missing and every position check fails; the unfixed
//   spawner stood them at the anchor height, 0.4 m over the floor, which the `feet on the floor` lines catch.
//! #3885: two residents due on one anchor spawn on the floor under it, not inside each other. The anchor is put
//! 0.4 m over the ground on purpose: a body spawned at the anchor height hovers. The test measures the floor
//! under each resident itself (a trace down, every layer, the body excluded), then checks the feet are within
//! 5 cm of it and the two bodies are at least 0.5 m apart, and that the runtime overlap sweep finds no pair.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentSpawnSpot : LF_Test
{
	protected static const string SHOP_KIND = "test_residents_spot_shop";
	protected static const string HOME_KIND = "test_residents_spot_home";
	protected static const ResourceName PLACE_PREFAB = "{25C8B889A777399D}Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et";
	protected static const ResourceName ANCHOR_PREFAB = "{CCED18E36D627B1D}Prefabs/Systems/Residents/LF_ResidentAnchor.et";
	protected static const int WORK_MINUTE = 450;	// 07:30
	protected static const float HOVER_M = 0.4;
	protected static const float FEET_TOLERANCE_M = 0.05;
	protected static const float APART_M = 0.5;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/spawn-spot";
	}

	//------------------------------------------------------------------------------------------------
	//! Open DebugWorld terrain for the stand-ins: the first candidate with ground under it (no town coordinates).
	//! False when none has any.
	protected bool FindGround(out vector at)
	{
		array<vector> candidates = {"420 0 420", "380 0 460", "460 0 380", "200 0 330", "330 0 60"};
		foreach (vector candidate : candidates)
		{
			float y = GetGame().GetWorld().GetSurfaceY(candidate[0], candidate[2]);
			if (y > -100)
			{
				at = Vector(candidate[0], y, candidate[2]);
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The floor under a body found by a trace from just above its feet, the body itself excluded.
	protected bool FloorUnder(IEntity body, out float floorY)
	{
		IEntity hit;
		vector from = body.GetOrigin() + "0 0.4 0";
		float d = LF_PlacementAudit.SurfaceTrace(from, from - "0 2 0", body, hit);
		if (d >= 1.99)
			return false;

		floorY = from[1] - d;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		vector shopAt;
		bool open = FindGround(shopAt);
		ctx.True(open, "the DebugWorld has open ground for the stand-ins");
		if (!open)
			return;

		vector homeAt = shopAt + "600 0 0";
		IEntity shop = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, shopAt, "0 0 0", false);
		IEntity home = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, homeAt, "0 0 0", false);
		ctx.True(shop && home, "the test shop and home spawn");
		if (!shop || !home)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(shop);
			SCR_EntityHelper.DeleteEntityAndChildren(home);
			return;
		}

		ctx.True(LF_Places.Register(SHOP_KIND, "spot_shop", shop), "the shop is placed");
		ctx.True(LF_Places.Register(HOME_KIND, "spot_home", home), "the home is placed");

		// The anchor floats HOVER_M over the ground under it.
		vector counterAt = shopAt + "3 0 4";
		counterAt[1] = GetGame().GetWorld().GetSurfaceY(counterAt[0], counterAt[2]) + HOVER_M;
		IEntity counter = LF_Utils.SpawnEntityPrefab(ANCHOR_PREFAB, counterAt, "0 0 0", false);
		ctx.NotNull(counter, "the counter anchor spawns");

		if (counter)
		{
			LF_ResidentAnchorComponent comp = LF_ResidentAnchorComponent.Cast(counter.FindComponent(LF_ResidentAnchorComponent));
			ctx.NotNull(comp, "the anchor carries the component");
			if (comp)
				comp.SetAnchorId("counter");
		}

		LF_ResidentsConfig config = new LF_ResidentsConfig();
		config.m_iSpawnRadiusM = 150;
		config.m_iDespawnRadiusM = 200;
		config.m_iMaxLive = 24;
		config.m_aResidents = {};
		config.m_aResidents.Insert(MakeResident("aaa_baker"));
		config.m_aResidents.Insert(MakeResident("bbb_clerk"));
		LF_ResidentsConfig.UseForTest(config);
		LF_ResidentTimetable.Load(config);
		LF_ResidentSpawner.ResetStatics();
		LF_NpcSpots.ResetStatics();

		LF_Test_ResidentSpawner_Players players = new LF_Test_ResidentSpawner_Players();
		players.m_aAt.Insert(shopAt + "30 0 0");
		if (!RedProof())
			LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);

		ctx.Equal(2, LF_ResidentSpawner.LiveCount(), "both residents due on the one anchor appear");
		IEntity baker = LF_ResidentSpawner.GetLive("aaa_baker");
		IEntity clerk = LF_ResidentSpawner.GetLive("bbb_clerk");
		ctx.NotNull(baker, "the first resident is live");
		ctx.NotNull(clerk, "the second resident is live");
		if (baker && clerk)
		{
			float apart = vector.DistanceXZ(baker.GetOrigin(), clerk.GetOrigin());
			ctx.True(apart >= APART_M, string.Format("the two residents stand %1 m apart (wanted %2 or more)", apart, APART_M));

			float bakerFloor;
			float clerkFloor;
			ctx.True(FloorUnder(baker, bakerFloor), "there is a floor under the first resident");
			ctx.True(FloorUnder(clerk, clerkFloor), "there is a floor under the second resident");
			float bakerGap = baker.GetOrigin()[1] - bakerFloor;
			float clerkGap = clerk.GetOrigin()[1] - clerkFloor;
			ctx.True(Math.AbsFloat(bakerGap) <= FEET_TOLERANCE_M, string.Format("the first resident's feet are %1 m from the floor (the anchor floats %2 m)", bakerGap, HOVER_M));
			ctx.True(Math.AbsFloat(clerkGap) <= FEET_TOLERANCE_M, string.Format("the second resident's feet are %1 m from the floor (the anchor floats %2 m)", clerkGap, HOVER_M));

			array<string> pairs = {};
			LF_NpcSpots.Overlaps(pairs);
			foreach (string pair : pairs)
			{
				ctx.True(!pair.Contains(baker.GetID().ToString()) && !pair.Contains(clerk.GetID().ToString()), "the runtime sweep reports a pair overlapping: " + pair);
			}
		}

		// A cold spot with no floor in reach keeps the anchor height instead of failing the spawn.
		LF_ResidentSpot sky = new LF_ResidentSpot();
		sky.m_vOrigin = counterAt + "0 500 0";
		float none;
		ctx.False(LF_ResidentSpawner.FloorAt(sky.m_vOrigin, none), "no floor 500 m over the anchor");
		LF_ResidentSpot kept = LF_ResidentSpawner.OnFloor(sky, "test");
		ctx.True(Math.AbsFloat(kept.m_vOrigin[1] - sky.m_vOrigin[1]) < 0.001, "a spot with no floor under it keeps its height");

		LF_ResidentSpawner.DespawnAll();
		LF_ResidentSpawner.ResetStatics();
		LF_ResidentTimetable.ResetStatics();
		LF_ResidentsConfig.Reload();
		LF_NpcSpots.ResetStatics();
		LF_Places.Unregister(SHOP_KIND, "spot_shop");
		LF_Places.Unregister(HOME_KIND, "spot_home");
		SCR_EntityHelper.DeleteEntityAndChildren(counter);
		SCR_EntityHelper.DeleteEntityAndChildren(shop);
		SCR_EntityHelper.DeleteEntityAndChildren(home);
	}

	//------------------------------------------------------------------------------------------------
	//! A resident who works the shop counter 07:00 to 18:00 and is at home on the bench otherwise.
	protected LF_ResidentDef MakeResident(string id)
	{
		LF_ResidentDef def = LF_ResidentDef.Make(id, HOME_KIND, SHOP_KIND);
		def.Add("06:00", "home", "bench");
		def.Add("07:00", "work", "counter", "wave");
		def.Add("18:00", "home", "bench");
		return def;
	}
}
