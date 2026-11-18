// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_VehicleTow` (the sweep past the delay skipped):
// `RED-OK vehicles/tow (fails with the hook on, 45 assertions)`: `only the abandoned car is towed (expected 1, got 0)`,
// `the record reads TOWED`, `the desk asks the tow fee for a towed car (expected 150, got 250)`. Green without the hook.

//! Abandoned cars are towed (#2657): an owned car left past towAfterMinutes leaves the street,
//! reads TOWED with its towedAt, and the impound desk releases it for the tow fee. A car someone
//! sits in, its owner standing by, a police car, a car by a garage, a stolen car and a job car
//! (not an owned record) are never towed.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_VehicleTow : LF_Test
{
	protected static const string OWNER = "vehicle-tow-owner";
	protected static const string LEFT = "vehicle-tow-left";
	protected static const string STOLEN = "vehicle-tow-stolen";
	protected static const string GARAGED = "vehicle-tow-garage";
	protected static const string LEFT_PLATE = "TW LF 1001";
	protected static const string STOLEN_PLATE = "TW ST 2002";
	protected static const string GARAGED_PLATE = "TW GR 3003";
	protected static const int T0 = 1800000000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/tow";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int delay = LF_GarageConfig.TowAfterMinutes() * 60;
		ctx.Equal(3600, delay, "the tow delay is 60 minutes");
		ctx.Equal(150, LF_GarageConfig.TowFee(), "the tow fee is 150");

		// The rule: only an unattended, unexempt car past the delay is towed.
		int late = T0 + delay;
		ctx.True(LF_VehicleTowRules.Check(false, false, false, false, false, T0, late, delay) == LF_ETowCheck.TOW, "an abandoned car past the delay is towed");
		ctx.True(LF_VehicleTowRules.Check(false, false, false, false, false, T0, late - 1, delay) == LF_ETowCheck.WAITING, "a second short of the delay waits");
		ctx.True(LF_VehicleTowRules.Check(false, false, false, false, false, 0, late, delay) == LF_ETowCheck.WAITING, "a car first seen unattended starts its clock");
		ctx.True(LF_VehicleTowRules.Check(true, false, false, false, false, T0, late, delay) == LF_ETowCheck.ATTENDED, "a car with its owner seated is never towed");
		ctx.True(LF_VehicleTowRules.Check(false, true, false, false, false, T0, late, delay) == LF_ETowCheck.ATTENDED, "a car with its owner within 50 m is never towed");
		ctx.True(LF_VehicleTowRules.Check(false, false, true, false, false, T0, late, delay) == LF_ETowCheck.EXEMPT, "a police car is never towed");
		ctx.True(LF_VehicleTowRules.Check(false, false, false, true, false, T0, late, delay) == LF_ETowCheck.EXEMPT, "a car by a garage is never towed");
		ctx.True(LF_VehicleTowRules.Check(false, false, false, false, true, T0, late, delay) == LF_ETowCheck.EXEMPT, "a stolen car is never towed");

		ctx.True(LF_VehicleSpawnPointComponent.CountForShop(LF_TestAnnex.IMPOUND_PAD_ID) > 0, "the impound yard has a pad");
		ctx.True(LF_VehicleSpawnPointComponent.CountForShop(LF_TestAnnex.GARAGE_PAD_ID) > 0, "the garage has a pad");
		IEntity desk = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.IMPOUND_DESK);
		ctx.True(desk != null, "Annex_Impound_Desk is placed");
		if (ctx.FailureCount() > 0)
			return;

		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		if (!owner)
			return;

		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		LF_VehicleTow.ResetStatics();
		LF_VehicleOwnership.Register(LEFT, OWNER, LEFT, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, LF_TestAnnex.LOT_ID, LEFT_PLATE, "#EL-Vehicle_S105", true);
		LF_VehicleOwnership.Register(STOLEN, OWNER, STOLEN, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, LF_TestAnnex.LOT_ID, STOLEN_PLATE, "#EL-Vehicle_S105", true);
		LF_VehicleOwnership.Register(GARAGED, OWNER, GARAGED, LF_VehicleCatalog.S105, LF_VehicleCatalog.S105_LOCKED, LF_TestAnnex.LOT_ID, GARAGED_PLATE, "#EL-Vehicle_S105", true);

		// Put the cars in the world: two out on the street away from any garage, one on the garage pad.
		string reasonKey;
		IEntity left = PlaceOnStreet(ctx, LEFT, owner, desk.GetOrigin(), 60);
		IEntity stolen = PlaceOnStreet(ctx, STOLEN, owner, desk.GetOrigin(), 90);
		ctx.True(LF_VehicleOwnership.CollectFromImpound(GARAGED, OWNER, owner, 0, LF_TestAnnex.GARAGE_PAD_ID, reasonKey), "a car stands on the garage pad");
		IEntity garaged = LF_VehicleOwnership.GetVehicle(GARAGED);
		IEntity jobCar = LF_Utils.SpawnEntityPrefab(LF_VehicleCatalog.S105, desk.GetOrigin() + "0 0 120");
		ctx.True(jobCar != null, "a job car (not an owned record) stands on the street");
		if (left && stolen && garaged && jobCar)
		{
			ctx.False(LF_VehicleTow.NearGarage(left.GetOrigin(), LF_GarageConfig.TowExemptNearGarage()), "the left car is away from every garage");
			ctx.True(LF_VehicleTow.NearGarage(garaged.GetOrigin(), LF_GarageConfig.TowExemptNearGarage()), "the garage-pad car is within the exempt radius");
			LF_VehicleLockComponent stolenLock = LF_Component<LF_VehicleLockComponent>.Find(stolen);
			ctx.NotNull(stolenLock, "the stolen car has a lock");
			if (stolenLock)
				stolenLock.SetStolen(true);

			ctx.Equal(0, LF_VehicleTow.Sweep(T0), "the first sweep only starts the clocks");
			ctx.Equal(0, LF_VehicleTow.Sweep(late - 1), "a second before the delay nothing is towed");

			// RedProof(): the sweep past the delay is skipped, so the tow assertions fail.
			int towed = 0;
			if (!RedProof())
				towed = LF_VehicleTow.Sweep(late);

			ctx.Equal(1, towed, "only the abandoned car is towed");
			ctx.True(LF_VehicleOwnership.GetVehicle(LEFT) == null, "the towed car is off the street");
			ctx.True(LF_VehicleOwnership.IsImpounded(LEFT), "the towed car waits at the impound lot");
			LF_VehicleStateRow state = LF_VehicleOwnership.StateOf(LEFT);
			ctx.True(state && state.m_iTowState == LF_EVehicleTowState.TOWED, "the record reads TOWED");
			if (state)
				ctx.Equal(late, state.m_iTowedAt, "the record keeps when it was towed");

			ctx.True(LF_VehicleOwnership.GetVehicle(STOLEN) != null, "the stolen car is not towed");
			ctx.True(LF_VehicleOwnership.GetVehicle(GARAGED) != null, "the car by the garage is not towed");
			ctx.True(GetGame().GetWorld().FindEntityByID(jobCar.GetID()) != null, "the job car is not towed");

			// The desk releases it for the tow fee.
			ctx.Equal(150, LF_VehicleTow.FeeOf(LEFT), "the desk asks the tow fee for a towed car");
			LF_MoneyUtils.AddCash(owner, 149);
			ctx.False(LF_VehicleTow.Collect(LEFT, OWNER, owner, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "short cash does not release the towed car");
			LF_MoneyUtils.AddCash(owner, 51);
			int cashBefore = LF_MoneyUtils.GetCash(owner);
			ctx.True(LF_VehicleTow.Collect(LEFT, OWNER, owner, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), "the owner pays and collects the towed car");
			ctx.Equal(cashBefore - 150, LF_MoneyUtils.GetCash(owner), "the tow fee is taken from carried cash");
			ctx.True(LF_VehicleOwnership.GetVehicle(LEFT) != null, "the released car is in the yard");
			LF_VehicleStateRow released = LF_VehicleOwnership.StateOf(LEFT);
			ctx.True(released && released.m_iTowState == LF_EVehicleTowState.NONE, "a released car is no longer TOWED");
			ctx.Equal(LF_GarageConfig.ImpoundFee(), LF_VehicleTow.FeeOf(LEFT), "an untowed car is back on the impound fee");
		}

		// Cleanup.
		RemoveCar(LEFT);
		RemoveCar(STOLEN);
		RemoveCar(GARAGED);
		if (jobCar)
			SCR_EntityHelper.DeleteEntityAndChildren(jobCar);

		LF_VehicleTow.ResetStatics();
		LF_TestCharacterFixture.Delete(owner);
		LF_VehicleOwnership.ApplyAll(before);
		LF_LicensePlateRegistry.Release(LEFT_PLATE);
		LF_LicensePlateRegistry.Release(STOLEN_PLATE);
		LF_LicensePlateRegistry.Release(GARAGED_PLATE);
	}

	//------------------------------------------------------------------------------------------------
	//! Releases the car on the impound pad (no fee) and drives it off the pad, clear of any garage.
	protected IEntity PlaceOnStreet(LF_TestContext ctx, string vehicleId, IEntity owner, vector from, float metres)
	{
		string reasonKey;
		ctx.True(LF_VehicleOwnership.CollectFromImpound(vehicleId, OWNER, owner, 0, LF_TestAnnex.IMPOUND_PAD_ID, reasonKey), vehicleId + " is put in the world");
		IEntity vehicle = LF_VehicleOwnership.GetVehicle(vehicleId);
		if (!vehicle)
			return null;

		float radius = LF_GarageConfig.TowExemptNearGarage() + 10;
		array<vector> directions = {"1 0 0", "-1 0 0", "0 0 1", "0 0 -1"};
		foreach (vector direction : directions)
		{
			vector spot = from + direction * metres;
			if (LF_VehicleTow.NearGarage(spot, radius))
				continue;

			spot[1] = GetGame().GetWorld().GetSurfaceY(spot[0], spot[2]) + 0.5;
			vehicle.SetOrigin(spot);
			vehicle.Update();
			return vehicle;
		}

		ctx.Fail(vehicleId + ": no street spot clear of the garages");
		return vehicle;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveCar(string vehicleId)
	{
		IEntity key = LF_VehicleOwnership.GetKey(vehicleId);
		if (key)
			SCR_EntityHelper.DeleteEntityAndChildren(key);

		IEntity vehicle = LF_VehicleOwnership.GetVehicle(vehicleId);
		if (vehicle)
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
	}
}
