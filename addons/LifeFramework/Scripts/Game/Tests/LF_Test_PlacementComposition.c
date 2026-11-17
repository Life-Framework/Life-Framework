// red-proof: observed 2026-11-16 (#1940) with `toolscli test --no-build --red LF_Test_PlacementComposition`: `RED-OK placement/composition (fails with the hook on, 17 assertions)` (RedProof() skips the first Confirm, so nothing is placed); green without the hook (`--only`, 1 passed).
// tier: WORLD
//! #1940 acceptance: composition placement (LF_PlacementComposition.c). A three-cone set (the same
//! shape as a roadblock's cones and barriers) is placed by an officer on the main street's south
//! verge: one plan with one verdict, one confirm that spawns every piece with each piece on the
//! ground, and one pick-up that removes all of them. A second set on top of the first, a malformed
//! proposal and a far-away picker are refused and change nothing. Further into the annex, a set with one piece half on a table (a rise past the step) is TOO_STEEP and places nothing.
class LF_Test_PlacementComposition : LF_Test
{
	static const ResourceName CONE = "{AEF9A26C2F0334B3}Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_base.et";
	protected static const ResourceName TABLE = "{622F9989DDC4865B}PrefabLibrary/Props/Civilian/Recreation/TableRecreation_01/TableRecreation_01.et";
	//! Just inside the annex's north gate, on open GridWorld ground (LF_Test_GridWorldBoot's gate spawn); the
	//! fixture is read by name and falls back to its known spot when the test runs in a world without it.
	protected static const vector GATE = "74 0 108";
	//! Further into the annex from the gate, for the steep spot.
	protected static const vector INSIDE = "-8 0 -8";

	protected vector GateGround()
	{
		IEntity spawn = GetGame().GetWorld().FindEntityByName("Spawn_Grid_Gate");
		if (spawn)
			return spawn.GetOrigin();

		return GATE;
	}

	override string GetName()
	{
		return "placement/composition";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity bystander = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer || !bystander)
			return;

		LF_PlacementComposition roadblock = Roadblock();
		ctx.Equal(3, roadblock.Count(), "the test composition has three pieces");

		vector gate = GateGround();
		Stand(officer, gate, 0);
		Stand(bystander, gate + "12 0 0", 0);
		PlaceAndPickUp(ctx, roadblock, officer, bystander);

		Stand(officer, gate + INSIDE, 0);
		Stand(bystander, gate + INSIDE + "12 0 0", 0);
		SteepSpot(ctx, roadblock, officer);

