// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_VehicleDlcGate` after the review rework
// (the gate takes the buyer's client answer, DlcManager is local only): with the gate skipped it failed `a player whose
// client says no DLC is refused the tractor` and `the refusal says the tractor needs the Rally DLC`. Green after.

[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_VehicleDlcGate : LF_Test
{
	override string GetName()
	{
		return "vehicles/dlc-gate";
	}

	override void Run(LF_TestContext ctx)
	{
		// The lot sells the Rally tractor only to a player whose own client says it owns the DLC;
		// vanilla refuses its driver seat otherwise (#3304). The refusal comes before any charge.
		ctx.True(LF_VehicleDlcGate.IsDlcVehicle(LF_VehicleDlcGate.Z7211), "the tractor is a DLC vehicle");
		ctx.True(LF_VehicleDlcGate.IsDlcVehicle(LF_VehicleDlcGate.Z7211_LOCKED), "the locked tractor child is a DLC vehicle");
		ctx.False(LF_VehicleDlcGate.IsDlcVehicle(LF_VehicleCatalog.UAZ469), "a base-game car needs no DLC");

		string reason;
		ctx.True(LF_VehicleDlcGate.Allows(LF_VehicleDlcGate.Z7211, true, reason), "an owner is allowed to buy the tractor");
		ctx.EqualStr("", reason, "an owner gets no refusal");

		reason = "";
		bool allowed = true;
		if (!RedProof())
			allowed = LF_VehicleDlcGate.Allows(LF_VehicleDlcGate.Z7211, false, reason);

		ctx.False(allowed, "a player whose client says no DLC is refused the tractor");
		ctx.EqualStr(LF_VehicleDlcGate.NEEDS_DLC_KEY, reason, "the refusal says the tractor needs the Rally DLC");

		reason = "";
		ctx.True(LF_VehicleDlcGate.Allows(LF_VehicleCatalog.UAZ469, false, reason), "a non-owner can still buy a base-game car");
		ctx.True(LF_VehicleDlcGate.LocalOwns(LF_VehicleCatalog.UAZ469), "a base-game car is owned without asking the engine");

		// The lot lists a price and a locked child for the tractor.
		LF_VehiclePricesConfig prices = LF_VehiclePricesConfig.Get();
		ctx.True(prices != null, "the vehicle prices config loads");
		if (prices)
			ctx.True(prices.PriceFor(LF_EVehiclePriceAction.BUY, LF_VehicleDlcGate.Z7211) > 0, "the tractor has a buy price");
	}
}
