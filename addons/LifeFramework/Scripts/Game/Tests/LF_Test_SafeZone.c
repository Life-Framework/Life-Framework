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

		// The placed safe zone, whichever world places it: read it back, no town coordinates.
		LF_ZoneArea square = LF_Zones.FindFirstByRule(LF_ZoneRule.SAFE);
		ctx.True(square != null, "the world places a safe zone that registered itself");
		if (square)
		{
			ctx.True(square.m_fRadius > 0, "the placed safe zone has a radius");
			ctx.True(LF_Zones.IsSafe(square.m_vCenter), "the centre of the placed safe zone is safe");
			ctx.True(LF_Zones.IsSafe(square.m_vCenter + "0 6 0"), "a roof above the placed safe zone is safe");
		}

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

		// A zone registers and unregisters by name; a bad radius is refused.
		ctx.False(LF_Zones.RegisterCircle("test-zone-bad", LF_ZoneRule.SAFE, "0 0 0", 0), "a zero radius zone is refused");
		ctx.True(LF_Zones.RegisterCircle("test-zone", LF_ZoneRule.SAFE, "70 0 70", 5), "a test zone registers");
		ctx.True(LF_Zones.IsSafe("72 0 72"), "a point 2.8 m from the test zone is inside");
		LF_Zones.Unregister("test-zone");
		ctx.False(LF_Zones.IsSafe("72 0 72"), "an unregistered zone protects nothing");

		// The safe zone notice is a sign carrying the safe_zone id (the annex twins of the two town notices).
		ctx.True(CarriesSignId(world.FindEntityByName(LF_TestAnnex.SIGN_SAFE_ZONE_SPAWN), "safe_zone"), "the safe zone notice carries the safe_zone sign id");
		ctx.True(CarriesSignId(world.FindEntityByName(LF_TestAnnex.SIGN_SAFE_ZONE_EAST), "safe_zone"), "the second safe zone notice carries the safe_zone sign id");
	}

	//------------------------------------------------------------------------------------------------
	protected bool CarriesSignId(IEntity sign, string signId)
	{
		if (!sign)
			return false;

		LF_SignComponent component = LF_Component<LF_SignComponent>.Find(sign);
		if (!component)
			return false;

		return component.GetSignId() == signId;
	}
}
