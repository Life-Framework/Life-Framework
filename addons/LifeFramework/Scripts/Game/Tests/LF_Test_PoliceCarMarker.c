// red-proof: RedProof() leaves the service flag off, so `the police car is a service vehicle`, the refused key toggle and the refused chop fail (run `tools\cli test --no-build --red LF_Test_PoliceCarMarker`).

//! #3875: a police-car marker puts out the locked police UAZ and marks it a service vehicle: no civilian key,
//! lockpick, hotwire or chop shop takes it, even when it is flagged stolen and stands in the chop bench's drop
//! area. It is nobody's record, so the garage cannot store it. The marker's car comes back through the normal
//! parked-car sweep, which the world-start spawn repeats after a restart. Every entity is deleted at the end.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PoliceCarMarker : LF_Test
{
	protected static const ResourceName MARKER = "{5D2B8E41A7C3906F}Prefabs/Systems/Vehicles/LF_PoliceCarMarker.et";
	protected static const ResourceName POLICE_CAR = "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/police-car-marker";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ParkedCars.ResetStatics();
		LF_ParkedCarConfig.Reload();

		IEntity bench = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.CHOP_SHOP);
		LF_ChopShopComponent shop = LF_Component<LF_ChopShopComponent>.Find(bench);
		ctx.NotNull(shop, "the playtest chop bench is placed");
		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		if (!shop || !worker)
		{
			LF_TestCharacterFixture.Delete(worker);
			return;
		}

		vector yard = bench.GetOrigin();
		worker.SetOrigin(yard + "0 0.1 1.5");
		IEntity markerEntity = LF_Utils.SpawnEntityPrefab(MARKER, yard + "3 0.5 0");
		ctx.True(markerEntity != null, "the police marker prefab spawns");
		LF_ParkedCarMarkerComponent marker;
		if (markerEntity)
			marker = LF_Component<LF_ParkedCarMarkerComponent>.Find(markerEntity);

		ctx.NotNull(marker, "the police marker carries the parked-car marker component");
		IEntity car;
		if (marker)
			car = marker.SpawnCar("test");

		ctx.True(car != null, "the police marker puts a car out");
		if (car)
			RunCar(ctx, car, worker, shop);

		// A test deletes what it spawns.
		LF_TestCharacterFixture.Delete(car);
		LF_TestCharacterFixture.Delete(markerEntity);
		LF_TestCharacterFixture.Delete(worker);
		LF_ParkedCars.ResetStatics();
		LF_ParkedCarConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCar(LF_TestContext ctx, IEntity car, IEntity worker, LF_ChopShopComponent shop)
	{
		ctx.True(LF_Utils.SamePrefab(LF_Utils.GetPrefabName(car), POLICE_CAR), "the marker's car is the police UAZ");
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);
		ctx.NotNull(vehicleLock, "the police car carries the normal lock");
		if (!vehicleLock)
			return;

		// RedProof(): the service flag is left off, so the refusals below fail.
		if (RedProof())
			vehicleLock.SetServiceVehicle(false);

		ctx.True(vehicleLock.IsVehicleLocked(), "the police car is locked");
		ctx.True(vehicleLock.IsServiceVehicle(), "the police car is a service vehicle");
		ctx.True(LF_VehicleOwnership.GetOwner(vehicleLock.GetVehicleIdentifier()).IsEmpty(), "nobody owns the police car");
		ctx.False(LF_VehicleOwnership.IsOwnedEntity(car), "the garage and the lot see no owner");

		// No key, no pick, no wire.
		ctx.False(vehicleLock.TryToggleBy(worker, "test"), "a civilian cannot toggle the police car's lock");
		ctx.True(vehicleLock.IsVehicleLocked(), "the police car is still locked");
		ctx.False(LF_Lockpick.ShowsPick(car), "no pick action is listed on the police car");
		ctx.True(LF_Lockpick.CheckDoor(car, worker) != LF_ELockpickCheck.NONE, "no pick attempt starts on the police car");
		ctx.False(LF_Hotwire.ShowsHotwire(car, worker), "no hotwire action is listed on the police car");
		ctx.Equal(LF_EHotwireCheck.NO_IGNITION_LOCK, LF_Hotwire.CheckVehicle(car, worker), "no hotwire attempt starts on the police car");

		// Flagged stolen and standing in the drop area, it is still no car to chop.
		vehicleLock.SetStolen(true);
		// A stolen car in the drop area is the one the bench loads, so a load that is refused means the police car was skipped
		// (another car of the yard may stand there, which is why the refusal is not pinned to one reason).
		ctx.True(shop.ServerLoad(worker, 100000) != LF_EChopCheck.NONE, "the chop bench does not take the police car");
		ctx.Equal(LF_StationProgress.EMPTY, shop.GetPercent(), "no job started on the police car");
		ctx.True(GetGame().GetWorld().FindEntityByID(car.GetID()) != null, "the police car is still there");
		ctx.True(vehicleLock.IsVehicleLocked(), "the bench did not unlock it");
	}
}
