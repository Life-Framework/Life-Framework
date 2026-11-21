// red-proof: 2026-11-21 (#3912): with the floor snap out of LF_ResidentWalk.Move the walk ended on the anchor 0.4 m up and the line "the walk ends on the floor" failed.
// red-proof: 2026-11-18, before LF_ResidentWalk existed the test did not compile; with the act skipped
//   (RedProof() on, `cli test --no-build --red LF_Test_ResidentWalk`) the timetable never moves the
//   resident, so `a resident whose line moves with a player near walks` and the arrival checks fail.
//   The run that went red and then green on the implementation is recorded in the PR.
//! residents/walk (#2909): a live resident whose timetable line moves on with a player within 50 m gets
//! a move order and walks to the next anchor (the same character, never despawned) and arrives inside
//! its timeout, where the activity starts; with nobody near they are placed on the new anchor directly;
//! a walk that never arrives is moved to the anchor after its timeout and logged; at most the cap walk
//! at once. GridWorld has no AIWorld, so a stand-in mover plays the AI (walks at 1.4 m/s, or stands
//! still for the stuck case); the vanilla AI group and waypoint are a manual step (docs/features/residents.md).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentWalk : LF_Test
{
	protected static const string SHOP_KIND = "test_residents_walk_shop";
	protected static const string HOME_KIND = "test_residents_walk_home";
	protected static const ResourceName PLACE_PREFAB = "{25C8B889A777399D}Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et";
	protected static const ResourceName ANCHOR_PREFAB = "{CCED18E36D627B1D}Prefabs/Systems/Residents/LF_ResidentAnchor.et";
	protected static const int WORK_MINUTE = 450;	// 07:30
	protected static const int HOME_MINUTE = 1100;	// 18:20
	protected static const float HOVER_M = 0.4;
	protected static const float FEET_TOLERANCE_M = 0.05;

	//! The floor under the home anchor, measured before the anchor is lifted (#3912).
	protected float m_fBenchFloor;
	protected bool m_bBenchFloor;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/walk";
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAnchor(LF_TestContext ctx, string id, vector at)
	{
		IEntity anchor = LF_Utils.SpawnEntityPrefab(ANCHOR_PREFAB, at, "0 0 0", false);
		ctx.NotNull(anchor, "anchor '" + id + "' spawns");
		if (!anchor)
			return null;

		LF_ResidentAnchorComponent comp = LF_ResidentAnchorComponent.Cast(anchor.FindComponent(LF_ResidentAnchorComponent));
		ctx.NotNull(comp, "anchor '" + id + "' carries the component");
		if (comp)
			comp.SetAnchorId(id);

		return anchor;
	}

	//------------------------------------------------------------------------------------------------
	protected float DistXZ(vector a, vector b)
	{
		return vector.DistanceXZ(a, b);
	}

	//------------------------------------------------------------------------------------------------
	//! A resident who works the shop counter 07:00 to 18:00 and sits on the home bench otherwise.
	protected LF_ResidentDef MakeResident(string id)
	{
		LF_ResidentDef def = LF_ResidentDef.Make(id, HOME_KIND, SHOP_KIND);
		def.Add("06:00", "home", "bench");
		def.Add("07:00", "work", "counter", "wave");
		def.Add("18:00", "home", "bench");
		return def;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The rule with no world: walk when seen and near enough, place when only the new spot is seen.
		ctx.True(LF_ResidentWalk.Decide(20, 40, 30, 0, 4, 200, true) == LF_EResidentMove.WALKING, "a player 20 m away and a spot 40 m off: walk");
		ctx.True(LF_ResidentWalk.Decide(80, 40, 30, 0, 4, 200, true) == LF_EResidentMove.PLACED, "a player 80 m away is not near enough to watch: place");
		ctx.True(LF_ResidentWalk.Decide(20, 500, 30, 0, 4, 200, true) == LF_EResidentMove.PLACED, "a spot 500 m off is not walked to: place");
		ctx.True(LF_ResidentWalk.Decide(20, 40, 30, 4, 4, 200, true) == LF_EResidentMove.PLACED, "the walking cap reached: place");
		ctx.True(LF_ResidentWalk.Decide(20, 40, 30, 0, 4, 200, false) == LF_EResidentMove.PLACED, "no way to move the body: place");
		ctx.True(LF_ResidentWalk.Decide(80, 40, 900, 0, 4, 200, true) == LF_EResidentMove.NONE, "nobody sees either end: despawn");

		IEntity vault = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(vault, "the annex vault is placed, to spawn the stand-ins near");
		if (!vault)
			return;

		// Two anchors 40 m apart: the shop counter and the home bench.
		vector shopAt = vault.GetOrigin() + "0 -20 0";
		vector homeAt = shopAt + "40 0 0";
		IEntity shop = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, shopAt, "0 0 0", false);
		IEntity home = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, homeAt, "0 0 0", false);
		ctx.True(shop && home, "the test shop and home spawn");
		if (!shop || !home)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(shop);
			SCR_EntityHelper.DeleteEntityAndChildren(home);
			return;
		}

		ctx.True(LF_Places.Register(SHOP_KIND, "walk_shop", shop), "the shop is placed");
		ctx.True(LF_Places.Register(HOME_KIND, "walk_home", home), "the home is placed");
		vector counterAt = shopAt + "0 0 0";
		vector benchAt = homeAt + "0 0 0";
		IEntity counter = SpawnAnchor(ctx, "counter", counterAt);
		IEntity bench = SpawnAnchor(ctx, "bench", benchAt);
		ctx.EqualFloat(40, DistXZ(counterAt, benchAt), 1, "the two anchors are 40 m apart");

		// The home anchor hovers HOVER_M over its floor: the walk must end on the floor (#3912).
		float ground = GetGame().GetWorld().GetSurfaceY(benchAt[0], benchAt[2]);
		if (bench)
			bench.SetOrigin(Vector(benchAt[0], ground + HOVER_M, benchAt[2]));

		m_bBenchFloor = LF_ResidentSpawner.FloorAt(Vector(benchAt[0], ground + HOVER_M, benchAt[2]), m_fBenchFloor);
		ctx.True(m_bBenchFloor, "there is a floor under the home anchor");

		LF_ResidentsConfig config = new LF_ResidentsConfig();
		config.m_iSpawnRadiusM = 150;
		config.m_iDespawnRadiusM = 200;
		config.m_iMaxLive = 24;
		config.m_iMaxWalking = 4;
		config.m_aResidents = {};
		config.m_aResidents.Insert(MakeResident("aaa_baker"));
		LF_ResidentsConfig.UseForTest(config);
		LF_ResidentTimetable.Load(config);
		LF_ResidentSpawner.ResetStatics();
		LF_ResidentWalk.ResetStatics();

		LF_Test_ResidentWalk_Mover mover = new LF_Test_ResidentWalk_Mover();
		LF_ResidentWalk.UseMoverForTest(mover);
		LF_Test_ResidentWalk_Players players = new LF_Test_ResidentWalk_Players();

		// Due at the counter with a player beside it: the resident appears there.
		players.m_aAt.Insert(counterAt + "10 0 0");
		LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);
		ctx.Equal(1, LF_ResidentSpawner.LiveCount(), "the resident appears on the counter");
		IEntity baker = LF_ResidentSpawner.GetLive("aaa_baker");
		ctx.NotNull(baker, "the live resident is an entity");
		if (baker)
			RunWalk(ctx, baker, players, mover, counterAt, benchAt);

		RunPlaced(ctx, players, mover, counterAt, benchAt);
		RunStuck(ctx, players, mover, counterAt, benchAt);
		RunCap(ctx, config, players, mover, counterAt, benchAt);

		LF_ResidentSpawner.DespawnAll();
		LF_ResidentSpawner.ResetStatics();
		LF_ResidentWalk.ResetStatics();
		LF_ResidentTimetable.ResetStatics();
		LF_ResidentsConfig.Reload();
		LF_Places.Unregister(SHOP_KIND, "walk_shop");
		LF_Places.Unregister(HOME_KIND, "walk_home");
		SCR_EntityHelper.DeleteEntityAndChildren(counter);
		SCR_EntityHelper.DeleteEntityAndChildren(bench);
		SCR_EntityHelper.DeleteEntityAndChildren(shop);
		SCR_EntityHelper.DeleteEntityAndChildren(home);
	}

	//------------------------------------------------------------------------------------------------
	//! The line moves home with a player 10 m away: the same character walks and arrives in time.
	protected void RunWalk(LF_TestContext ctx, IEntity baker, LF_Test_ResidentWalk_Players players, LF_Test_ResidentWalk_Mover mover, vector counterAt, vector benchAt)
	{
		EntityID first = baker.GetID();
		if (!RedProof())
			LF_ResidentSpawner.SweepAt(HOME_MINUTE, players);

		ctx.True(LF_ResidentWalk.IsWalking("aaa_baker"), "a resident whose line moves with a player near walks");
		ctx.Equal(1, mover.m_iBegun, "the resident got one move order");
		ctx.EqualFloat(0, DistXZ(mover.m_vTo, benchAt), 1, "the move order points at the new anchor");
		if (m_bBenchFloor)
			ctx.True(Math.AbsFloat(mover.m_vTo[1] - m_fBenchFloor) <= FEET_TOLERANCE_M, string.Format("the walk ends on the floor, not on the anchor %1 m up (destination %2 m over the floor)", HOVER_M, mover.m_vTo[1] - m_fBenchFloor));

		ctx.Equal(0, LF_ResidentSpawner.DespawnedTotal(), "a walking resident is not despawned");
		ctx.Equal(1, LF_ResidentSpawner.LiveCount(), "the walker stays live");
		ctx.EqualFloat(0, DistXZ(baker.GetOrigin(), counterAt), 1, "the walk starts where the resident stood");

		float allowed = LF_ResidentWalk.TimeoutOf("aaa_baker");
		ctx.True(allowed > 40, "the timeout covers the 40 m walk");

		float seconds;
		while (LF_ResidentWalk.IsWalking("aaa_baker") && seconds < allowed + 5)
		{
			LF_ResidentWalk.Step(1);
			seconds += 1;
		}

		ctx.True(seconds < allowed, "the resident arrives inside the timeout");
		ctx.Equal(1, LF_ResidentWalk.ArrivedTotal(), "one arrival counted");
		ctx.Equal(0, LF_ResidentWalk.TimedOutTotal(), "no timeout");
		ctx.EqualFloat(0, DistXZ(baker.GetOrigin(), benchAt), LF_ResidentWalk.ARRIVE_M, "the walker stands on the home anchor");
		IEntity again = LF_ResidentSpawner.GetLive("aaa_baker");
		ctx.True(again && again.GetID() == first, "the same character arrives (nobody popped)");
		ctx.Equal(1, mover.m_iEnded, "the move order was ended on arrival");
		ctx.Equal(0, LF_ResidentWalk.WalkingCount(), "nobody walks after arrival");

		// A later sweep on the same line leaves them be.
		LF_ResidentSpawner.SweepAt(HOME_MINUTE, players);
		ctx.Equal(1, mover.m_iBegun, "a sweep on the same line orders no new walk");
	}

	//------------------------------------------------------------------------------------------------
	//! Back to work with the player 70 m off the resident: nobody walks, the body is placed directly.
	protected void RunPlaced(LF_TestContext ctx, LF_Test_ResidentWalk_Players players, LF_Test_ResidentWalk_Mover mover, vector counterAt, vector benchAt)
	{
		players.m_aAt.Clear();
		players.m_aAt.Insert(benchAt + "0 0 70");
		int begun = mover.m_iBegun;
		int placed = LF_ResidentWalk.PlacedTotal();
		LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);
		ctx.False(LF_ResidentWalk.IsWalking("aaa_baker"), "with nobody near the resident does not walk");
		ctx.Equal(begun, mover.m_iBegun, "no move order with nobody near");
		ctx.Equal(placed + 1, LF_ResidentWalk.PlacedTotal(), "the resident is placed directly");
		IEntity baker = LF_ResidentSpawner.GetLive("aaa_baker");
		ctx.NotNull(baker, "the placed resident is still live");
		if (baker)
			ctx.EqualFloat(0, DistXZ(baker.GetOrigin(), counterAt), 1, "the placed resident stands on the work anchor at once");
	}

	//------------------------------------------------------------------------------------------------
	//! A mover that never gets there: after the timeout the resident is moved to the anchor and it is logged.
	protected void RunStuck(LF_TestContext ctx, LF_Test_ResidentWalk_Players players, LF_Test_ResidentWalk_Mover mover, vector counterAt, vector benchAt)
	{
		players.m_aAt.Clear();
		players.m_aAt.Insert(counterAt + "10 0 0");
		mover.m_bStuck = true;
		LF_ResidentSpawner.SweepAt(HOME_MINUTE, players);
		ctx.True(LF_ResidentWalk.IsWalking("aaa_baker"), "the stuck resident starts a walk");
		float allowed = LF_ResidentWalk.TimeoutOf("aaa_baker");
		LF_ResidentWalk.Step(allowed - 2);
		ctx.True(LF_ResidentWalk.IsWalking("aaa_baker"), "just before the timeout the walk goes on");
		ctx.Equal(0, LF_ResidentWalk.TimedOutTotal(), "no timeout yet");
		LF_ResidentWalk.Step(5);
		ctx.False(LF_ResidentWalk.IsWalking("aaa_baker"), "after the timeout the walk is over");
		ctx.Equal(1, LF_ResidentWalk.TimedOutTotal(), "the stuck walk timed out once (and warned)");
		IEntity baker = LF_ResidentSpawner.GetLive("aaa_baker");
		ctx.NotNull(baker, "the resident is still live");
		if (baker)
			ctx.EqualFloat(0, DistXZ(baker.GetOrigin(), benchAt), 1, "the stuck resident was moved onto the anchor");

		mover.m_bStuck = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Cap of one walker: of two residents with a line change, one walks and the other is placed.
	protected void RunCap(LF_TestContext ctx, LF_ResidentsConfig config, LF_Test_ResidentWalk_Players players, LF_Test_ResidentWalk_Mover mover, vector counterAt, vector benchAt)
	{
		LF_ResidentSpawner.DespawnAll();
		LF_ResidentWalk.ResetStatics();
		LF_ResidentWalk.UseMoverForTest(mover);
		config.m_iMaxWalking = 1;
		config.m_aResidents.Insert(MakeResident("bbb_clerk"));
		LF_ResidentTimetable.Load(config);
		LF_ResidentSpawner.ResetStatics();
		players.m_aAt.Clear();
		players.m_aAt.Insert(counterAt + "10 0 0");
		LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);
		ctx.Equal(2, LF_ResidentSpawner.LiveCount(), "two residents work the counter");

		LF_ResidentSpawner.SweepAt(HOME_MINUTE, players);
		ctx.Equal(1, LF_ResidentWalk.WalkingCount(), "the cap of one walker holds");
		ctx.Equal(1, LF_ResidentWalk.PlacedTotal(), "the second resident is placed instead");
		ctx.True(LF_ResidentWalk.IsWalking("aaa_baker"), "the first resident by id walks");
		ctx.Equal(2, LF_ResidentSpawner.LiveCount(), "both stay live");
		LF_ResidentWalk.Cancel("aaa_baker");
	}
}

