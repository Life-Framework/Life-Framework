// red-proof: remove the POLICE spawn point from
// Worlds/DebugWorld/DebugWorld_Layers/Gamemode.layer (or retag it CIVILIAN), then run
// `tools\cli test --tier all`; the POLICE count and resolve assertions go red.
// red-proof (#1893 fixtures): 2026-11-16 on GridWorld (one CIVILIAN gate spawn, no POLICE), before the
// fixture spawn points, `--only LF_Test_FactionSpawn --world Worlds/GridWorld/GridWorld.ent` failed
// `- DebugWorld has a POLICE spawn point` and `- a POLICE spawn point resolves`. The RedProof() hook
// skips the fixture spawn and the same assertions go red.

// tier: WORLD
//! Faction spawn points resolve by faction key. The test spawns one CIVILIAN and one POLICE spawn
//! point from the core spawn prefab and deletes them, so it needs no town spawns.
class LF_Test_FactionSpawn : LF_Test
{
	static const ResourceName SPAWN_POINT_PREFAB = "{260C1BEFBF9AA86B}Prefabs/MP/Spawn/SpawnPoint_LIFE.et";

	override string GetName()
	{
		return "faction-spawn-points";
	}

	override void Run(LF_TestContext ctx)
	{
		int policeBefore = SCR_SpawnPoint.GetSpawnPointCountForFaction("POLICE");
		SCR_SpawnPoint civilian;
		SCR_SpawnPoint police;
		if (!RedProof())
		{
			civilian = SpawnPoint("CIVILIAN", "400 0 480");
			police = SpawnPoint("POLICE", "410 0 480");
		}

		ctx.True(SCR_SpawnPoint.GetSpawnPointCountForFaction("CIVILIAN") >= 1, "the world has a CIVILIAN spawn point");
		ctx.True(SCR_SpawnPoint.GetSpawnPointCountForFaction("POLICE") >= 1, "the world has a POLICE spawn point");
		ctx.Equal(policeBefore + 1, SCR_SpawnPoint.GetSpawnPointCountForFaction("POLICE"), "the fixture adds one POLICE spawn point");
		SCR_SpawnPoint resolved = SCR_SpawnPoint.GetRandomSpawnPointForFaction("POLICE");
		ctx.True(resolved != null, "a POLICE spawn point resolves");
		if (resolved)
			ctx.EqualStr("POLICE", resolved.GetFactionKey(), "the resolved point is a POLICE point");

		if (civilian)
			SCR_EntityHelper.DeleteEntityAndChildren(civilian);

		if (police)
			SCR_EntityHelper.DeleteEntityAndChildren(police);

		ctx.Equal(policeBefore, SCR_SpawnPoint.GetSpawnPointCountForFaction("POLICE"), "the fixture POLICE point is gone after teardown");
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_SpawnPoint SpawnPoint(string factionKey, vector position)
	{
		SCR_SpawnPoint point = SCR_SpawnPoint.Cast(LF_Utils.SpawnEntityPrefab(SPAWN_POINT_PREFAB, position));
		if (point)
			point.SetFactionKey(factionKey);

		return point;
	}
}
