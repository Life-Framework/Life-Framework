// red-proof: 2026-11-14, LF_VehicleOwnership.IsOwnedEntity returning false: `--only LF_Test_CleanupSweep` failed `an owned parked car is kept` and `the old far item and the stray car are swept (expected 2, got 3)`. Restored, green.
// tier: WORLD
//! World cleanup (#690): an old dropped item far from players is removed; one near a player, an
//! owned parked car and a named entity are kept; an unowned car left alone goes too.
class LF_Test_CleanupSweep : LF_Test
{
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const string OWNER = "cleanup-owner";
	protected static const string OWNED_ID = "cleanup-owned";
	protected static const int HOUR_MS = 3600000;	// lf-fixed: test clock, longer than every shipped age

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "cleanup/sweep";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_Cleanup.ResetStatics();
		LF_CleanupConfig config = LF_CleanupConfig.Get();
		ctx.NotNull(config, "the cleanup config loads");
		if (!config)
			return;

		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		LF_VehicleOwnership.Register(OWNED_ID, OWNER, OWNED_ID, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, "cleanup-lot", "CL AA 1001", "#EL-Vehicle_S105", true);

		vector player = Vector(340, 0, 100);
		player[1] = GetGame().GetWorld().GetSurfaceY(player[0], player[2]);
		vector nearSpot = Vector(345, GetGame().GetWorld().GetSurfaceY(345, 100), 100);
		vector farSpot = Vector(900, GetGame().GetWorld().GetSurfaceY(900, 100), 100);
		vector farSpot2 = Vector(930, GetGame().GetWorld().GetSurfaceY(930, 100), 100);
		vector farSpot3 = Vector(960, GetGame().GetWorld().GetSurfaceY(960, 100), 100);

		IEntity farItem = LF_Utils.SpawnEntityPrefab(APPLE, farSpot);
		IEntity nearItem = LF_Utils.SpawnEntityPrefab(APPLE, nearSpot);
		IEntity ownedCar = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.S105_LOCKED, farSpot2);
		IEntity strayCar = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.S105_LOCKED, farSpot3);
		ctx.NotNull(farItem, "the far item spawned");
		ctx.NotNull(nearItem, "the near item spawned");
		ctx.NotNull(ownedCar, "the owned car spawned");
		ctx.NotNull(strayCar, "the stray car spawned");
		if (!farItem || !nearItem || !ownedCar || !strayCar)
			return;

		LF_VehicleLockComponent ownedLock = LF_Component<LF_VehicleLockComponent>.Find(ownedCar);
		if (ownedLock)
			ownedLock.SetVehicleIdentifier(OWNED_ID);

		EntityID farId = farItem.GetID();
		EntityID nearId = nearItem.GetID();
		EntityID ownedId = ownedCar.GetID();
		EntityID strayId = strayCar.GetID();

		array<IEntity> entities = {farItem, nearItem, ownedCar, strayCar};
		array<vector> players = {player};

		// First sweep: every entity is new, so none is old yet.
		ctx.Equal(0, LF_Cleanup.Sweep(entities, players, 0), "a fresh sweep removes nothing: nothing has aged");

		// Much later: the far item and the stray car have gone unwatched for the whole window.
		int removed = LF_Cleanup.Sweep(entities, players, HOUR_MS * 2);
		ctx.Equal(2, removed, "the old far item and the stray car are swept");
		ctx.True(GetGame().GetWorld().FindEntityByID(farId) == null, "the old far item is removed");
		ctx.True(GetGame().GetWorld().FindEntityByID(strayId) == null, "the abandoned unowned car is removed");
		ctx.True(GetGame().GetWorld().FindEntityByID(nearId) != null, "an item near a player is kept");
		ctx.True(GetGame().GetWorld().FindEntityByID(ownedId) != null, "an owned parked car is kept");

		if (!GetGame().GetWorld().FindEntityByID(nearId))
			return;

		LF_Cleanup.Exempt(nearItem);
		LF_Cleanup.Sweep({nearItem}, {farSpot}, HOUR_MS * 4);
		ctx.True(GetGame().GetWorld().FindEntityByID(nearId) != null, "a registered exempt item is kept even when old and far");

		SCR_EntityHelper.DeleteEntityAndChildren(GetGame().GetWorld().FindEntityByID(nearId));
		SCR_EntityHelper.DeleteEntityAndChildren(GetGame().GetWorld().FindEntityByID(ownedId));
		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release("CL AA 1001");
		LF_Cleanup.ResetStatics();
	}
}
