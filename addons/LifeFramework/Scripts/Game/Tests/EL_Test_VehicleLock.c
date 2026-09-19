// red-proof: make EL_VehicleStorageAccessControl.IsStorageLocked return false (or make
// EL_VehicleLockComponent.IsLocked return false) and run `tools\cli test --tier all`;
// the locked-denies-unlocked-allows assertions fail. For the key path, clear the debug
// prefab identifier or break IdentifiersMatch and the configured-key assertions fail.

// tier: WORLD
class EL_Test_VehicleLock : EL_Test
{
	protected static const ResourceName LOCK_FIXTURE = "{EA882144DF2D4BEC}Prefabs/Vehicles/Test/VehicleLockFixture.et";
	protected static const ResourceName KEY_PREFAB = "{9D94B834AF514B4A}Prefabs/Items/Roleplay/VehicleKey.et";
	protected static const string DEBUG_VEHICLE_NAME = "Vehicle_LockTest";
	protected static const string DEBUG_MATCHING_KEY_NAME = "VehicleKey_Matching_Debug";
	protected static const string DEBUG_WRONG_KEY_NAME = "VehicleKey_Wrong_Debug";

	override string GetName()
	{
		return "vehiclelock/lock-key-access";
	}

	override void Run(EL_TestContext ctx)
	{
		World world = GetGame().GetWorld();
		IEntity debugVehicle = world.FindEntityByName(DEBUG_VEHICLE_NAME);
		ctx.NotNull(debugVehicle, "DebugWorld has the configured lock vehicle");
		if (ctx.FailureCount() > 0)
			return;

		EL_VehicleLockComponent debugVehicleLock = EL_Component<EL_VehicleLockComponent>.Find(debugVehicle);
		EL_VehicleStorageAccessControl debugAccessControl = EL_Component<EL_VehicleStorageAccessControl>.Find(debugVehicle);
		IEntity debugMatchingKey = world.FindEntityByName(DEBUG_MATCHING_KEY_NAME);
		IEntity debugWrongKey = world.FindEntityByName(DEBUG_WRONG_KEY_NAME);
		ctx.NotNull(debugVehicleLock, "debug vehicle carries EL_VehicleLockComponent");
		ctx.NotNull(debugAccessControl, "debug vehicle carries storage access control");
		ctx.NotNull(debugMatchingKey, "DebugWorld has the matching vehicle key");
		ctx.NotNull(debugWrongKey, "DebugWorld has the wrong vehicle key");
		if (ctx.FailureCount() > 0)
			return;

		EL_VehicleKeyComponent matchingKeyComponent = EL_Component<EL_VehicleKeyComponent>.Find(debugMatchingKey);
		EL_VehicleKeyComponent wrongKeyComponent = EL_Component<EL_VehicleKeyComponent>.Find(debugWrongKey);
		ctx.NotNull(matchingKeyComponent, "matching debug key carries EL_VehicleKeyComponent");
		ctx.NotNull(wrongKeyComponent, "wrong debug key carries EL_VehicleKeyComponent");
		if (ctx.FailureCount() > 0)
			return;

		debugVehicleLock.SetVehicleIdentifier("ELDebugKey1");
		matchingKeyComponent.BindToVehicleIdentifier("ELDebugKey1");
		wrongKeyComponent.BindToVehicleIdentifier("ELDebugKeyWrong");

		ctx.EqualStr("ELDebugKey1", debugVehicleLock.GetVehicleIdentifier(), "debug vehicle has its configured identifier");
		ctx.EqualStr("ELDebugKey1", matchingKeyComponent.GetVehicleIdentifier(), "matching debug key has its configured identifier");
		ctx.EqualStr("ELDebugKeyWrong", wrongKeyComponent.GetVehicleIdentifier(), "wrong debug key has a different configured identifier");
		ctx.False(debugVehicleLock.IsVehicleLocked(), "debug vehicle starts unlocked");
		ctx.False(debugAccessControl.IsStorageLocked(null), "debug vehicle storage starts accessible");
		ctx.True(debugVehicleLock.IsValidKey(debugMatchingKey), "matching debug key is accepted before locking");
		ctx.False(debugVehicleLock.IsValidKey(debugWrongKey), "wrong debug key is rejected before locking");

		debugVehicleLock.SetLocked(true);
		ctx.True(debugVehicleLock.IsVehicleLocked(), "debug vehicle locks");
		ctx.True(debugAccessControl.IsStorageLocked(null), "locked debug vehicle denies storage");
		ctx.True(debugVehicleLock.IsValidKey(debugMatchingKey), "matching key remains valid while locked");
		ctx.False(debugVehicleLock.IsValidKey(debugWrongKey), "wrong key remains rejected while locked");

		debugVehicleLock.ToggleLocked();
		ctx.False(debugVehicleLock.IsVehicleLocked(), "debug vehicle unlocks with ToggleLocked");
		ctx.False(debugAccessControl.IsStorageLocked(null), "unlocked debug vehicle allows storage again");

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
