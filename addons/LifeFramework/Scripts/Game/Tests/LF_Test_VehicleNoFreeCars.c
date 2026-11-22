// red-proof: 2026-11-22 `tools\cli test --no-build --red LF_Test_VehicleNoFreeCars` printed RED PROOF OK: with the job driver
// and the service flag skipped the truck and the unlocked police car let the stranger in, the unfixed behaviour.

//! No free cars (2026-11-22 island playtest: "a bunch of unlocked cars that one could steal"). One entry rule,
//! LF_VehicleLockComponent.MayEnter, that the client's get-in offer and the server's entry guard share:
//! - a locked car refuses a stranger and says "Vehicle is locked"; unlocked by its owner, it opens;
//! - a police car refuses a civilian even when it stands unlocked, and opens for an officer on duty even locked;
//! - a job vehicle (the garbage truck, the depot tanker) opens for its job driver only, unlocked or not;
//! - the company truck prefab carries the lock, so the garbage round can hand it to one player.
//! The server guard's rule (driver's seat of a player only; cargo seats a patient or suspect is put in stay) is
//! checked as a table.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_VehicleNoFreeCars : LF_Test
{
	protected static const ResourceName TRUCK = "{2B402516452EC797}Prefabs/Vehicles/Wheeled/Ural4320/LF_Ural4320_transport_CIV.et";
	protected static const ResourceName POLICE_MARKER = "{5D2B8E41A7C3906F}Prefabs/Systems/Vehicles/LF_PoliceCarMarker.et";
	protected static const string OFFICER = "no-free-cars-officer";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/no-free-cars";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunRules(ctx);

		IEntity stranger = LF_TestCharacterFixture.Spawn(ctx);
		if (!stranger)
			return;

		IEntity truck = LF_Utils.SpawnEntityPrefab(TRUCK, stranger.GetOrigin() + "6 0.3 0");
		RunTruck(ctx, truck, stranger);
		LF_TestCharacterFixture.Delete(truck);

		RunPoliceCar(ctx, stranger);
		LF_TestCharacterFixture.Delete(stranger);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRules(LF_TestContext ctx)
	{
		// EntryRule(systemOn, locked, service, onDutyPolice, jobDriverId, userPlayerId, pilot)
		ctx.False(LF_VehicleLockComponent.EntryRule(true, true, false, false, -1, 3, true), "a locked car refuses a stranger");
		ctx.True(LF_VehicleLockComponent.EntryRule(true, false, false, false, -1, 3, true), "an unlocked car opens");
		ctx.False(LF_VehicleLockComponent.EntryRule(true, false, true, false, -1, 3, false), "an unlocked police car refuses a civilian, any seat");
		ctx.True(LF_VehicleLockComponent.EntryRule(true, true, true, true, -1, 3, true), "a locked police car opens for an officer on duty");
		ctx.False(LF_VehicleLockComponent.EntryRule(true, false, false, false, 7, 3, true), "a job car's wheel refuses another player");
		ctx.False(LF_VehicleLockComponent.EntryRule(true, false, false, false, 7, 0, true), "a job car's wheel refuses a body with no player");
		ctx.True(LF_VehicleLockComponent.EntryRule(true, true, false, false, 7, 7, true), "a job car's wheel is its job driver's, locked or not");
		ctx.True(LF_VehicleLockComponent.EntryRule(true, false, false, false, 7, 3, false), "a taxi fare boards the job car's back seat");
		ctx.True(LF_VehicleLockComponent.EntryRule(false, true, true, false, 7, 3, true), "with VehicleLock off every door opens (#1076)");

		// Refuses(isPlayer, pilot, hasLock, allowed)
		ctx.True(LF_VehicleEntryGuard.Refuses(true, true, true, false), "the server puts a refused player out of the driver's seat");
		ctx.False(LF_VehicleEntryGuard.Refuses(true, false, true, false), "a cargo seat (a carried patient, a suspect) is left alone");
		ctx.False(LF_VehicleEntryGuard.Refuses(false, true, true, false), "a non-player crew is left alone");
		ctx.False(LF_VehicleEntryGuard.Refuses(true, true, false, false), "a car with no LF lock is not this guard's");
		ctx.False(LF_VehicleEntryGuard.Refuses(true, true, true, true), "an allowed driver stays");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunTruck(LF_TestContext ctx, IEntity truck, IEntity stranger)
	{
		ctx.NotNull(truck, "the company truck spawns");
		LF_VehicleLockComponent truckLock;
		if (truck)
			truckLock = LF_Component<LF_VehicleLockComponent>.Find(truck);

		ctx.NotNull(truckLock, "the company truck carries LF_VehicleLockComponent");
		if (!truckLock)
			return;

		ctx.True(truck.FindComponent(SCR_BaseLockComponent) == truckLock, "the lock replaces the vanilla lock, so the get-in action asks it");

		// A locked car: the stranger is refused, with the reason the get-in action shows.
		truckLock.SetLocked(true);
		ctx.True(truckLock.IsLocked(stranger, null), "a locked car refuses a stranger");
		ctx.EqualStr("#EL-VehicleLock_Reason", truckLock.GetCannotPerformReason(stranger), "the refusal says the car is locked");

		// Its owner unlocks it with the key: it opens.
		truckLock.SetLocked(false);
		ctx.False(truckLock.IsLocked(stranger, null), "unlocked by its owner, the car opens");

		// A job vehicle: unlocked or not, only the job driver (player 7 here; the fixture is no player).
		if (!RedProof())
			truckLock.SetJobDriver(7);

		ctx.Equal(7, truckLock.GetJobDriverPlayerId(), "the job driver is recorded");
		ctx.False(truckLock.IsVehicleLocked(), "handed to a job driver, the doors unlock for riders");
		ctx.True(truckLock.IsLocked(stranger, null), "the job truck's wheel refuses anyone but its job driver");

		truckLock.SetJobDriver(0);
		ctx.Equal(-1, truckLock.GetJobDriverPlayerId(), "a driver id of 0 ends the job hold");
		ctx.True(truckLock.IsVehicleLocked(), "with the job over, the truck locks again");
		ctx.True(truckLock.IsLocked(stranger, null), "and refuses the stranger");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPoliceCar(LF_TestContext ctx, IEntity person)
	{
		LF_ParkedCars.ResetStatics();
		LF_ParkedCarConfig.Reload();
		IEntity markerEntity = LF_Utils.SpawnEntityPrefab(POLICE_MARKER, person.GetOrigin() + "-6 0.5 0");
		LF_ParkedCarMarkerComponent marker;
		if (markerEntity)
			marker = LF_Component<LF_ParkedCarMarkerComponent>.Find(markerEntity);

		IEntity car;
		if (marker)
			car = marker.SpawnCar("test");

		LF_VehicleLockComponent carLock;
		if (car)
			carLock = LF_Component<LF_VehicleLockComponent>.Find(car);

		ctx.NotNull(carLock, "the police marker puts out a locked service car");
		LF_Utils.SetUidForTests(person, OFFICER);
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(OFFICER);
		if (carLock && account)
		{
			account.SetFaction(LF_Faction.CIVILIAN);
			account.SetOnDuty(false);

			// Left unlocked by an officer: still no civilian's car.
			carLock.SetLocked(false);
			if (RedProof())
				carLock.SetServiceVehicle(false);

			ctx.True(carLock.IsLocked(person, null), "an unlocked police car refuses a civilian");

			// On duty: it opens for them, locked or not.
			carLock.SetServiceVehicle(true);
			carLock.SetLocked(true);
			account.SetFaction(LF_Faction.POLICE);
			account.SetOnDuty(true);
			ctx.False(carLock.IsLocked(person, null), "a locked police car opens for an officer on duty");

			account.SetOnDuty(false);
			account.SetFaction(LF_Faction.CIVILIAN);
		}

		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(car);
		LF_TestCharacterFixture.Delete(markerEntity);
		LF_ParkedCars.ResetStatics();
		LF_ParkedCarConfig.Reload();
	}
}
