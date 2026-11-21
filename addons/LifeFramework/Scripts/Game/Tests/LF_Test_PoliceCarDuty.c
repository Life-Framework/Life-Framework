// red-proof: 2026-11-21 `tools\cli test --no-build --red LF_Test_PoliceCarDuty` printed RED PROOF OK: RedProof() leaves
// the officer off duty, which is the unfixed behaviour (TryToggleBy refused every service vehicle and UserHasValidKey
// wanted a key nobody holds), so the on-duty unlock and start assertions fail.

//! #3875 follow-up (rc 0.2.0): the locked police cars outside the station open and start for an officer on duty,
//! and for nobody else. A civilian, or the same officer off duty, is still refused the lock and the engine.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PoliceCarDuty : LF_Test
{
	protected static const ResourceName MARKER = "{5D2B8E41A7C3906F}Prefabs/Systems/Vehicles/LF_PoliceCarMarker.et";
	protected static const string OFFICER = "police-car-duty-officer";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/police-car-duty";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ParkedCars.ResetStatics();
		LF_ParkedCarConfig.Reload();

		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer)
			return;

		IEntity markerEntity = LF_Utils.SpawnEntityPrefab(MARKER, officer.GetOrigin() + "4 0.5 0");
		LF_ParkedCarMarkerComponent marker;
		if (markerEntity)
			marker = LF_Component<LF_ParkedCarMarkerComponent>.Find(markerEntity);

		ctx.NotNull(marker, "the police marker spawns with its marker component");
		IEntity car;
		if (marker)
			car = marker.SpawnCar("test");

		ctx.True(car != null, "the police marker puts a car out");
		LF_VehicleLockComponent vehicleLock;
		if (car)
			vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);

		if (vehicleLock)
		{
			officer.SetOrigin(car.GetOrigin() + "2 0.1 0");
			LF_Utils.SetUidForTests(officer, OFFICER);
			LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(OFFICER);
			RunCar(ctx, vehicleLock, officer, account);
			if (account)
			{
				account.SetOnDuty(false);
				account.SetFaction(LF_Faction.CIVILIAN);
			}
		}

		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(car);
		LF_TestCharacterFixture.Delete(markerEntity);
		LF_TestCharacterFixture.Delete(officer);
		LF_ParkedCars.ResetStatics();
		LF_ParkedCarConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCar(LF_TestContext ctx, LF_VehicleLockComponent vehicleLock, IEntity officer, LF_PlayerAccount account)
	{
		ctx.True(vehicleLock.IsServiceVehicle(), "the police car is a service vehicle");
		ctx.True(vehicleLock.IsVehicleLocked(), "the police car stands locked");
		ctx.NotNull(account, "the officer has an account");
		if (!account)
			return;

		// Off duty, a civilian: refused.
		account.SetFaction(LF_Faction.CIVILIAN);
		account.SetOnDuty(false);
		ctx.False(vehicleLock.UserHasValidKey(officer), "a civilian holds no key to the police car");
		ctx.False(vehicleLock.TryToggleBy(officer, "test"), "a civilian cannot unlock the police car");
		ctx.True(vehicleLock.IsVehicleLocked(), "the police car is still locked");

		// On duty as police: the car is theirs to open and start. RedProof() leaves the officer off duty.
		if (!RedProof())
		{
			account.SetFaction(LF_Faction.POLICE);
			account.SetOnDuty(true);
		}

		ctx.True(vehicleLock.UserHasValidKey(officer), "an on-duty officer holds the police car's key");
		ctx.True(vehicleLock.TryToggleBy(officer, "test"), "an on-duty officer unlocks the police car");
		ctx.False(vehicleLock.IsVehicleLocked(), "the police car is unlocked");
		ctx.True(LF_Hotwire.MayStartEngine(vehicleLock.GetOwner(), officer), "an on-duty officer may start the police car");
		ctx.False(vehicleLock.IsStolen(), "the police car is not marked stolen");

		// Clocked out again: no key, the lock and the engine refuse.
		account.SetOnDuty(false);
		account.SetFaction(LF_Faction.CIVILIAN);
		ctx.False(vehicleLock.UserHasValidKey(officer), "off duty, the officer holds no key");
		ctx.False(vehicleLock.TryToggleBy(officer, "test"), "off duty, the officer cannot lock the police car");
	}
}
