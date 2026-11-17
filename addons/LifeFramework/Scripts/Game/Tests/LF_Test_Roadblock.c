// red-proof: observed 2026-11-16 (#1941) with `tools\cli test --no-build --red LF_Test_Roadblock`: `RED-OK police/roadblock (fails with the hook on, 12 assertions)` (RedProof() skips the officer's deploy, so nothing is placed); green without the hook (`--only`, 1 passed).
// tier: WORLD
//! #1941 acceptance: the police roadblock kit (LF_Roadblock.c). On open ground inside the annex
//! (the stand-in for a road: the kit lies across the way the officer faces, and GridWorld has no AI
//! road network) a civilian is refused with the reason logged and the kit is kept. An on-duty officer
//! deploys the kit: one barrier and four cones, each on the ground, the kit used up. A vehicle seen
//! moving near the set and then stopped is logged once. A civilian cannot pick the set up; the officer
//! folds all five pieces away with one pick-up and carries the kit again.
class LF_Test_Roadblock : LF_Test
{
	protected static const string OFFICER_UID = "roadblock-officer";
	protected static const string CIVILIAN_UID = "roadblock-civilian";
	//! Just inside the annex's north gate, on GridWorld ground (LF_Test_GridWorldBoot's gate spawn);
	//! the fixture is read by name and falls back to its known spot when the world has none.
	protected static const vector GATE = "74 0 108";
	//! Further into the annex from the gate, clear of its fence line.
	protected static const vector INSIDE = "-8 0 -8";

	override string GetName()
	{
		return "police/roadblock";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity civilian = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer || !civilian)
			return;

		LF_PlayerAccount account = LF_TestPoliceKit.Account(OFFICER_UID, true);
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		LF_PoliceRanks.BindOfficer(officer, OFFICER_UID);
		LF_PlayerAccount civilianAccount = LF_TestPoliceKit.Account(CIVILIAN_UID, false);
		LF_PoliceRanks.BindOfficer(civilian, CIVILIAN_UID);

		vector gate = GATE;
		IEntity spawn = GetGame().GetWorld().FindEntityByName("Spawn_Grid_Gate");
		if (spawn)
			gate = spawn.GetOrigin();

		Stand(officer, gate + INSIDE, 0);
		Stand(civilian, gate + INSIDE + "12 0 0", 0);
		RunRoadblock(ctx, officer, civilian);