//------------------------------------------------------------------------------------------------
//! Stand-in players for the walk test: wherever the test puts them.
class LF_Test_ResidentWalk_Players : LF_ResidentPlayers
{
	ref array<vector> m_aAt = {};

	override int Positions(notnull array<vector> into)
	{
		foreach (vector at : m_aAt)
			into.Insert(at);

		return m_aAt.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! Stands in for the vanilla AI in GridWorld (no AIWorld): records the move order and walks the body at
//! 1.4 m/s, or stands still when stuck.
class LF_Test_ResidentWalk_Mover : LF_ResidentMover
{
	int m_iBegun;
	int m_iEnded;
	vector m_vTo;
	bool m_bStuck;

	override bool Available()
	{
		return true;
	}

	override bool Begin(notnull IEntity body, vector to, notnull LF_ResidentWalkState walk)
	{
		m_iBegun++;
		m_vTo = to;
		return true;
	}

	override void Tick(IEntity body, notnull LF_ResidentWalkState walk, float seconds)
	{
		if (!body || m_bStuck)
			return;

		vector from = body.GetOrigin();
		vector flat = walk.m_Spot.m_vOrigin - from;
		flat[1] = 0;
		float left = flat.Length();
		if (left <= 0.01)
			return;

		float step = Math.Min(1.4 * seconds, left);	// lf-float: test walking pace, metres
		flat.Normalize();
		body.SetOrigin(from + flat * step);
	}

	override void End(IEntity body, notnull LF_ResidentWalkState walk)
	{
		m_iEnded++;
	}
}
