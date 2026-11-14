// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_VehicleStateLegacy`:
// LF_VehicleOwnership.ApplyAll carrying the previous live record's state onto the loaded one (a state with
// no row) failed `[LFTEST] FAIL persist/vehicle-state-legacy`, `- the car loads untowed (version 32 load 1)
// (expected 0, got 1)`, `- the car loads with no compartment (version 23 load 1) ...`. Restored; green.
// tier: PERSISTENCE
//! #752a: a save from before the "vehiclestate" payload (version 32, and version 23, the e2e legacy
//! save) has no state rows. Each loads, twice, with every vehicle untowed, uninspected and with no
//! compartment, and the "vehicles" records unchanged. The test puts back the stores it found.
class LF_Test_VehicleStateLegacy : LF_Test
{
	protected static const string OWNER = "vehicle-state-legacy-owner";
	protected static const string CAR = "vehicle-state-legacy-car";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/vehicle-state-legacy";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		LF_VehicleOwnership.Register(CAR, OWNER, CAR, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", "VS LG 3001", "#EL-Vehicle_S105", true);
		LF_VehicleOwnership.Store(CAR, OWNER);

		CheckVersion(ctx, persistence, 32);
		CheckVersion(ctx, persistence, 23);

		LF_VehicleOwnership.ApplyAll(before);
		kept.Restore();
		LF_LicensePlateRegistry.Release("VS LG 3001");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckVersion(LF_TestContext ctx, LF_PersistenceComponent persistence, int version)
	{
		// The live car has state; a save written in the old shape cannot hold it.
		LF_VehicleOwnership.SetState(CAR, LF_EVehicleTowState.TOWED, 1800000000, 1800000600, 3);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, saveContext, version);
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(CAR), string.Format("the version %1 save carries the vehicle record", version));
		ctx.False(saved.Contains("m_iInspectedAt"), string.Format("the version %1 save has no vehicle state rows", version));

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (version %1 load %2)", version, pass);
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the old save parses" + tag);
			LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);

			LF_VehicleStateRow state = LF_VehicleOwnership.StateOf(CAR);
			ctx.NotNull(state, "the car is back" + tag);
			if (!state)
				return;

			ctx.Equal(LF_EVehicleTowState.NONE, state.m_iTowState, "the car loads untowed" + tag);
			ctx.Equal(0, state.m_iTowedAt, "the car has no tow stamp" + tag);
			ctx.Equal(0, state.m_iInspectedAt, "the car loads uninspected" + tag);
			ctx.Equal(0, state.m_iCompartment, "the car loads with no compartment" + tag);
			ctx.EqualStr("VS LG 3001", LF_VehicleOwnership.GetPlate(CAR), "the vehicles record keeps its plate" + tag);
			ctx.True(LF_VehicleOwnership.IsStored(CAR), "the vehicles record keeps the garaged flag" + tag);
			ctx.Equal(0, LF_VehicleOwnership.ExportState().Count(), "no vehicle has state after the old load" + tag);
		}
	}
}
