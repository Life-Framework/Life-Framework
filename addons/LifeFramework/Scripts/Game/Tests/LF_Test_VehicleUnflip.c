// red-proof: observed 2026-11-19 with `toolsi test --no-build --red LF_Test_VehicleUnflip`: RED PROOF OK; with the push skipped the car stayed on its roof (`the car is upright afterwards`, `a car on its wheels refuses` failed). Green without the hook.
//! vehicles/unflip (#3333): "Push it back over" sets an empty overturned car upright on the ground at the
//! same spot; a car on its wheels, a car with a driver in it, a player out of reach and a second push inside
//! the cooldown are refused and the car does not move.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_VehicleUnflip : LF_Test
{
	protected static const ResourceName UAZ = "{C8B16ABCA57679A2}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et";
	protected static const vector SPOT = "140 0 60";
	protected static const int T0 = 100000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/unflip";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_VehicleUnflip.ResetStatics();

		// The pure rule.
		ctx.True(LF_VehicleUnflip.Rules(-1, false, 2, -1, T0) == LF_EUnflipCheck.NONE, "a car on its roof, empty, in reach, never pushed: allowed");
		ctx.True(LF_VehicleUnflip.Rules(0, false, 2, -1, T0) == LF_EUnflipCheck.NONE, "a car on its side is allowed");
		ctx.True(LF_VehicleUnflip.Rules(1, false, 2, -1, T0) == LF_EUnflipCheck.NOT_OVERTURNED, "a car on its wheels refuses");
		ctx.True(LF_VehicleUnflip.Rules(-1, true, 2, -1, T0) == LF_EUnflipCheck.OCCUPIED, "a car with someone inside refuses");
		ctx.True(LF_VehicleUnflip.Rules(-1, false, 20, -1, T0) == LF_EUnflipCheck.TOO_FAR, "a player out of reach refuses");
		ctx.True(LF_VehicleUnflip.Rules(-1, false, 2, T0, T0 + 1000) == LF_EUnflipCheck.COOLDOWN, "a second push inside the cooldown refuses");
		ctx.True(LF_VehicleUnflip.Rules(-1, false, 2, T0, T0 + LF_VehicleUnflip.COOLDOWN_MS) == LF_EUnflipCheck.NONE, "after the cooldown it is allowed again");

		// The world: a real car.
		vector at = SPOT;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + 2;
		IEntity car = LF_Utils.SpawnEntityPrefab(UAZ, at, "0 0 180");
		ctx.NotNull(car, "a UAZ469 spawns on its roof");
		if (!car)
			return;

		ctx.True(LF_VehicleUnflip.IsOverturned(car), "the spawned car lies on its roof");
		ctx.False(LF_VehicleUnflip.HasOccupant(car), "nobody is inside");

		// Out of reach: refused, the car stays on its roof.
		ctx.True(LF_VehicleUnflip.Try(car, 50, T0) == LF_EUnflipCheck.TOO_FAR, "a player out of reach cannot push it over");
		ctx.True(LF_VehicleUnflip.IsOverturned(car), "the refused car is still on its roof");

		vector before = car.GetOrigin();
		LF_EUnflipCheck result = LF_EUnflipCheck.NOT_OVERTURNED;
		if (!RedProof())
			result = LF_VehicleUnflip.Try(car, 2, T0);

		ctx.True(result == LF_EUnflipCheck.NONE, "the empty overturned car is pushed back over");
		ctx.False(LF_VehicleUnflip.IsOverturned(car), "the car is upright afterwards");
		ctx.True(LF_VehicleUnflip.UpDot(car) > 0.95, "the car's up axis points up");
		vector after = car.GetOrigin();
		ctx.True(vector.DistanceXZ(before, after) < 0.5, "the car stays at the same spot");
		float ground = GetGame().GetWorld().GetSurfaceY(after[0], after[2]);
		ctx.True(after[1] - ground < 1.5, "the car stands on the ground, not in the air");

		// On its wheels now: a second push refuses.
		ctx.True(LF_VehicleUnflip.Try(car, 2, T0 + 60000) == LF_EUnflipCheck.NOT_OVERTURNED, "a car on its wheels refuses");

		// A car with someone inside refuses: the rule above covers it. The engine seats a character a frame
		// after the request (see LF_Test_JailVehicleArrest), so a seated car is the manual step.
		SCR_EntityHelper.DeleteEntityAndChildren(car);
		LF_VehicleUnflip.ResetStatics();
	}
}
