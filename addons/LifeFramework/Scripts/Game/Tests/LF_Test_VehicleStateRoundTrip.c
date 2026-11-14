// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_VehicleStateRoundTrip,...`:
// LF_VehicleOwnership.ExportState writing 0 for m_iInspectedAt failed `[LFTEST] FAIL persist/vehicle-state`,
// `- the inspected-at stamp is kept (load 1) (expected 1800000600, got 0)`. Restored; green.
// tier: PERSISTENCE
//! #752a through the real save path ("vehiclestate" payload). A towed, inspected car with a fitted
//! compartment and a police-impounded car come back with every field after the store is written and
//! read, twice; a stock car writes no row and loads untowed. The "vehicles" records are unchanged.
class LF_Test_VehicleStateRoundTrip : LF_Test
{
	protected static const string OWNER = "vehicle-state-trip-owner";
	protected static const string TOWED = "vehicle-state-trip-towed";
	protected static const string SEIZED = "vehicle-state-trip-seized";
	protected static const string STOCK = "vehicle-state-trip-stock";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/vehicle-state";
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

		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		LF_VehicleOwnership.Register(TOWED, OWNER, TOWED, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", "VS RT 2001", "#EL-Vehicle_S105", true);
		LF_VehicleOwnership.Register(SEIZED, OWNER, SEIZED, LF_VehicleCatalog.UAZ469, LF_VehicleCatalog.UAZ469_LOCKED, "roadmap-lot", "VS RT 2002", "#EL-Vehicle_UAZ469", false);
		LF_VehicleOwnership.Register(STOCK, OWNER, STOCK, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "roadmap-lot", "VS RT 2003", "#EL-Vehicle_S105", true);
		LF_VehicleOwnership.Store(STOCK, OWNER);
		ctx.True(LF_VehicleOwnership.SetState(TOWED, LF_EVehicleTowState.TOWED, 1800000000, 1800000600, 2), "the towed car takes its state");
		ctx.True(LF_VehicleOwnership.SetState(SEIZED, LF_EVehicleTowState.POLICE_IMPOUND, 1800001000, 0, 0), "the seized car takes its state");

		bool stockRow = false;
		foreach (LF_VehicleStateRow exported : LF_VehicleOwnership.ExportState())
		{
			if (exported && exported.m_sVehicleId == STOCK)
				stockRow = true;
		}
		ctx.False(stockRow, "a stock car writes no state row");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("m_iInspectedAt"), "the save carries the vehicle state rows");

		// Change the live state so the load has to put it back.
		LF_VehicleOwnership.SetState(TOWED, LF_EVehicleTowState.NONE, 0, 0, 0);
		LF_VehicleOwnership.SetState(SEIZED, LF_EVehicleTowState.NONE, 0, 0, 0);
		LF_VehicleOwnership.SetState(STOCK, LF_EVehicleTowState.TOWED, 5, 5, 1);

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			JsonLoadContext versionContext = new JsonLoadContext(false);
			ctx.True(versionContext.LoadFromString(saved), "the save parses" + tag);
			int version;
			versionContext.ReadValue("version", version);
			ctx.Equal(33, version, "the save is written at version 33 (vehiclestate is payload 33)" + tag);

			JsonLoadContext loadContext = new JsonLoadContext(false);
			loadContext.LoadFromString(saved);
			LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);

			LF_VehicleStateRow towed = LF_VehicleOwnership.StateOf(TOWED);
			ctx.NotNull(towed, "the towed car is back" + tag);
			if (towed)
			{
				ctx.Equal(LF_EVehicleTowState.TOWED, towed.m_iTowState, "the tow state is kept" + tag);
				ctx.Equal(1800000000, towed.m_iTowedAt, "the towed-at stamp is kept" + tag);
				ctx.Equal(1800000600, towed.m_iInspectedAt, "the inspected-at stamp is kept" + tag);
				ctx.Equal(2, towed.m_iCompartment, "the compartment level is kept" + tag);
			}

			LF_VehicleStateRow seized = LF_VehicleOwnership.StateOf(SEIZED);
			ctx.NotNull(seized, "the seized car is back" + tag);
			if (seized)
			{
				ctx.Equal(LF_EVehicleTowState.POLICE_IMPOUND, seized.m_iTowState, "the police impound is kept" + tag);
				ctx.Equal(1800001000, seized.m_iTowedAt, "the seizure stamp is kept" + tag);
				ctx.Equal(0, seized.m_iInspectedAt, "an uninspected car stays uninspected" + tag);
			}

			LF_VehicleStateRow stock = LF_VehicleOwnership.StateOf(STOCK);
			ctx.NotNull(stock, "the stock car is back" + tag);
			if (stock)
			{
				ctx.Equal(LF_EVehicleTowState.NONE, stock.m_iTowState, "the stock car loads untowed" + tag);
				ctx.Equal(0, stock.m_iCompartment, "the stock car loads with no compartment" + tag);
			}

			ctx.EqualStr("VS RT 2001", LF_VehicleOwnership.GetPlate(TOWED), "the vehicles record keeps its plate" + tag);
			ctx.True(LF_VehicleOwnership.IsStored(STOCK), "the vehicles record keeps the garaged flag" + tag);
			ctx.False(LF_VehicleOwnership.IsLocked(SEIZED), "the vehicles record keeps the lock flag" + tag);
		}

		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release("VS RT 2001");
		LF_LicensePlateRegistry.Release("VS RT 2002");
		LF_LicensePlateRegistry.Release("VS RT 2003");
	}
}
