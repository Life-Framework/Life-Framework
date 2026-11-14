// red-proof: 2026-11-14, LF_VehicleOwnership.IsOwnedEntity returning false: `--only LF_Test_CleanupPersist` failed `the owned car survives the sweep`. Restored, green.
// tier: PERSISTENCE
//! World cleanup (#690): a sweep never deletes a saved vehicle record. A swept stray car leaves the
//! saved vehicle records as they were, and the owned car's record still serializes.
class LF_Test_CleanupPersist : LF_Test
{
	protected static const string OWNED_ID = "cleanup-persist-owned";
	protected static const int HOUR_MS = 3600000;	// lf-fixed: test clock, longer than every shipped age

	override string GetName()
	{
		return "cleanup/persist";
	}

	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		LF_Cleanup.ResetStatics();
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		LF_VehicleOwnership.Register(OWNED_ID, "cleanup-persist-uid", OWNED_ID, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "cleanup-lot", "CP AA 2002", "#EL-Vehicle_S105", true);
		int recordsBefore = LF_VehicleOwnership.ExportAll().Count();

		float y = GetGame().GetWorld().GetSurfaceY(900, 100);
		IEntity car = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.S105_LOCKED, Vector(900, y, 100));
		ctx.NotNull(car, "the car spawned");
		if (!car)
			return;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);
		if (vehicleLock)
			vehicleLock.SetVehicleIdentifier(OWNED_ID);

		EntityID carId = car.GetID();
		array<IEntity> entities = {car};
		array<vector> players = {};
		LF_Cleanup.Sweep(entities, players, 0);
		LF_Cleanup.Sweep(entities, players, HOUR_MS * 2);
		ctx.True(GetGame().GetWorld().FindEntityByID(carId) != null, "the owned car survives the sweep");
		ctx.Equal(recordsBefore, LF_VehicleOwnership.ExportAll().Count(), "the sweep leaves every saved vehicle record");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		ctx.True(saveContext.SaveToString().Contains(OWNED_ID), "the save still carries the owned vehicle record");

		SCR_EntityHelper.DeleteEntityAndChildren(GetGame().GetWorld().FindEntityByID(carId));
		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release("CP AA 2002");
		LF_Cleanup.ResetStatics();
	}
}
