// red-proof: asserted False on the inside-the-zone hit and ran
// `tools\cli test --no-build --only LF_Test_SafeZone` (2026-11-08). The suite reported
// `[LFTEST]   - a player shot inside the zone takes no damage`. Restored the assertion to True.

// tier: WORLD
class LF_Test_SafeZone : LF_Test
{
	override string GetName()
	{
		return "safezone/blocks";
	}

	override void Run(LF_TestContext ctx)
	{
		// The rule: player-on-player damage with either side inside is refused.
		ctx.True(LF_Zones.ShouldRefuse(1, 2, true, false), "a player shot inside the zone takes no damage");
		ctx.True(LF_Zones.ShouldRefuse(1, 2, false, true), "a shot fired out of the zone does no damage");
		ctx.False(LF_Zones.ShouldRefuse(1, 2, false, false), "a shot with both players outside does damage");
		ctx.False(LF_Zones.ShouldRefuse(1, 1, true, true), "self damage in the zone is not refused");
		ctx.False(LF_Zones.ShouldRefuse(0, 2, true, false), "NPC or world damage in the zone is not refused");
		ctx.False(LF_Zones.ShouldRefuse(1, 0, true, false), "a player hurting an NPC is not refused by the zone");

		// The flagship zone is placed on the square and covers the civilian spawn.
		BaseWorld world = GetGame().GetWorld();
		ctx.True(world != null, "DebugWorld is loaded");
		if (!world)
			return;

		IEntity square = world.FindEntityByName("LF_SafeZone_Square");
		ctx.True(square != null, "LF_SafeZone_Square is placed in DebugWorld");
		if (square)
		{
			LF_ZoneComponent zone = LF_ZoneComponent.Cast(square.FindComponent(LF_ZoneComponent));
			ctx.True(zone != null, "the square carries LF_ZoneComponent");
			if (zone)
				ctx.InRange(zone.GetRadius(), 25, 40, "the square zone radius covers the square");
		}

		ctx.True(LF_Zones.Find("LF_SafeZone_Square") != null, "the square zone registered itself");
		ctx.True(LF_Zones.IsSafe("126 0 246"), "the civilian spawn point is inside the zone");

		// Every civilian arrival lands in a zone, whichever spawn point the game picks.
		int civilianPoints;
		array<SCR_SpawnPoint> spawnPoints = SCR_SpawnPoint.GetSpawnPoints();
		if (spawnPoints)
		{
			foreach (SCR_SpawnPoint spawnPoint : spawnPoints)
			{
				if (!spawnPoint || spawnPoint.GetFactionKey() != "CIVILIAN")
					continue;

				civilianPoints++;
				ctx.True(LF_Zones.IsSafe(spawnPoint.GetOrigin()), string.Format("civilian spawn point at %1 is inside a safe zone", spawnPoint.GetOrigin()));
			}
		}

		ctx.True(civilianPoints > 0, "DebugWorld has civilian spawn points");
		ctx.True(LF_Zones.IsSafe("113 0 250"), "the fountain is inside the zone");
		ctx.True(LF_Zones.IsSafe("113 6 250"), "a roof above the fountain is inside the zone");
		ctx.False(LF_Zones.IsSafe("179 0 244.3"), "the general store is outside the zone");
		ctx.False(LF_Zones.IsSafe("229 0 244.3"), "the police station is outside the zone");

		// A zone registers and unregisters by name; a bad radius is refused.
		ctx.False(LF_Zones.RegisterCircle("test-zone-bad", LF_ZoneRule.SAFE, "0 0 0", 0), "a zero radius zone is refused");
		ctx.True(LF_Zones.RegisterCircle("test-zone", LF_ZoneRule.SAFE, "70 0 70", 5), "a test zone registers");
		ctx.True(LF_Zones.IsSafe("72 0 72"), "a point 2.8 m from the test zone is inside");
		LF_Zones.Unregister("test-zone");
		ctx.False(LF_Zones.IsSafe("72 0 72"), "an unregistered zone protects nothing");

		// Both safe zone notices stand at the square.
		ctx.True(world.FindEntityByName("Sign_SafeZone_Spawn") != null, "a safe zone notice stands at the spawn");
		ctx.True(world.FindEntityByName("Sign_SafeZone_East") != null, "a safe zone notice stands where the street enters the square");
	}
}
