// red-proof: revert ResolveSpawnPointForLocation to return null (or move the
// POLICE spawn points so neither is near the outpost), then run
// `tools\cli test --tier all`; the location resolution assertions go red.

// tier: WORLD
class EL_Test_SpawnLocation : EL_Test
{
	override string GetName()
	{
		return "spawn-location";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_SpawnLocation town = EL_SpawnLocations.GetByKey("town");
		ctx.True(town != null, "location registry has 'town'");
		ctx.True(town && town.m_eFaction == EL_Faction.CIVILIAN, "'town' belongs to CIVILIAN");
		ctx.True(EL_SpawnLocations.GetByKey("nonexistent") == null, "unknown key resolves to null");
		ctx.True(EL_SpawnLocations.GetForFaction(EL_Faction.POLICE).Count() == 2, "POLICE has two locations");
		ctx.True(EL_SpawnLocations.GetForFaction(EL_Faction.CIVILIAN).Count() == 2, "CIVILIAN has two locations");

		EL_SpawnLogic.SetSelectedLocation(4242, "outpost");
		SCR_SpawnPoint resolved = EL_SpawnLogic.ResolveSpawnPointForLocation(4242, EL_Faction.POLICE);
		ctx.True(resolved != null, "a spawn point resolves for the 'outpost' choice");

		if (resolved)
		{
			vector origin = resolved.GetOrigin();
			EL_SpawnLocation outpost = EL_SpawnLocations.GetByKey("outpost");
			float distanceToOutpost = vector.DistanceSq(origin, outpost.m_vPosition);
			float distanceToStation = vector.DistanceSq(origin, EL_SpawnLocations.GetByKey("station").m_vPosition);
			ctx.True(distanceToOutpost < distanceToStation, "resolved point is the one nearest the chosen location");
		}

		EL_SpawnLogic.SetSelectedLocation(4242, "bogus");
		ctx.True(EL_SpawnLogic.ResolveSpawnPointForLocation(4242, EL_Faction.POLICE) == null, "unknown location key is rejected, falls back");
	}
};
