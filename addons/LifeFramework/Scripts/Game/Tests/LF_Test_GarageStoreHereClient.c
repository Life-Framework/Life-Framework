// red-proof: observed 2026-11-18 (tools\cli test --no-build --red LF_Test_GarageStoreHereClient, RED-OK, 12 assertions): with the
// RedProof() hook on the lock never gets its replicated owner key, and "the owner's replicated key opens the
// offer" fails. On main before the fix the class did not compile (no OwnedIdFor / SetOwnerUid).

//! #3366: the car's own "Store in garage" row is decided on a client, where LF_VehicleOwnership's tables are
//! empty. The owner key the lock replicates must open the offer for its owner and for nobody else.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_GarageStoreHereClient : LF_Test
{
	protected static const ResourceName LOCK_FIXTURE = "{EA882144DF2D4BEC}Prefabs/Vehicles/Test/VehicleLockFixture.et";
	protected static const string OWNER_UID = "test-garage-here-owner";
	protected static const string STRANGER_UID = "test-garage-here-stranger";
	protected static const string VEHICLE_ID = "test-garage-here-vehicle";

	override string GetName()
	{
		return "vehicles/garage-store-here-client";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource res = Resource.Load(LOCK_FIXTURE);
		ctx.True(res.IsValid(), "lock fixture prefab loads: " + LOCK_FIXTURE);
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity vehicle = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(vehicle, "lock fixture spawns a vehicle");
		if (ctx.FailureCount() > 0)
			return;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
		ctx.NotNull(vehicleLock, "fixture carries LF_VehicleLockComponent");
		if (ctx.FailureCount() > 0)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return;
		}

		vehicleLock.SetVehicleIdentifier(VEHICLE_ID);
		ctx.EqualStr("", vehicleLock.GetOwnerUid(), "a car nobody owns carries no owner key");
		ctx.EqualStr("", LF_VehicleStoreHereAction.OwnedIdFor(vehicle, OWNER_UID, false), "the client branch offers an unowned car to nobody");

		if (!RedProof())
			vehicleLock.SetOwnerUid(OWNER_UID);

		// useTables=false is the client branch: no ownership table is read, only what the lock replicated.
		ctx.EqualStr(OWNER_UID, vehicleLock.GetOwnerUid(), "the lock carries the owner's key");
		ctx.EqualStr(VEHICLE_ID, LF_VehicleStoreHereAction.OwnedIdFor(vehicle, OWNER_UID, false), "the owner's replicated key opens the offer");
		ctx.EqualStr("", LF_VehicleStoreHereAction.OwnedIdFor(vehicle, STRANGER_UID, false), "a stranger's key does not open the offer");
		ctx.EqualStr("", LF_VehicleStoreHereAction.OwnedIdFor(vehicle, "", false), "a player with no key yet does not open the offer");

		// The server branch still reads the tables, which know nothing of this test car.
		ctx.EqualStr("", LF_VehicleStoreHereAction.OwnedIdFor(vehicle, OWNER_UID, true), "the server branch ignores the replicated key");

		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
	}
}
