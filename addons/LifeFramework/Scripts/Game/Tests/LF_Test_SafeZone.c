// red-proof: asserted False on the inside-the-zone hit and ran
// `tools\cli test --no-build --only LF_Test_SafeZone` (2026-11-08). The suite reported
// `[LFTEST]   - a player shot inside the zone takes no damage`. Restored the assertion to True.
// red-proof (#1893 fixtures): 2026-11-16 on GridWorld, before the fixture plaza, `--only LF_Test_SafeZone
// --world Worlds/GridWorld/GridWorld.ent` failed `- the world places a safe zone that registered itself` and
// `- civilian spawn point at <74,0,108> is inside a safe zone` (the grid gate spawn is outside any zone on
// purpose). The RedProof() hook skips the fixture zone and the plaza assertions go red.

// tier: WORLD
//! Safe zone damage rule, plus a fixture plaza (a safe circle with a CIVILIAN spawn point in it) the
//! test registers and removes itself; GridWorld places no town zones (docs/architecture/three-worlds.md).
class LF_Test_SafeZone : LF_Test
{
	static const ResourceName SPAWN_POINT_PREFAB = "{260C1BEFBF9AA86B}Prefabs/MP/Spawn/SpawnPoint_LIFE.et";
	static const string PLAZA_NAME = "sz-fixture-plaza";
	//! Far from the grid annex (x 18..114, z 18..114) and its gate spawn.
	static const float PLAZA_X = 480;
	static const float PLAZA_Z = 400;
	static const float PLAZA_RADIUS = 20;

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

		BaseWorld world = GetGame().GetWorld();
		ctx.True(world != null, "a world is loaded");
		if (!world)
			return;

		// The fixture plaza: a safe circle with a civilian arrival point inside it.
		vector plazaCentre = Vector(PLAZA_X, 0, PLAZA_Z);
		if (!RedProof())
			ctx.True(LF_Zones.RegisterCircle(PLAZA_NAME, LF_ZoneRule.SAFE, plazaCentre, PLAZA_RADIUS), "the fixture plaza registers");

		SCR_SpawnPoint arrival = SCR_SpawnPoint.Cast(LF_Utils.SpawnEntityPrefab(SPAWN_POINT_PREFAB, plazaCentre + "5 0 5"));
		ctx.True(arrival != null, "fixture: a civilian spawn point spawns on the plaza");
		if (arrival)
			arrival.SetFactionKey("CIVILIAN");

		LF_ZoneArea plaza = LF_Zones.Find(PLAZA_NAME);
		ctx.True(plaza != null, "the fixture safe zone is registered");
		ctx.True(LF_Zones.FindFirstByRule(LF_ZoneRule.SAFE) != null, "a safe zone is found by its rule");
		if (plaza)
		{
			ctx.True(plaza.m_fRadius > 0, "the plaza has a radius");
			ctx.True(LF_Zones.IsSafe(plaza.m_vCenter), "the centre of the plaza is safe");
			ctx.True(LF_Zones.IsSafe(plaza.m_vCenter + "0 6 0"), "a roof above the plaza is safe");
		}

		// A civilian arriving on the plaza lands in the zone.
		if (arrival)
		{
			ctx.EqualStr("CIVILIAN", arrival.GetFactionKey(), "the plaza spawn point is a CIVILIAN point");
			ctx.True(LF_Zones.IsSafe(arrival.GetOrigin()), string.Format("the civilian spawn point at %1 is inside the plaza", arrival.GetOrigin()));
			SCR_EntityHelper.DeleteEntityAndChildren(arrival);
		}

		LF_Zones.Unregister(PLAZA_NAME);
		ctx.False(LF_Zones.IsSafe(plazaCentre), "the removed plaza protects nothing");

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
