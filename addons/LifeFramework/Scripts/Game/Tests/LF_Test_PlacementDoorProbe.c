// tier: WORLD
// red-proof: 2026-11-17, with the LF_PlacementDoors.Find call removed from LF_PlacementRules.FindBlocker: `--only LF_Test_PlacementDoorProbe` failed `[LFTEST] FAIL placement/door-probe` (`a spot inside the leaf's swing is blocked by a door (expected 'a door', got '')`, the other side, the walk-up, the verdict and the reason). Restored, green.
//! placement/door-probe (#2445): the shared blocker search (LF_PlacementRules.FindBlocker, which the
//! single, aimed and composition placements all run) refuses a spot in a door leaf's swing or in the
//! walk-up in front of a ground-floor door, with the reason "a door", and passes a spot 4 m in front of
//! the door or 2.5 m beside it. A house door is spawned on open ground south of the DebugWorld town
//! (310, 100); the spots are measured from its own leaf box, never typed.
class LF_Test_PlacementDoorProbe : LF_Test
{
	protected static const ResourceName DOOR = "{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "placement/door-probe";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		BaseWorld world = GetGame().GetWorld();
		float groundY = world.GetSurfaceY(310, 100);
		IEntity door = LF_Utils.SpawnEntityPrefab(DOOR, Vector(310, groundY, 100));
		ctx.NotNull(door, "a house door spawned");
		if (!door)
			return;

		RunDoor(ctx, door);
		SCR_EntityHelper.DeleteEntityAndChildren(door);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDoor(LF_TestContext ctx, IEntity door)
	{
		LF_PlacementBox leaf = LF_PlacementBox.FromEntity(door);
		LF_PlacementBox swing = leaf.DoorZone(LF_PlacementDoors.SWING_CLEAR_M);
		ctx.NotNull(swing, "the door leaf stands upright, so it has a swing zone");
		if (!swing)
			return;

		float groundY = GetGame().GetWorld().GetSurfaceY(leaf.m_vCentre[0], leaf.m_vCentre[2]);
		ctx.True(leaf.m_vCentre[1] - leaf.m_vHalf[1] - groundY < LF_PlacementDoors.GROUND_FLOOR_M, "the door is a ground-floor door");

		// The leaf's thin side is the one the zone pushed out: that is the way a person walks up to it.
		vector front = "0 0 1";
		for (int i = 0; i < 3; i++)
		{
			if (swing.m_vHalf[i] > leaf.m_vHalf[i] + 0.001)
				front = swing.m_aAxes[i];
		}

		vector side = LF_PlacementGeometry.Cross(front, "0 1 0").Normalized();
		vector base = Vector(leaf.m_vCentre[0], groundY, leaf.m_vCentre[2]);

		ctx.EqualStr(LF_PlacementDoors.REASON, Probe(base + front * 0.5), "a spot inside the leaf's swing is blocked by a door");
		ctx.EqualStr(LF_PlacementDoors.REASON, Probe(base - front * 0.5), "a spot inside the swing on the other side is blocked too");
		ctx.EqualStr(LF_PlacementDoors.REASON, Probe(base + front * 2), "a spot in the walk-up in front of the door is blocked by a door");
		ctx.EqualStr("", Probe(base + front * 4), "a spot 4 m in front of the door is clear");
		ctx.EqualStr("", Probe(base + side * 2.5), "a spot 2.5 m beside the door is clear");

		// The same answer through the verdict the ghost colours by.
		LF_PlacementProbe probe = new LF_PlacementProbe();
		probe.m_bGround = true;
		probe.m_sBlocker = Probe(base + front * 0.5);
		LF_EPlacementVerdict verdict = LF_PlacementRules.Judge(probe, LF_PlacementService.MAX_STEP);
		ctx.True(verdict == LF_EPlacementVerdict.BLOCKED, "the doorway spot judges blocked");
		ctx.EqualStr("a door is in the way", LF_PlacementRules.Reason(verdict, probe, LF_PlacementService.MAX_STEP), "and the refusal names the door");

		// The thing being moved is excluded from its own check; the door itself, excluded, no longer answers.
		array<IEntity> exclude = {door};
		ctx.EqualStr("", Box(base + front * 0.5, exclude), "an excluded door does not block");
	}

	//------------------------------------------------------------------------------------------------
	//! FindBlocker for a 0.4 x 0.8 x 0.4 m prop standing at `at`.
	protected string Probe(vector at)
	{
		return Box(at, null);
	}

	//------------------------------------------------------------------------------------------------
	protected string Box(vector at, array<IEntity> exclude)
	{
		vector transform[4];
		Math3D.MatrixIdentity4(transform);
		transform[3] = at;
		return LF_PlacementRules.FindBlocker(transform, "-0.2 0.05 -0.2", "0.2 0.85 0.2", exclude);
	}
}
