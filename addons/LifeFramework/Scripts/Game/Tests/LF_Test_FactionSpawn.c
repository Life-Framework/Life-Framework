// red-proof: remove the POLICE spawn point from
// Worlds/DebugWorld/DebugWorld_Layers/Gamemode.layer (or retag it CIVILIAN), then run
// `tools\cli test --tier all`; the POLICE count and resolve assertions go red.

// tier: WORLD
class LF_Test_FactionSpawn : LF_Test
{
	override string GetName()
	{
		return "faction-spawn-points";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(SCR_SpawnPoint.GetSpawnPointCountForFaction("CIVILIAN") >= 1, "DebugWorld has a CIVILIAN spawn point");
		ctx.True(SCR_SpawnPoint.GetSpawnPointCountForFaction("POLICE") >= 1, "DebugWorld has a POLICE spawn point");
		ctx.True(SCR_SpawnPoint.GetRandomSpawnPointForFaction("POLICE") != null, "a POLICE spawn point resolves");
	}
};