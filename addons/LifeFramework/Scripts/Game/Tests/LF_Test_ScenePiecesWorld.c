// red-proof: Observed 2026-11-13 with `tools/cli test --no-build --only LF_Test_ScenePiecesWorld`: LF_ScenePieces.GroundSpot returning the point unprobed failed `- the casing rests on the ground, not at the shooter's height (gap 1.1942 m)` and `- a second casing lands apart from the first`. Restored; green.
// tier: WORLD
//! Scene pieces (#148) in the world: a casing from a shooter standing 1.2 m up lands on the ground
//! under the placement probe, not in the air; a burst's casings land apart; a robber on foot leaves
//! one footprint card (none from a car, none for a car theft); a witness's statement lies at their
//! feet and bags with its note; and every piece is police-only: no inventory pickup, only the bag
//! action.
class LF_Test_ScenePiecesWorld : LF_Test
{
	protected static const int T0 = 2000000;
	protected static const int REPORT = 41;
	//! How far a piece's bottom may be from the surface under it.
	protected static const float RESTS = 0.05;

	protected ref array<IEntity> m_aSpawned = {};

	override string GetName()
	{
		return "investigation/scene-pieces-world";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Evidence.ResetStatics();
		BaseWorld world = GetGame().GetWorld();
		float ground = world.GetSurfaceY(1000, 1000);
		LF_Evidence.OpenScene(REPORT, "pieces-test-uid", LF_WantedReports.OFFENCE_ROBBERY, "Test Square", Vector(1000, ground, 1000), T0);

		IEntity casing = LF_Evidence.OnShot(Vector(1000, ground + 1.2, 1000), T0 + 1000);
		ctx.NotNull(casing, "a shot at the scene leaves a casing");
		if (casing)
		{
			LF_PlacementMeasure m = LF_PlacementAudit.Measure(casing, false);
			LF_Debug.Info("Evidence", "test casing " + LF_PlacementAudit.Format(m));
			ctx.True(Math.AbsFloat(m.m_fGap) < RESTS, string.Format("the casing rests on the ground, not at the shooter's height (gap %1 m)", m.m_fGap));
			RunPoliceOnly(ctx, casing, "casing");
		}

		IEntity second = LF_Evidence.OnShot(Vector(1000, ground, 1000), T0 + 1100);
		if (casing && second)
			ctx.True(vector.DistanceXZ(casing.GetOrigin(), second.GetOrigin()) > 0.2, "a second casing lands apart from the first");

		RunFootprint(ctx, ground);
		RunStatement(ctx, ground);

		LF_Evidence.ResetStatics();
		foreach (IEntity spawned : m_aSpawned)
		{
			if (spawned)
				SCR_EntityHelper.DeleteEntityAndChildren(spawned);
		}

		if (casing)
			SCR_EntityHelper.DeleteEntityAndChildren(casing);

		if (second)
			SCR_EntityHelper.DeleteEntityAndChildren(second);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity Stand(vector at)
	{
		IEntity body = LF_Utils.SpawnEntityPrefab(LF_Evidence.CASING_PREFAB, at, "0 0 0", false);
		m_aSpawned.Insert(body);
		return body;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunFootprint(LF_TestContext ctx, float ground)
	{
		IEntity suspect = Stand(Vector(1005, ground, 1000));
		LF_WantedReportRecord robbery = new LF_WantedReportRecord();
		robbery.m_iSequence = REPORT;
		robbery.m_sOffence = LF_WantedReports.OFFENCE_ROBBERY;
		robbery.m_sPlace = "Test Square";
		IEntity card = LF_ScenePieces.OnReport(robbery, suspect, T0);
		ctx.NotNull(card, "a robber on foot leaves a footprint card");
		if (card)
		{
			m_aSpawned.Insert(card);
			LF_PlacementMeasure m = LF_PlacementAudit.Measure(card, false);
			LF_Debug.Info("Evidence", "test footprint " + LF_PlacementAudit.Format(m));
			ctx.True(Math.AbsFloat(m.m_fGap) < RESTS, string.Format("the footprint card lies on the ground (gap %1 m)", m.m_fGap));
			ctx.True(m.m_fTilt > 80 || (m.m_vMaxs[1] - m.m_vMins[1]) < 0.05, "the card lies flat, not on its edge");
			RunPoliceOnly(ctx, card, "footprint card");
		}

		ctx.True(LF_ScenePieces.OnReport(robbery, suspect, T0 + 10) == null, "one footprint card per report");
		ctx.Equal(1, LF_Evidence.CountFor(REPORT, LF_Evidence.KIND_FOOTPRINT), "exactly one footprint lies for the report");

		LF_WantedReportRecord driven = new LF_WantedReportRecord();
		driven.m_iSequence = REPORT + 1;
		driven.m_sOffence = LF_WantedReports.OFFENCE_ROBBERY;
		driven.m_sVehicle = "#LF-Wanted_VehicleUnknown";
		ctx.True(LF_ScenePieces.OnReport(driven, suspect, T0) == null, "a robber in a car leaves no footprint");

		LF_WantedReportRecord theft = new LF_WantedReportRecord();
		theft.m_iSequence = REPORT + 2;
		theft.m_sOffence = LF_WantedReports.OFFENCE_VEHICLE_THEFT;
		ctx.True(LF_ScenePieces.OnReport(theft, suspect, T0) == null, "a car theft leaves no footprint");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunStatement(LF_TestContext ctx, float ground)
	{
		IEntity witness = Stand(Vector(1003, ground, 1006));
		LF_WitnessScene scene = new LF_WitnessScene();
		scene.m_iReport = REPORT;
		scene.m_sOffence = LF_WantedReports.OFFENCE_ROBBERY;
		scene.m_sPlace = "Test Square";
		LF_WitnessStatement statement = new LF_WitnessStatement();
		statement.m_sClothing = "#AR-Jacket_M88";
		statement.m_sPlateFragment = "AB";
		IEntity paper = LF_ScenePieces.LeaveStatement(scene, statement, witness, T0);
		ctx.NotNull(paper, "a witness's statement lies at their feet");
		if (!paper)
			return;

		LF_PlacementMeasure m = LF_PlacementAudit.Measure(paper, false);
		LF_Debug.Info("Evidence", "test statement " + LF_PlacementAudit.Format(m));
		ctx.True(Math.AbsFloat(m.m_fGap) < RESTS, string.Format("the statement lies on the ground (gap %1 m)", m.m_fGap));
		ctx.True(vector.DistanceXZ(paper.GetOrigin(), witness.GetOrigin()) < 1, "the statement lies by the witness");
		RunPoliceOnly(ctx, paper, "statement");

		IEntity bag = LF_Evidence.Bag(paper, null, T0 + 60000);
		ctx.NotNull(bag, "an officer bags the statement");
		if (!bag)
			return;

		m_aSpawned.Insert(bag);
		LF_EvidenceBagComponent bagComponent = LF_EvidenceBagComponent.Cast(bag.FindComponent(LF_EvidenceBagComponent));
		ctx.NotNull(bagComponent, "the bag carries its component");
		if (bagComponent)
		{
			ctx.EqualStr(LF_Evidence.KIND_STATEMENT, bagComponent.GetKind(), "the bag says it holds a statement");
			ctx.EqualStr(LF_ScenePieces.NoteStatement("#AR-Jacket_M88", "", "AB"), bagComponent.GetNote(), "the bag carries what the witness said");
			ctx.Equal(REPORT, bagComponent.GetReport(), "the statement is filed against its report");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A piece is not an inventory item (no vanilla pickup, no vicinity grab) and offers the bag action.
	protected void RunPoliceOnly(LF_TestContext ctx, notnull IEntity piece, string what)
	{
		ctx.True(piece.FindComponent(InventoryItemComponent) == null, "nobody picks up the " + what + " by hand: it is not an inventory item");
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(piece.FindComponent(BaseActionsManagerComponent));
		bool bag;
		bool pickUp;
		if (manager)
		{
			array<BaseUserAction> actions = {};
			manager.GetActionsList(actions);
			foreach (BaseUserAction action : actions)
			{
				if (LF_BagEvidenceAction.Cast(action))
					bag = true;

				if (SCR_PickUpItemAction.Cast(action))
					pickUp = true;
			}
		}

		ctx.True(bag, "the " + what + " offers Bag as evidence");
		ctx.False(pickUp, "the " + what + " offers no Pick up");
	}
}
