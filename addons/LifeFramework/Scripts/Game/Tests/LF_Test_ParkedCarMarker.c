// red-proof: RedProof() skips the sweep that respawns, so `the sweep past the wait puts a second car out` fails (run `tools\cli test --no-build --red LF_Test_ParkedCarMarker`).

//! #3365: a parked-car marker spawns one locked car, puts a fresh one out when that car has been gone for
//! m_iRespawnMinutes, and holds back while a player is within 100 m. Every car and the marker are deleted at
//! the end.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ParkedCarMarker : LF_Test
{
	protected static const ResourceName MARKER = "{C48202E5BB6761C0}Prefabs/Systems/Vehicles/LF_ParkedCarMarker.et";
	protected static const int T0 = 1800000000;
	protected static const int WAIT = 1800; // the marker's default, 30 minutes

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/parked-car-marker";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ParkedCars.ResetStatics();
		LF_ParkedCarConfig.Reload();
		LF_ParkedCarConfig config = LF_ParkedCarConfig.Get();
		ctx.True(config != null && config.m_aCars.Count() > 0, "the core parked car list has cars");

		IEntity desk = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.IMPOUND_DESK);
		ctx.True(desk != null, "Annex_Impound_Desk is placed");
		if (!config || !desk)
			return;

		vector spot = desk.GetOrigin() + "0 0 60";
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		IEntity markerEntity = LF_Utils.SpawnEntityPrefab(MARKER, spot);
		ctx.True(markerEntity != null, "the marker prefab spawns");
		if (!markerEntity)
			return;

		array<IEntity> cars = {};
		LF_ParkedCarMarkerComponent marker = LF_Component<LF_ParkedCarMarkerComponent>.Find(markerEntity);
		ctx.NotNull(marker, "the marker carries its component");
		if (marker)
			RunMarker(ctx, marker, markerEntity, cars);

		// A test deletes what it spawns.
		foreach (IEntity car : cars)
		{
			LF_TestCharacterFixture.Delete(car);
		}

		LF_TestCharacterFixture.Delete(markerEntity);
		LF_ParkedCars.ResetStatics();
		LF_ParkedCarConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunMarker(LF_TestContext ctx, LF_ParkedCarMarkerComponent marker, IEntity markerEntity, array<IEntity> cars)
	{
		// 1. The marker spawns a locked car.
		IEntity first = marker.SpawnCar("test");
		ctx.True(first != null, "the marker spawns a car");
		if (!first)
			return;

		cars.Insert(first);
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(first);
		ctx.NotNull(vehicleLock, "the parked car carries the normal lock");
		if (vehicleLock)
			ctx.True(vehicleLock.IsVehicleLocked(), "the parked car is locked");

		ctx.Equal(1, marker.SpawnCount(), "one car so far");
		ctx.Equal(0, LF_ParkedCars.Sweep(T0), "a car at home is left alone");
		ctx.Equal(0, LF_ParkedCars.Sweep(T0 + WAIT * 2), "a car at home stays left alone however long it takes");

		// 2. Take the car 200 m away: the clock starts, then the wait passes and a fresh car comes out.
		first.SetOrigin(first.GetOrigin() + "200 0 0");
		first.Update();
		ctx.Equal(0, LF_ParkedCars.Sweep(T0), "the first sweep after the car left only starts the clock");
		ctx.Equal(0, LF_ParkedCars.Sweep(T0 + WAIT - 1), "a second before the wait nothing comes out");

		// RedProof(): the sweep past the wait is skipped, so the respawn assertions fail.
		int respawned = 0;
		if (!RedProof())
			respawned = LF_ParkedCars.Sweep(T0 + WAIT);

		ctx.Equal(1, respawned, "the sweep past the wait puts a second car out");
		ctx.Equal(2, marker.SpawnCount(), "the marker has put out two cars");
		IEntity second = marker.GetCar();
		ctx.True(second != null && second != first, "the marker's car is a new one");
		if (second && second != first)
		{
			cars.Insert(second);
			ctx.True(vector.Distance(second.GetOrigin(), markerEntity.GetOrigin()) < 5, "the fresh car stands at the marker");
			LF_VehicleLockComponent secondLock = LF_Component<LF_VehicleLockComponent>.Find(second);
			ctx.True(secondLock != null && secondLock.IsVehicleLocked(), "the fresh car is locked too");
		}

		ctx.True(GetGame().GetWorld().FindEntityByID(first.GetID()) != null, "the taken car is left alone in the world");

		// 3. A player within 100 m holds the respawn back.
		if (!second)
			return;

		second.SetOrigin(second.GetOrigin() + "200 0 0");
		second.Update();
		int later = T0 + WAIT * 4;
		ctx.Equal(0, LF_ParkedCars.Sweep(later), "the second car left: the clock starts");
		LF_ParkedCars.AddStandIn(markerEntity.GetOrigin() + "50 0 0");
		ctx.Equal(0, LF_ParkedCars.Sweep(later + WAIT), "with a player 50 m away no third car comes out");
		ctx.Equal(2, marker.SpawnCount(), "still two cars put out");
	}
}
