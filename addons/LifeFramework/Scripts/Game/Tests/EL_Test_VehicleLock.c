// red-proof: make EL_VehicleStorageAccessControl.IsStorageLocked return false (or make
// EL_VehicleLockComponent.IsLocked return false) and run `tools\cli test --tier all`;
// the locked-denies-unlocked-allows assertions fail. For the key path, break
// IdentifiersMatch (drop the empty guard) and the unbound-key assertions fail.

// tier: WORLD
class EL_Test_VehicleLock : EL_Test
{
	protected static const ResourceName LOCK_FIXTURE = "{EA882144DF2D4BEC}Prefabs/Vehicles/Test/VehicleLockFixture.et";
	protected static const ResourceName KEY_PREFAB = "{9D94B834AF514B4A}Prefabs/Items/Roleplay/VehicleKey.et";

	override string GetName()
	{
		return "vehiclelock/lock-key-access";
	}

	override void Run(EL_TestContext ctx)
	{
		Resource res = Resource.Load(LOCK_FIXTURE);
		ctx.True(res.IsValid(), "lock fixture prefab loads: " + LOCK_FIXTURE);
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;

		IEntity vehicle = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(vehicle != null, "lock fixture spawns a vehicle");
		if (ctx.FailureCount() > 0)
			return;

		EL_VehicleLockComponent vehicleLock = EL_Component<EL_VehicleLockComponent>.Find(vehicle);
		ctx.NotNull(vehicleLock, "fixture carries EL_VehicleLockComponent");
		if (ctx.FailureCount() > 0)
			return;

		EL_VehicleStorageAccessControl accessControl = EL_Component<EL_VehicleStorageAccessControl>.Find(vehicle);
		ctx.NotNull(accessControl, "fixture carries EL_VehicleStorageAccessControl");
		if (ctx.FailureCount() > 0)
			return;

		ctx.False(vehicleLock.IsVehicleLocked(), "vehicle starts unlocked");
		ctx.False(accessControl.IsStorageLocked(null), "storage access allowed while unlocked");

		vehicleLock.SetLocked(true);
		ctx.True(vehicleLock.IsVehicleLocked(), "vehicle is locked after SetLocked(true)");
		ctx.True(accessControl.IsStorageLocked(null), "storage access denied while locked");

		vehicleLock.ToggleLocked();
		ctx.False(vehicleLock.IsVehicleLocked(), "vehicle unlocked after ToggleLocked");
		ctx.False(accessControl.IsStorageLocked(null), "storage access allowed again after unlock");

		Resource keyRes = Resource.Load(KEY_PREFAB);
		ctx.True(keyRes.IsValid(), "key prefab loads: " + KEY_PREFAB);
		if (ctx.FailureCount() > 0)
			return;

		IEntity key = GetGame().SpawnEntityPrefab(keyRes, GetGame().GetWorld(), params);
		ctx.True(key != null, "key prefab spawns an entity");
		if (ctx.FailureCount() > 0)
			return;

		EL_VehicleKeyComponent keyComponent = EL_Component<EL_VehicleKeyComponent>.Find(key);
		ctx.NotNull(keyComponent, "key entity carries EL_VehicleKeyComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.False(vehicleLock.IsValidKey(key), "an unbound key opens nothing");

		vehicleLock.SetVehicleIdentifier("test-vehicle");
		keyComponent.BindToVehicleIdentifier("test-vehicle");
		ctx.True(vehicleLock.IsValidKey(key), "a key bound to the vehicle identifier is valid");

		keyComponent.BindToVehicleIdentifier("other-vehicle");
		ctx.False(vehicleLock.IsValidKey(key), "a key bound to a different vehicle is rejected");
	}
};