		LF_TestCharacterFixture.Delete(officer);
		LF_TestCharacterFixture.Delete(bystander);
	}

	//------------------------------------------------------------------------------------------------
	//! Three cones across the heading, 1.5 m apart.
	protected LF_PlacementComposition Roadblock()
	{
		LF_PlacementComposition composition = new LF_PlacementComposition();
		composition.m_sName = "test-roadblock";
		composition.AddPiece(CONE, "-1.5 0 0");
		composition.AddPiece(CONE, "0 0 0");
		composition.AddPiece(CONE, "1.5 0 0");
		return composition;
	}

	//------------------------------------------------------------------------------------------------
	protected void PlaceAndPickUp(LF_TestContext ctx, LF_PlacementComposition roadblock, IEntity officer, IEntity bystander)
	{
		// The spot the officer's ghost shows: 3 m ahead (north), heading north.
		vector spot[4];
		Math3D.MatrixIdentity4(spot);
		spot[3] = Where(officer) + "0 0 3";

		LF_PlacementCompositionPlan plan = new LF_PlacementCompositionPlan();
		LF_EPlacementVerdict verdict = LF_PlacementCompositions.Plan(officer, roadblock, null, spot, plan);
		ctx.Equal(LF_EPlacementVerdict.VALID, verdict, "the verge ahead of the officer is valid for the whole set: " + plan.m_sReason);
		ctx.Equal(3, plan.Count(), "one plan holds a verdict for each piece");

		// One ghost for the set, from its pieces; it follows the plan and goes away.
		ctx.True(LF_PlacementCompositionGhost.Show(roadblock, plan), "the ghost of the set shows");
		ctx.Equal(3, LF_PlacementCompositionGhost.Count(), "the ghost is one entity per piece");
		LF_PlacementCompositionGhost.Hide();
		ctx.Equal(0, LF_PlacementCompositionGhost.Count(), "hiding the ghost removes every piece of it");

		// A malformed proposal (no forward) places nothing.
		vector noForward[4];
		Copy(spot, noForward);
		noForward[2] = vector.Zero;
		ctx.True(LF_PlacementCompositions.Confirm(officer, roadblock, noForward) == null, "a proposal with no forward is refused");
		ctx.True(LF_PlacementCompositions.LastRefusal().IndexOf("malformed") != -1, "the logged reason says 'malformed': " + LF_PlacementCompositions.LastRefusal());
		ctx.Equal(0, LF_PlacementCompositions.Count(), "a refused proposal remembers no set");

		LF_PlacedComposition placed;
		if (!RedProof())
			placed = LF_PlacementCompositions.Confirm(officer, roadblock, spot);

		if (!placed)
		{
			ctx.Fail("the officer's set was refused: " + LF_PlacementCompositions.LastRefusal());
			return;
		}

		ctx.Equal(1, LF_PlacementCompositions.Count(), "the confirm remembers one set");
		ctx.Equal(3, placed.m_aIds.Count(), "one confirm spawned all three pieces");
		array<IEntity> pieces = {};
		BaseWorld world = GetGame().GetWorld();
		foreach (EntityID id : placed.m_aIds)
		{
			IEntity piece = world.FindEntityByID(id);
			ctx.True(piece != null, "each piece is in the world");
			if (!piece)
				continue;

			pieces.Insert(piece);
			vector origin = piece.GetOrigin();
			float ground = world.GetSurfaceY(origin[0], origin[2]);
			ctx.InRange(origin[1] - ground, -0.05, 0.3, "each piece lies on the ground");
		}

		if (pieces.Count() == 3)
		{
			ctx.InRange(vector.Distance(pieces[0].GetOrigin(), pieces[2].GetOrigin()), 2.9, 3.1, "the outer pieces keep the set's 3 m shape");
			ctx.InRange(Horizontal(pieces[1].GetOrigin(), spot[3]), 0, 0.1, "the middle piece stands at the spot the ghost showed");
		}

		// A second set on the first, and a pick-up by someone 12 m away, change nothing.
		ctx.True(LF_PlacementCompositions.Confirm(officer, roadblock, spot) == null, "a set on top of a set is refused");
		ctx.Equal(1, LF_PlacementCompositions.Count(), "the refused second set remembers nothing");
		if (pieces.Count() == 3)
		{
			ctx.Equal(0, LF_PlacementCompositions.PickUp(bystander, pieces[0]), "a pick-up from 12 m away removes nothing");
			ctx.True(LF_PlacementCompositions.LastRefusal().IndexOf("from the player") != -1, "its reason says how far: " + LF_PlacementCompositions.LastRefusal());
			ctx.Equal(1, LF_PlacementCompositions.Count(), "the set stays");

			// Picking up one piece removes the set.
			ctx.Equal(3, LF_PlacementCompositions.PickUp(officer, pieces[1]), "picking up one piece removes the three");
		}

		foreach (EntityID removed : placed.m_aIds)
			ctx.True(world.FindEntityByID(removed) == null, "no piece of a picked-up set stays");

		ctx.Equal(0, LF_PlacementCompositions.Count(), "the picked-up set is forgotten");
	}

	//------------------------------------------------------------------------------------------------
	//! A table under one cone's footprint makes a rise past the step: the whole set is TOO_STEEP.
	protected void SteepSpot(LF_TestContext ctx, LF_PlacementComposition roadblock, IEntity officer)
	{
		vector spot[4];
		Math3D.MatrixIdentity4(spot);
		spot[3] = Where(officer) + "0 0 3";

		// The table's far edge (+z) under the middle cone: its near half is on the table top, its far half on the ground.
		IEntity table = LF_Utils.SpawnEntityPrefab(TABLE, spot[3]);
		ctx.NotNull(table, "the table spawned");
		if (!table)
			return;

		vector tmin;
		vector tmax;
		table.GetWorldBounds(tmin, tmax);
		vector origin = table.GetOrigin();
		origin[0] = origin[0] + spot[3][0] - (tmin[0] + tmax[0]) * 0.5;
		origin[2] = origin[2] + spot[3][2] - tmax[2];
		table.SetOrigin(origin);
		table.Update();

		LF_PlacementCompositionPlan plan = new LF_PlacementCompositionPlan();
		LF_EPlacementVerdict verdict = LF_PlacementCompositions.Plan(officer, roadblock, null, spot, plan);
		ctx.Equal(LF_EPlacementVerdict.TOO_STEEP, verdict, "a cone half on a table makes the set too steep: " + plan.m_sReason);
		ctx.True(plan.m_sReason.IndexOf("piece 2") != -1, "the reason names the piece that is on the kerb: " + plan.m_sReason);
		ctx.Equal(LF_EPlacementVerdict.VALID, plan.m_aVerdicts[0], "the pieces on level ground are valid");

		ctx.True(LF_PlacementCompositions.Confirm(officer, roadblock, spot) == null, "the set over the steep spot is refused");
		ctx.True(LF_PlacementCompositions.LastRefusal().IndexOf("rises") != -1, "the logged reason says the ground rises: " + LF_PlacementCompositions.LastRefusal());
		ctx.Equal(0, LF_PlacementCompositions.Count(), "the refusal places no piece of the set");

		LF_TestCharacterFixture.Delete(table);
	}

	//------------------------------------------------------------------------------------------------
	//! Where the confirm sees the character (its world transform).
	protected vector Where(IEntity character)
	{
		vector transform[4];
		character.GetWorldTransform(transform);
		return transform[3];
	}

	//------------------------------------------------------------------------------------------------
	protected void Copy(vector from[4], out vector to[4])
	{
		for (int i = 0; i < 4; i++)
			to[i] = from[i];
	}

	//------------------------------------------------------------------------------------------------
	protected float Horizontal(vector a, vector b)
	{
		return vector.Distance(Vector(a[0], 0, a[2]), Vector(b[0], 0, b[2]));
	}

	//------------------------------------------------------------------------------------------------
	//! `yaw` in degrees from north (+z) toward east (+x).
	protected void Stand(IEntity character, vector at, float yaw)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		vector transform[4];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), transform);
		transform[3] = at;
		character.SetWorldTransform(transform);
		character.Update();
	}
}