		LF_TestCharacterFixture.Delete(officer);
		LF_TestCharacterFixture.Delete(civilian);
		LF_TestPoliceKit.Release(OFFICER_UID);
		LF_TestPoliceKit.Release(CIVILIAN_UID);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRoadblock(LF_TestContext ctx, IEntity officer, IEntity civilian)
	{
		LF_PlacementComposition composition = LF_Roadblocks.Composition();
		ctx.Equal(5, composition.Count(), "the kit lays one barrier and four cones");

		// A civilian is refused, with the reason logged, and keeps the kit.
		IEntity refusedKit = LF_Utils.SpawnEntityPrefab(LF_Roadblocks.KIT, civilian.GetOrigin() + "0 0 0.5");
		ctx.NotNull(refusedKit, "the folded kit spawns");
		vector civilianSpot[4];
		LF_Roadblocks.SpotFor(civilian, refusedKit, civilianSpot);
		ctx.True(LF_Roadblocks.Deploy(civilian, refusedKit, civilianSpot) == null, "a civilian cannot deploy the kit");
		ctx.True(LF_Roadblocks.LastRefusal().IndexOf("not on-duty police") != -1, "the refusal says why: " + LF_Roadblocks.LastRefusal());
		ctx.Equal(0, LF_Roadblocks.Count(), "a refused deploy leaves no roadblock");
		ctx.Equal(0, LF_PlacementCompositions.Count(), "a refused deploy places no piece");
		ctx.True(GetGame().GetWorld().FindEntityByID(refusedKit.GetID()) != null, "the refused kit is not used up");
		LF_TestCharacterFixture.Delete(refusedKit);

		// An on-duty officer deploys the set.
		IEntity kit = LF_Utils.SpawnEntityPrefab(LF_Roadblocks.KIT, officer.GetOrigin() + "0.5 0 0");
		ctx.NotNull(kit, "the officer's kit spawns");
		vector spot[4];
		LF_EPlacementVerdict kitVerdict = LF_Roadblocks.SpotFor(officer, kit, spot);
		LF_PlacementCompositionPlan plan = new LF_PlacementCompositionPlan();
		LF_EPlacementVerdict verdict = LF_PlacementCompositions.Plan(officer, composition, kit, spot, plan);
		ctx.Equal(LF_EPlacementVerdict.VALID, verdict, "the open ground ahead is valid for the whole set: " + plan.m_sReason + " (kit plan " + LF_PlacementRules.Name(kitVerdict) + ")");
		ctx.True(LF_PlacementCompositionGhost.Show(composition, plan), "the ghost of the set shows");
		ctx.Equal(5, LF_PlacementCompositionGhost.Count(), "the ghost has a piece for each prop");
		LF_PlacementCompositionGhost.Hide();

		EntityID kitId = kit.GetID();
		LF_PlacedComposition placed;
		if (!RedProof())
			placed = LF_Roadblocks.Deploy(officer, kit, spot);

		if (!placed)
		{
			ctx.Fail("the officer's roadblock was refused: " + LF_Roadblocks.LastRefusal());
			LF_TestCharacterFixture.Delete(kit);
			return;
		}

		ctx.Equal(1, LF_Roadblocks.Count(), "the deploy registers one roadblock");
		ctx.Equal(5, placed.m_aIds.Count(), "one deploy spawned all five pieces");
		ctx.True(GetGame().GetWorld().FindEntityByID(kitId) == null, "the folded kit is used up");

		BaseWorld world = GetGame().GetWorld();
		IEntity nearest;
		float nearestDistance = 1000;
		int barriers;
		foreach (EntityID id : placed.m_aIds)
		{
			IEntity piece = world.FindEntityByID(id);
			ctx.NotNull(piece, "each piece is in the world");
			if (!piece)
				continue;

			vector origin = piece.GetOrigin();
			float ground = world.GetSurfaceY(origin[0], origin[2]);
			ctx.InRange(origin[1] - ground, -0.05, 0.3, "each piece lies on the ground");
			if (LF_Utils.SamePrefab(piece.GetPrefabData().GetPrefabName(), LF_Roadblocks.BARRIER))
				barriers++;

			float distance = vector.Distance(origin, spot[3]);
			if (distance < nearestDistance)
			{
				nearestDistance = distance;
				nearest = piece;
			}
		}

		ctx.Equal(1, barriers, "one of the pieces is the barrier");
		if (!nearest)
			return;

		// A vehicle seen moving near the set and then stopped is logged, once.
		LF_RoadblockSet roadblock = LF_Roadblocks.Find(nearest);
		ctx.NotNull(roadblock, "a piece finds its roadblock");
		if (roadblock)
		{
			EntityID car = nearest.GetID();
			ctx.False(roadblock.Observe(car, 0, 5, "test car"), "a car that was never seen moving is not logged as stopped");
			ctx.False(roadblock.Observe(car, 8, 7, "test car"), "a car rolling up is not a stop");
			ctx.True(roadblock.Observe(car, 0.1, 4, "test car"), "the car that rolled up and stopped is logged");
			ctx.False(roadblock.Observe(car, 0, 4, "test car"), "the same stop is logged once");
			ctx.Equal(1, LF_Roadblocks.StoppedCount(), "the roadblock counts the stop");
			ctx.False(roadblock.Observe(car, 8, 30, "test car"), "a car far from the set is not watched");
		}

		// A civilian cannot fold it away; the officer, stood by a piece, can.
		ctx.Equal(0, LF_Roadblocks.PickUp(civilian, nearest), "a civilian cannot pick the roadblock up");
		ctx.True(LF_Roadblocks.LastRefusal().IndexOf("not on-duty police") != -1, "the refusal says why: " + LF_Roadblocks.LastRefusal());
		ctx.Equal(1, LF_Roadblocks.Count(), "the roadblock stays");

		Stand(officer, nearest.GetOrigin() + "1.5 0 0", 0);
		ctx.Equal(5, LF_Roadblocks.PickUp(officer, nearest), "one pick-up removes all five pieces: " + LF_Roadblocks.LastRefusal());
		foreach (EntityID removed : placed.m_aIds)
			ctx.True(world.FindEntityByID(removed) == null, "no piece of a picked-up roadblock stays");

		ctx.Equal(0, LF_Roadblocks.Count(), "the picked-up roadblock is forgotten");
		ctx.True(LF_TestCharacterFixture.HasItem(officer, LF_Roadblocks.KIT), "the officer carries the kit again");
	}

	//------------------------------------------------------------------------------------------------
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
