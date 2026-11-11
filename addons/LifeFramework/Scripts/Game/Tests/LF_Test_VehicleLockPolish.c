// red-proof: in LF_VehicleLockComponent.ActionLabelKey return the Lock key for both states and
// run `tools\cli test --no-build --only LF_Test_VehicleLockPolish`; the "locked reads Unlock"
// assertion fails.

// tier: LOGIC
//! Lock label per state, sort order below Get in, occupant key permission, Store-in-garage offer.
class LF_Test_VehicleLockPolish : LF_Test
{
	override string GetName()
	{
		return "vehiclelock/polish";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_VehicleLockComponent.ActionLabelKey(true) == "#EL-VehicleLock_ActionUnlock", "locked reads Unlock");
		ctx.True(LF_VehicleLockComponent.ActionLabelKey(false) == "#EL-VehicleLock_ActionLock", "unlocked reads Lock");

		// Vanilla SCR_GetInUserAction carries Sort Priority 1 in the base vehicle prefabs.
		ctx.True(LF_VehicleLockComponent.ACTION_SORT_PRIORITY > 1, "the lock row sorts below Get in");

		ctx.True(LF_VehicleLockComponent.MayToggle(true, 0), "a seated key holder may toggle");
		ctx.False(LF_VehicleLockComponent.MayToggle(false, 0), "a passenger without the key is refused");
		ctx.False(LF_VehicleLockComponent.MayToggle(true, 40), "a key holder far away is refused");

		vector garage = "500 0 500";
		vector near = "505 0 500";
		vector far = "3000 0 3000";
		ctx.True(LF_VehicleStoreHereAction.IsOffered(true, false, true, garage, near, near), "owner on foot at the garage is offered Store in garage");
		ctx.False(LF_VehicleStoreHereAction.IsOffered(true, true, true, garage, near, near), "a seated player is refused");
		ctx.False(LF_VehicleStoreHereAction.IsOffered(false, false, true, garage, near, near), "a non-owner is refused");
		ctx.False(LF_VehicleStoreHereAction.IsOffered(true, false, true, garage, near, far), "a car away from the garage is refused");
		ctx.False(LF_VehicleStoreHereAction.IsOffered(true, false, true, garage, far, near), "an owner away from the garage is refused");
	}
}
