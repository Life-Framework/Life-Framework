// red-proof: 2026-11-10 `tools/cli test --no-build --only LF_Test_LockpickVehicle` with the keyed-start expectation flipped to True failed `- a keyed start is not stolen`. Restored; green.
// tier: LOGIC
//! Hotwire and vehicle picking rules (#412): an unkeyed start sets the stolen flag, a keyed start does
//! not, a free ignition is never stolen, the start gate lets in a key holder or the hotwirer only.
class LF_Test_LockpickVehicle : LF_Test
{
	override string GetName()
	{
		return "lockpick/vehicle";
	}

	override void Run(LF_TestContext ctx)
	{
		// Stolen flag.
		ctx.True(LF_HotwireRules.StartsStolen(true, false), "an unkeyed start of a locked-ignition car is stolen");
		ctx.False(LF_HotwireRules.StartsStolen(true, true), "a keyed start is not stolen");
		ctx.False(LF_HotwireRules.StartsStolen(false, false), "a car with a free ignition is never stolen by starting");

		// Start gate.
		ctx.True(LF_HotwireRules.MayStart(false, false, -1, 3), "a free ignition starts for anyone");
		ctx.True(LF_HotwireRules.MayStart(true, true, -1, 3), "a key holder starts a locked ignition");
		ctx.False(LF_HotwireRules.MayStart(true, false, -1, 3), "no key and no hotwire does not start");
		ctx.True(LF_HotwireRules.MayStart(true, false, 3, 3), "the player who hotwired it starts it");
		ctx.False(LF_HotwireRules.MayStart(true, false, 3, 4), "a different player does not ride on someone else's hotwire");
		ctx.False(LF_HotwireRules.MayStart(true, false, -1, -1), "an unknown driver with no hotwire does not start");

		// Time, odds, noise.
		ctx.EqualFloat(LF_HotwireRules.BASE_SECONDS, LF_HotwireRules.Seconds(0), 0.001, "skill 0 takes the base time");
		ctx.True(LF_HotwireRules.Seconds(8) < LF_HotwireRules.Seconds(2), "a better skill hotwires faster");
		ctx.True(LF_HotwireRules.Succeeds(0, 0.1), "a low roll hotwires it");
		ctx.False(LF_HotwireRules.Succeeds(0, 0.99), "a high roll fails: hotwiring can fail");
		ctx.True(LF_HotwireRules.SuccessChance(99) <= LF_HotwireRules.MAX_SUCCESS, "success is capped below certain");
		ctx.True(LF_HotwireRules.IsHeard(true, 10, 0.99), "a hotwire is always reported");
		ctx.True(LF_HotwireRules.IsHeard(false, 0, 0.01), "a loud failure is reported");
		ctx.False(LF_HotwireRules.IsHeard(false, 10, 0.99), "a quiet failure is not");

		// The gate for the action.
		ctx.Equal(LF_EHotwireCheck.NONE, LF_HotwireRules.Check(true, false, false, true, false), "an unkeyed locked-ignition car at the wheel is allowed");
		ctx.Equal(LF_EHotwireCheck.NO_IGNITION_LOCK, LF_HotwireRules.Check(false, false, false, true, false), "a free ignition needs no hotwire");
		ctx.Equal(LF_EHotwireCheck.HAS_KEY, LF_HotwireRules.Check(true, true, false, true, false), "a key holder does not hotwire");
		ctx.Equal(LF_EHotwireCheck.ALREADY, LF_HotwireRules.Check(true, false, true, true, false), "an already hotwired car refuses");
		ctx.Equal(LF_EHotwireCheck.NOT_SEATED, LF_HotwireRules.Check(true, false, false, false, false), "outside the car refuses");
		ctx.Equal(LF_EHotwireCheck.BUSY, LF_HotwireRules.Check(true, false, false, true, true), "a second attempt refuses");
		ctx.True(LF_HotwireRules.RefusalKey(LF_EHotwireCheck.NOT_SEATED) != "", "every refusal has a line to print");
	}
}

// red-proof: 2026-11-10 (QA) asserting `CanBeShownScript` on the placed car failed `- the lockpick action shows on the locked car` against the door-only check; green after LF_LockpickAction.CanBeShownScript went back to LF_Lockpick.ShowsPick. Earlier: wiring asserted on the placed car; green with 5 of 5 (`--only` the four lockpick tests and debugworld/entity-collision) on 2026-11-10; the door twin failed the same way before its fix (see LF_Test_LockpickDoor).
// tier: WORLD
//! The playtest lock-up car is wired (#412): locked, ignition locked, pickable, both actions on it,
//! no stolen flag yet; the free-ignition car beside the lot starts for anyone. Spawns nothing. The
//! held pick and hotwire are the two-client session proof.
class LF_Test_LockpickVehicleWorld : LF_Test
{
	override string GetName()
	{
		return "e2e/lockpick-vehicle";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity car = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.LOCKPICK_CAR);
		ctx.True(car != null, "Annex_Lockpick_Car is placed");
		if (ctx.FailureCount() > 0)
			return;

		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);
		ctx.NotNull(vehicleLock, "the car carries LF_VehicleLockComponent");
		LF_LockpickTargetComponent target = LF_Component<LF_LockpickTargetComponent>.Find(car);
		ctx.NotNull(target, "the car carries LF_LockpickTargetComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(vehicleLock.IsVehicleLocked(), "the car starts locked");
		ctx.True(vehicleLock.IsIgnitionLocked(), "the car needs its key to start");
		ctx.False(vehicleLock.IsStolen(), "the car is not stolen yet");
		ctx.Equal(-1, vehicleLock.GetHotwiredPlayerId(), "nobody has hotwired it");
		ctx.True(target.IsPickable(), "the car lock is pickable");
		ctx.Equal(LF_ELockpickCheck.NO_PICK, LF_Lockpick.CheckDoor(car, null), "without a pick the locked car says it needs one");
		ctx.True(LF_Lockpick.ShowsPick(car), "the pick action is listed on the locked car");

		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(car);
		ctx.NotNull(actions, "the car carries ActionsManagerComponent");
		if (ctx.FailureCount() > 0)
			return;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		bool pickWired = false;
		bool hotwireWired = false;
		foreach (BaseUserAction action : actionList)
		{
			LF_LockpickAction pickAction = LF_LockpickAction.Cast(action);
			if (pickAction)
			{
				pickWired = true;
				// The prompt a player sees: the action must offer itself on the locked car (a door-only check hid it).
				ctx.True(pickAction.CanBeShownScript(car), "the lockpick action shows on the locked car");
			}

			if (LF_HotwireAction.Cast(action))
				hotwireWired = true;
		}

		ctx.True(pickWired, "the lockpick action is on the car");
		ctx.True(hotwireWired, "the hotwire action is on the car");

		IEntity lotCar = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.VEHICLE_S105);
		ctx.True(lotCar != null, "Annex_Vehicle_S105 is placed");
		if (lotCar)
		{
			LF_VehicleLockComponent lotLock = LF_Component<LF_VehicleLockComponent>.Find(lotCar);
			ctx.NotNull(lotLock, "the lot car carries LF_VehicleLockComponent");
			if (lotLock)
				ctx.False(lotLock.IsIgnitionLocked(), "the lot car keeps the free ignition");

			ctx.True(LF_Hotwire.MayStartEngine(lotCar, null), "a free-ignition car starts for anyone");
		}
	}
}
