// red-proof: 2026-11-16, cli test --no-build --only LF_Test_GridWorldBoot --world Worlds/TestLogic/TestLogic.ent:
//   [LFTEST]   - world/grid-boot is tier=world and needs DebugWorld; this run booted the minimal LOGIC world ...
//   (none of the fixtures exist there). With GridWorld sharing DebugWorld's terrain it failed
//   "there is ground under the gate spawn", which is why GridWorld has its own GridTerrain copy.
//------------------------------------------------------------------------------------------------
//! world/grid-boot: the debug grid world (Worlds/GridWorld, docs/architecture/three-worlds.md) boots
//! alone with the annex copied in: the annex fence posts either side of the north gate, the test runner,
//! one fixture per legacy annex layer by name, and ground under the gate spawn. In TestLogic none of
//! these exist; in DebugWorld all but the gate spawn do until W6 strips the annex from it.
// tier: WORLD
class LF_Test_GridWorldBoot : LF_Test
{
	override string GetName()
	{
		return "world/grid-boot";
	}

	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		ctx.True(world != null, "a world is loaded");
		if (!world)
			return;

		// One fixture per legacy annex layer (Annex, Crimes, HousesKeys, Jobs, NPCs, PRFeatures, Police,
		// Tests, WhitelistChat), found by the name the layer gives it.
		array<string> fixtures = {"Annex_Bank_Vault", "CrimeScene", "HouseKeysStation", "JobCenter", "NPCStation",
			"PRFeatureCrate", "PoliceStation", "LF_TestRunner", "WhitelistStation"};
		foreach (string name : fixtures)
			ctx.NotNull(world.FindEntityByName(name), "the annex fixture " + name + " is in the world");

		// The annex fence: the two posts either side of the north gate gap.
		ctx.True(HasFenceAt(world, Vector(69, 0, 112)), "the annex fence stands west of the north gate (69, 112)");
		ctx.True(HasFenceAt(world, Vector(79, 0, 112)), "the annex fence stands east of the north gate (79, 112)");

		// The gate spawn just inside the north gate gap (x 69..79 at z 112), and ground under it: a
		// straight-down trace hits the world.
		IEntity spawn = world.FindEntityByName("Spawn_Grid_Gate");
		vector at = Vector(74, 0, 108);
		if (spawn)
			at = spawn.GetOrigin();
		// Only GridWorld has it; the suite still boots DebugWorld, which has the annex but not this spawn.
		string worldFile = GetGame().GetWorldFile();
		if (worldFile.Contains("GridWorld"))
			ctx.True(spawn != null, "the gate spawn point Spawn_Grid_Gate is in GridWorld");

		TraceParam trace = new TraceParam();
		trace.Start = at + Vector(0, 5, 0);
		trace.End = at - Vector(0, 5, 0);
		trace.Flags = TraceFlags.WORLD;
		float fraction = world.TraceMove(trace, null);
		LF_Debug.Info("GridWorld", string.Format("ground trace under the gate spawn %1: fraction %2", at, fraction));
		ctx.True(fraction < 1, "there is ground under the gate spawn");
	}

	// A NetFence entity within half a metre of `pos`.
	protected bool HasFenceAt(BaseWorld world, vector pos)
	{
		m_bFoundFence = false;
		world.QueryEntitiesBySphere(pos, 0.5, FenceQuery, null, EQueryEntitiesFlags.ALL);
		return m_bFoundFence;
	}

	protected bool m_bFoundFence;

	protected bool FenceQuery(IEntity e)
	{
		EntityPrefabData data = e.GetPrefabData();
		if (!data)
			return true;
		string prefab = data.GetPrefabName();
		if (prefab.Contains("NetFence"))
		{
			m_bFoundFence = true;
			return false;
		}
		return true;
	}
}
