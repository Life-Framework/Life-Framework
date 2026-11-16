// red-proof: on main this file fails to compile (LF_Roadblocks.ExpireAt and LF_RoadblockConfig are absent), which fails the suite; with the hook on (`--red`) every expire call is skipped and each count and gone assertion fails. A green or `--red` boot was unavailable on this PC at ship time (stale rdb, no lane with RAM for a Workbench refresh); the lead runs `--red LF_Test_RoadblockExpiry` where a lane has room.
// tier: WORLD
//! #2100 acceptance: a deployed roadblock clears itself (LF_Roadblock.c). Three sets are laid through
//! the server path (LF_Roadblocks.Deploy). The first is older than the configured time (20 minutes by
//! default, Configs/Police/LF_Roadblock.conf) and is removed with every piece when the expiry runs.
//! The second is removed when its officer goes off duty. The third, inside the window and laid by an
//! officer still on duty, stays; it goes when its officer's account is gone.
class LF_Test_RoadblockExpiry : LF_Test
{
	protected static const string OFFICER_A_UID = "roadblock-expiry-a";
	protected static const string OFFICER_B_UID = "roadblock-expiry-b";
	//! Just inside the annex's north gate, as LF_Test_Roadblock.
	protected static const vector GATE = "74 0 108";
	protected static const vector INSIDE = "-8 0 -8";

	override string GetName()
	{
		return "police/roadblock-expiry";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity officerA = LF_TestCharacterFixture.Spawn(ctx);
		IEntity officerB = LF_TestCharacterFixture.Spawn(ctx);
		if (!officerA || !officerB)
			return;

		LF_PlayerAccount accountA = LF_TestPoliceKit.Account(OFFICER_A_UID, true);
		accountA.SetFaction(LF_Faction.POLICE);
		accountA.SetOnDuty(true);
		LF_PoliceRanks.BindOfficer(officerA, OFFICER_A_UID);
		LF_PlayerAccount accountB = LF_TestPoliceKit.Account(OFFICER_B_UID, true);
		accountB.SetFaction(LF_Faction.POLICE);
		accountB.SetOnDuty(true);
		LF_PoliceRanks.BindOfficer(officerB, OFFICER_B_UID);

		vector gate = GATE;
		IEntity spawn = GetGame().GetWorld().FindEntityByName("Spawn_Grid_Gate");
		if (spawn)
			gate = spawn.GetOrigin();

		RunExpiry(ctx, officerA, officerB, accountB, gate + INSIDE);

		LF_TestCharacterFixture.Delete(officerA);
		LF_TestCharacterFixture.Delete(officerB);
		LF_TestPoliceKit.Release(OFFICER_A_UID);
		LF_TestPoliceKit.Release(OFFICER_B_UID);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunExpiry(LF_TestContext ctx, IEntity officerA, IEntity officerB, LF_PlayerAccount accountB, vector origin)
	{
		ctx.Equal(20 * 60000, LF_RoadblockConfig.ExpiryMs(), "the shipped config stands a roadblock for 20 minutes");

		// Three sets, laid by the server path one stretch apart: A, B, then A again.
		LF_PlacedComposition placedOld = Lay(ctx, officerA, origin);
		LF_PlacedComposition placedDuty = Lay(ctx, officerB, origin + "14 0 0");
		LF_PlacedComposition placedFresh = Lay(ctx, officerA, origin + "-14 0 0");
		if (!placedOld || !placedDuty || !placedFresh)
			return;

		ctx.Equal(3, LF_Roadblocks.Count(), "three roadblocks stand");
		LF_RoadblockSet setOld = SetOf(placedOld);
		LF_RoadblockSet setDuty = SetOf(placedDuty);
		LF_RoadblockSet setFresh = SetOf(placedFresh);
		ctx.NotNull(setOld, "the first set is registered");
		ctx.NotNull(setDuty, "the second set is registered");
		ctx.NotNull(setFresh, "the third set is registered");
		if (!setOld || !setDuty || !setFresh)
			return;

		// Nothing is due at the moment of the deploy.
		int now = setFresh.m_iDeployedMs;
		if (!RedProof())
			ctx.Equal(0, LF_Roadblocks.ExpireAt(now), "nothing clears the moment the sets are laid");

		ctx.Equal(3, LF_Roadblocks.Count(), "three roadblocks still stand");

		// The first set was laid longer ago than the configured time; the others were not.
		setOld.m_iDeployedMs = now - LF_RoadblockConfig.ExpiryMs() - 1000;
		if (!RedProof())
			ctx.Equal(1, LF_Roadblocks.ExpireAt(now), "one set has stood its time and clears");

		ctx.Equal(2, LF_Roadblocks.Count(), "the old roadblock is forgotten");
		ExpectGone(ctx, placedOld, "an expired roadblock leaves no piece");
		ExpectStanding(ctx, placedDuty, "the roadblock inside the window stays (B on duty)");
		ExpectStanding(ctx, placedFresh, "the roadblock inside the window stays (A on duty)");

		// B goes off duty: B's set clears at once, A's stays.
		accountB.SetOnDuty(false);
		if (!RedProof())
			ctx.Equal(1, LF_Roadblocks.ExpireAt(now), "the set of an officer who went off duty clears");

		ctx.Equal(1, LF_Roadblocks.Count(), "one roadblock stands");
		ExpectGone(ctx, placedDuty, "a roadblock whose officer went off duty leaves no piece");
		ExpectStanding(ctx, placedFresh, "an on-duty officer's roadblock inside the window stays");

		// A leaves (the account is gone): the last set clears too.
		LF_TestPoliceKit.Release(OFFICER_A_UID);
		if (!RedProof())
			ctx.Equal(1, LF_Roadblocks.ExpireAt(now), "the set of an officer who left clears");

		ctx.Equal(0, LF_Roadblocks.Count(), "no roadblock stands");
		ExpectGone(ctx, placedFresh, "a roadblock whose officer left leaves no piece");
	}

	//------------------------------------------------------------------------------------------------
	//! Lays one set for `officer` at `at` with a kit and the server's own plan.
	protected LF_PlacedComposition Lay(LF_TestContext ctx, IEntity officer, vector at)
	{
		Stand(officer, at, 0);
		IEntity kit = LF_Utils.SpawnEntityPrefab(LF_Roadblocks.KIT, officer.GetOrigin() + "0.5 0 0");
		ctx.NotNull(kit, "the kit spawns");
		if (!kit)
			return null;

		LF_PlacedComposition placed = LF_Roadblocks.DeployOwnSpot(officer, kit);
		if (!placed)
		{
			ctx.Fail("the roadblock was refused: " + LF_Roadblocks.LastRefusal());
			LF_TestCharacterFixture.Delete(kit);
		}

		return placed;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_RoadblockSet SetOf(LF_PlacedComposition placed)
	{
		if (!placed || placed.m_aIds.IsEmpty())
			return null;

		IEntity piece = GetGame().GetWorld().FindEntityByID(placed.m_aIds[0]);
		return LF_Roadblocks.Find(piece);
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpectGone(LF_TestContext ctx, LF_PlacedComposition placed, string message)
	{
		BaseWorld world = GetGame().GetWorld();
		foreach (EntityID id : placed.m_aIds)
			ctx.True(world.FindEntityByID(id) == null, message);
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpectStanding(LF_TestContext ctx, LF_PlacedComposition placed, string message)
	{
		BaseWorld world = GetGame().GetWorld();
		foreach (EntityID id : placed.m_aIds)
			ctx.True(world.FindEntityByID(id) != null, message);
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
