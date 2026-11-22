// red-proof: 2026-11-22 `tools/cli test --no-build --only LF_Test_ParkedCarRestart` on the unfixed marker (a PutOut shim of the old
// FirstSpawn): FAIL `exactly one car stands on the marker (expected 1, got 2)`. Restart: `tools/cli wt e2e parked-car-restart` with
// the marker's Hold removed (and the single-boot IsSaved line muted) failed pass 2 `e2e the marker car from the save boot did not come
// back as a copy (expected 0, got 1)`; with the fix both grid boots pass (99/99).
//! A parked-car marker gives exactly one locked car after a restart (#4122). The engine's world save wrote every
//! marker car; at a restart it put the car back unlocked and the marker put out a second one on the same spot,
//! so anyone could drive the copy away. A marker car now stays out of the world save, and a marker whose spot
//! holds a car an older save restored adopts it (locked, out of the save) instead of adding one; asking twice
//! gives the same car. Across a restart (`tools\cli wt e2e`): the save boot leaves a marker car at spot A and a
//! saved unlocked car (what an older build wrote) at spot B; the reload boot finds nothing at A and exactly one
//! locked, unsaved car at each spot after the markers put out their cars, twice.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_ParkedCarRestart : LF_Test
{
	protected static const ResourceName MARKER = "{C48202E5BB6761C0}Prefabs/Systems/Vehicles/LF_ParkedCarMarker.et";
	//! The marker's home radius (LF_ParkedCarMarkerComponent.m_fHomeMetres default).
	protected static const float HOME = 8;
	//! Spots off the impound desk, clear of the gate and of LF_Test_ParkedCarMarker's spot (desk + 0 0 60).
	protected static const vector LOCAL_SPOT = "0 0 80";
	protected static const vector E2E_SPOT_A = "-14 0 100";
	protected static const vector E2E_SPOT_B = "14 0 100";

	override string GetName()
	{
		return "vehicles/parked-car-restart";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
#ifdef LF_E2E_LOAD_BOOT
		VerifyE2E(ctx);
		return;
#endif

		ctx.NotNull(PersistenceSystem.GetInstance(), "the world has the engine's persistence system");
		vector spot;
		if (!Spot(LOCAL_SPOT, spot))
		{
			ctx.True(false, "Annex_Impound_Desk is placed");
			return;
		}

		LF_ParkedCars.ResetStatics();
		LF_ParkedCarConfig.Reload();

		// A marker's own car never enters the world save.
		IEntity markerEntity = LF_Utils.SpawnEntityPrefab(MARKER, spot);
		LF_ParkedCarMarkerComponent marker = LF_Component<LF_ParkedCarMarkerComponent>.Find(markerEntity);
		ctx.NotNull(marker, "the marker spawns with its component");
		if (!marker)
		{
			LF_TestCharacterFixture.Delete(markerEntity);
			return;
		}

		IEntity car = marker.SpawnCar("test");
		ctx.NotNull(car, "the marker puts out a car");
		ctx.False(LF_VehicleWorldSave.IsSaved(car), "a marker car is not in the engine's world save, so a restart puts no copy back");
		LF_TestCharacterFixture.Delete(car);
		LF_TestCharacterFixture.Delete(markerEntity);

		// What a save from an older build restores: an unlocked car on the spot, tracked by the world save.
		// A tracked car of another prefab beside it (a layer car) is never taken.
		IEntity restored = SpawnRestoredCopy(spot);
		ctx.True(LF_VehicleWorldSave.IsSaved(restored), "a plain spawned car is tracked, as a restored one is");
		IEntity bystander = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.M923, spot + "0 0.3 5");
		ctx.True(LF_VehicleWorldSave.IsSaved(bystander), "the car of another prefab beside the marker is tracked too");
		LF_ParkedCars.AssumeSaveLoaded();
		markerEntity = LF_Utils.SpawnEntityPrefab(MARKER, spot);
		marker = LF_Component<LF_ParkedCarMarkerComponent>.Find(markerEntity);
		if (marker)
		{
			CheckOneLockedCar(ctx, marker, spot, "", 2);
			ctx.True(marker.GetCar() == restored, "the marker adopted the restored copy, not the car of another prefab beside it");
		}

		LF_TestCharacterFixture.Delete(bystander);
		DeleteCarsAt(spot);
		LF_TestCharacterFixture.Delete(markerEntity);
		LF_ParkedCars.ResetStatics();

#ifdef LF_E2E_HOST_STOP
		SeedE2E(ctx);
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! The marker puts out its car twice; exactly one car stands on the spot, locked and out of the save.
	//! \param expected Vehicles on the spot afterwards: the marker's car, plus a bystander when the test placed one.
	protected static void CheckOneLockedCar(LF_TestContext ctx, LF_ParkedCarMarkerComponent marker, vector spot, string prefix, int expected = 1)
	{
		IEntity first = marker.PutOut("test");
		IEntity second = marker.PutOut("test again");
		ctx.NotNull(first, prefix + "the marker has a car");
		ctx.True(first == second, prefix + "asking the marker twice gives the same car");
		ctx.Equal(expected, LF_Utils.GetNearbyVehicles(spot, HOME).Count(), prefix + "exactly one car stands on the marker");
		if (!first)
			return;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(first);
		ctx.True(vehicleLock && vehicleLock.IsVehicleLocked(), prefix + "the marker's car is locked");
		ctx.False(LF_VehicleWorldSave.IsSaved(first), prefix + "the marker's car is out of the world save");
	}

	//------------------------------------------------------------------------------------------------
	//! An unlocked car with a lock component, tracked by the world save: what an older build's save puts back.
	protected static IEntity SpawnRestoredCopy(vector spot)
	{
		LF_ParkedCarConfig config = LF_ParkedCarConfig.Get();
		if (!config || !config.m_aCars || config.m_aCars.IsEmpty())
			return null;

		IEntity car = LF_Utils.SpawnEntityPrefab(config.m_aCars[0].m_sPrefab, spot + "0 0.3 0");
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);
		if (vehicleLock)
			vehicleLock.SetLocked(false);

		return car;
	}

	//------------------------------------------------------------------------------------------------
	//! \param offset From the impound desk; the ground height is taken under it.
	//! \return False when this world has no annex impound desk.
	protected static bool Spot(vector offset, out vector spot)
	{
		IEntity desk = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.IMPOUND_DESK);
		if (!desk)
			return false;

		spot = desk.GetOrigin() + offset;
		spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void DeleteCarsAt(vector spot)
	{
		array<IEntity> vehicles = LF_Utils.GetNearbyVehicles(spot, HOME);
		foreach (IEntity vehicle : vehicles)
		{
			LF_TestCharacterFixture.Delete(vehicle);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: a marker car at spot A, and a saved unlocked car at spot B, when the shutdown save runs.
	static void SeedE2E(LF_TestContext ctx)
	{
		vector spotA;
		vector spotB;
		if (!Spot(E2E_SPOT_A, spotA) || !Spot(E2E_SPOT_B, spotB))
		{
			LF_Debug.Info(LF_ParkedCarConfig.FEATURE, "e2e parked-car restart not seeded: this world has no annex impound");
			return;
		}

		IEntity markerEntity = LF_Utils.SpawnEntityPrefab(MARKER, spotA);
		LF_ParkedCarMarkerComponent marker = LF_Component<LF_ParkedCarMarkerComponent>.Find(markerEntity);
		IEntity car;
		if (marker)
			car = marker.PutOut("e2e seed");

		ctx.NotNull(car, "e2e the marker at spot A puts out a car before the save");
		LF_TestDebrisProbe.Keep(car);
		LF_TestCharacterFixture.Delete(markerEntity);

		IEntity restored = SpawnRestoredCopy(spotB);
		ctx.True(LF_VehicleWorldSave.IsSaved(restored), "e2e the unlocked car at spot B is in the world save");
		LF_TestDebrisProbe.Keep(restored);
		LF_ParkedCars.ResetStatics();
		LF_Debug.Info(LF_ParkedCarConfig.FEATURE, string.Format("e2e parked-car restart seeded: marker car at %1, saved unlocked car at %2", spotA, spotB));
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: nothing came back at spot A; the saved car at spot B is adopted; one locked car each.
	static void VerifyE2E(LF_TestContext ctx)
	{
		vector spotA;
		vector spotB;
		bool placed = Spot(E2E_SPOT_A, spotA) && Spot(E2E_SPOT_B, spotB);
		if (!LF_E2E.RequiresSeed("parked-car restart cars (#4122)", placed && !LF_Utils.GetNearbyVehicles(spotB, HOME).IsEmpty()))
			return;

		ctx.Equal(0, LF_Utils.GetNearbyVehicles(spotA, HOME).Count(), "e2e the marker car from the save boot did not come back as a copy");
		ctx.Equal(1, LF_Utils.GetNearbyVehicles(spotB, HOME).Count(), "e2e the saved car at spot B came back once");

		LF_ParkedCars.ResetStatics();
		array<IEntity> markers = {};
		markers.Insert(LF_Utils.SpawnEntityPrefab(MARKER, spotA));
		markers.Insert(LF_Utils.SpawnEntityPrefab(MARKER, spotB));
		LF_ParkedCarMarkerComponent markerA = LF_Component<LF_ParkedCarMarkerComponent>.Find(markers[0]);
		LF_ParkedCarMarkerComponent markerB = LF_Component<LF_ParkedCarMarkerComponent>.Find(markers[1]);
		if (markerA && markerB)
		{
			CheckOneLockedCar(ctx, markerA, spotA, "e2e spot A: ");
			CheckOneLockedCar(ctx, markerB, spotB, "e2e spot B: ");
			LF_Debug.Info(LF_ParkedCarConfig.FEATURE, string.Format("e2e restart: %1 car(s) at spot A, %2 at spot B", LF_Utils.GetNearbyVehicles(spotA, HOME).Count(), LF_Utils.GetNearbyVehicles(spotB, HOME).Count()));
		}

		DeleteCarsAt(spotA);
		DeleteCarsAt(spotB);
		foreach (IEntity markerEntity : markers)
		{
			LF_TestCharacterFixture.Delete(markerEntity);
		}

		LF_ParkedCars.ResetStatics();
	}
}
