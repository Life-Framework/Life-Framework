// red-proof: 2026-11-18, before LF_ResidentSpawner existed the test did not compile; with the spawn
//   act skipped (RedProof() on, `cli test --no-build --red LF_Test_ResidentSpawner`) the first sweep
//   spawns nothing and `a resident due at the counter appears` fails. The run that went red and then
//   green on the implementation is recorded in the PR.
//! residents/spawner: a resident due at an anchor appears when a player is within the spawn radius and
//! leaves past the despawn radius (#2858). One sweep spawns one character tagged with the resident id on
//! the counter anchor; a second spawns no duplicate; a player between the two radii keeps it (hysteresis);
//! when the timetable moves the resident home the work spot despawns and home spawns only if a player is
//! near it; a live cap of 1 with two residents due spawns the first by id and logs the other once.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentSpawner : LF_Test
{
	protected static const string SHOP_KIND = "test_residents_spawn_shop";
	protected static const string HOME_KIND = "test_residents_spawn_home";
	protected static const ResourceName PLACE_PREFAB = "{25C8B889A777399D}Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et";
	protected static const ResourceName ANCHOR_PREFAB = "{CCED18E36D627B1D}Prefabs/Systems/Residents/LF_ResidentAnchor.et";
	protected static const int WORK_MINUTE = 450;	// 07:30
	protected static const int HOME_MINUTE = 1100;	// 18:20

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/spawner";
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
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return Math.Sqrt(dx * dx + dz * dz);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity vault = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(vault, "the annex vault is placed, to spawn the stand-ins near");
		if (!vault)
			return;

		vector shopAt = vault.GetOrigin() + "0 -20 0";
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

		ctx.True(LF_Places.Register(SHOP_KIND, "spawn_shop", shop), "the shop is placed");
		ctx.True(LF_Places.Register(HOME_KIND, "spawn_home", home), "the home is placed");
		vector counterAt = shopAt + "3 0 4";
		vector benchAt = homeAt + "2 0 2";
		IEntity counter = SpawnAnchor(ctx, "counter", counterAt);
		IEntity bench = SpawnAnchor(ctx, "bench", benchAt);

		LF_ResidentsConfig config = new LF_ResidentsConfig();
		config.m_iSpawnRadiusM = 150;
		config.m_iDespawnRadiusM = 200;
		config.m_iMaxLive = 24;
		config.m_aResidents = {};
		config.m_aResidents.Insert(MakeResident("aaa_baker"));
		LF_ResidentsConfig.UseForTest(config);
		LF_ResidentTimetable.Load(config);
		LF_ResidentSpawner.ResetStatics();

		LF_Test_ResidentSpawner_Players players = new LF_Test_ResidentSpawner_Players();

		// Due at the counter, a player in range: exactly one character on the anchor, tagged.
		players.m_aAt.Insert(shopAt + "30 0 0");
		if (!RedProof())
			LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);

		ctx.Equal(1, LF_ResidentSpawner.LiveCount(), "a resident due at the counter appears");
		IEntity baker = LF_ResidentSpawner.GetLive("aaa_baker");
		ctx.NotNull(baker, "the live resident is an entity");
		if (baker)
		{
			ctx.EqualFloat(0, DistXZ(baker.GetOrigin(), counterAt), 0.5, "the resident stands on the counter anchor");
			ctx.EqualStr("aaa_baker", LF_ResidentSpawner.ResidentIdOf(baker), "the character is tagged with the resident id");
		}

		EntityID first;
		if (baker)
			first = baker.GetID();

		LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);
		ctx.Equal(1, LF_ResidentSpawner.LiveCount(), "a second sweep leaves one resident live");
		ctx.Equal(1, LF_ResidentSpawner.SpawnedTotal(), "a second sweep spawns no duplicate");
		IEntity again = LF_ResidentSpawner.GetLive("aaa_baker");
		if (again)
			ctx.True(again.GetID() == first, "the same character stays");

		// Hysteresis: between the spawn and the despawn radius the resident stays; beyond it they go.
		players.m_aAt.Clear();
		players.m_aAt.Insert(counterAt + "180 0 0");
		LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);
		ctx.Equal(1, LF_ResidentSpawner.LiveCount(), "a player between the radii keeps the resident");

		players.m_aAt.Clear();
		players.m_aAt.Insert(counterAt + "250 0 0");
		LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);
		ctx.Equal(0, LF_ResidentSpawner.LiveCount(), "a player beyond the despawn radius removes the resident");
		ctx.True(LF_ResidentSpawner.GetLive("aaa_baker") == null, "the despawned resident is gone");
		ctx.Equal(1, LF_ResidentSpawner.DespawnedTotal(), "one despawn counted");

		// Out of spawn range (180 m) a due resident does not appear; in range again they do.
		players.m_aAt.Clear();
		players.m_aAt.Insert(counterAt + "180 0 0");
		LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);
		ctx.Equal(0, LF_ResidentSpawner.LiveCount(), "a player outside the spawn radius spawns nobody");

		players.m_aAt.Clear();
		players.m_aAt.Insert(counterAt + "40 0 0");
		LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);
		ctx.Equal(1, LF_ResidentSpawner.LiveCount(), "back in range the resident appears again");

		// The timetable moves them home: the work spot despawns and, with the player at the shop only, home stays empty.
		LF_ResidentSpawner.SweepAt(HOME_MINUTE, players);
		ctx.Equal(0, LF_ResidentSpawner.LiveCount(), "at the home line the work spot despawns and home has no player near");

		players.m_aAt.Clear();
		players.m_aAt.Insert(benchAt + "20 0 0");
		LF_ResidentSpawner.SweepAt(HOME_MINUTE, players);
		ctx.Equal(1, LF_ResidentSpawner.LiveCount(), "with a player near home the resident appears there");
		IEntity athome = LF_ResidentSpawner.GetLive("aaa_baker");
		if (athome)
			ctx.EqualFloat(0, DistXZ(athome.GetOrigin(), benchAt), 0.5, "the resident stands on the home bench");

		// Live cap: two residents due, room for one.
		LF_ResidentSpawner.DespawnAll();
		config.m_iMaxLive = 1;
		config.m_aResidents.Insert(MakeResident("bbb_clerk"));
		LF_ResidentTimetable.Load(config);
		players.m_aAt.Clear();
		players.m_aAt.Insert(shopAt + "30 0 0");
		int warnsBefore = LF_ResidentSpawner.CapWarnCount();
		LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);
		ctx.Equal(1, LF_ResidentSpawner.LiveCount(), "the cap holds the live residents at one");
		ctx.True(LF_ResidentSpawner.GetLive("aaa_baker") != null, "the first resident by id spawns");
		ctx.True(LF_ResidentSpawner.GetLive("bbb_clerk") == null, "the second waits");
		ctx.Equal(warnsBefore + 1, LF_ResidentSpawner.CapWarnCount(), "the capped resident is logged");
		LF_ResidentSpawner.SweepAt(WORK_MINUTE, players);
		ctx.Equal(warnsBefore + 1, LF_ResidentSpawner.CapWarnCount(), "the capped resident is logged once");

		LF_ResidentSpawner.DespawnAll();
		LF_ResidentSpawner.ResetStatics();
		LF_ResidentTimetable.ResetStatics();
		LF_ResidentsConfig.Reload();
		LF_Places.Unregister(SHOP_KIND, "spawn_shop");
		LF_Places.Unregister(HOME_KIND, "spawn_home");
		SCR_EntityHelper.DeleteEntityAndChildren(counter);
		SCR_EntityHelper.DeleteEntityAndChildren(bench);
		SCR_EntityHelper.DeleteEntityAndChildren(shop);
		SCR_EntityHelper.DeleteEntityAndChildren(home);
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
}

//------------------------------------------------------------------------------------------------
//! Stand-in players for the spawner test: wherever the test puts them.
class LF_Test_ResidentSpawner_Players : LF_ResidentPlayers
{
	ref array<vector> m_aAt = {};

	override int Positions(notnull array<vector> into)
	{
		foreach (vector at : m_aAt)
			into.Insert(at);

		return m_aAt.Count();
	}
}
