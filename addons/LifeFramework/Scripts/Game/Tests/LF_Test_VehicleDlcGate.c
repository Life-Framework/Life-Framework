// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_VehicleDlcGate` (RED PROOF OK): with the gate
// skipped it failed `the owner check asked the engine once (expected 1, got 0)`, `a player without the DLC is refused the
// tractor` and `the refusal says the tractor needs the Rally DLC (expected '#LF-Vehicle_NeedsRallyDlc', got '')`. Green after.

//! Test checker: answers for the engine, and counts how often it was asked.
class LF_Test_VehicleDlcChecker : LF_VehicleDlcChecker
{
	bool m_bAvailable = true;
	bool m_bOwns;
	int m_iAsked;

	override bool Available()
	{
		return m_bAvailable;
	}

	override bool CanUse(IEntity player, ResourceName prefab)
	{
		m_iAsked++;
		return m_bOwns;
	}
}

[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_VehicleDlcGate : LF_Test
{
	override string GetName()
	{
		return "vehicles/dlc-gate";
	}

	override void Run(LF_TestContext ctx)
	{
		// The lot sells the Rally tractor only to a player who owns the DLC; vanilla refuses its driver
		// seat otherwise (#3304). The refusal comes before any charge.
		ctx.True(LF_VehicleDlcGate.IsDlcVehicle(LF_VehicleDlcGate.Z7211), "the tractor is a DLC vehicle");
		ctx.False(LF_VehicleDlcGate.IsDlcVehicle(LF_VehicleCatalog.UAZ469), "a base-game car needs no DLC");

		LF_Test_VehicleDlcChecker owner = new LF_Test_VehicleDlcChecker();
		owner.m_bOwns = true;
		string reason;
		bool allowed = true;
		if (!RedProof())
			allowed = LF_VehicleDlcGate.AllowsWith(owner, null, LF_VehicleDlcGate.Z7211, reason);

		ctx.True(allowed, "an owner is allowed to buy the tractor");
		ctx.Equal(1, owner.m_iAsked, "the owner check asked the engine once");

		LF_Test_VehicleDlcChecker plain = new LF_Test_VehicleDlcChecker();
		reason = "";
		allowed = false;
		if (!RedProof())
			allowed = LF_VehicleDlcGate.AllowsWith(plain, null, LF_VehicleDlcGate.Z7211, reason);
		else
			allowed = true;

		ctx.False(allowed, "a player without the DLC is refused the tractor");
		ctx.EqualStr(LF_VehicleDlcGate.NEEDS_DLC_KEY, reason, "the refusal says the tractor needs the Rally DLC");

		LF_Test_VehicleDlcChecker blind = new LF_Test_VehicleDlcChecker();
		blind.m_bAvailable = false;
		blind.m_bOwns = true;
		reason = "";
		allowed = LF_VehicleDlcGate.AllowsWith(blind, null, LF_VehicleDlcGate.Z7211, reason);
		ctx.False(allowed, "a missing DlcManager refuses the DLC vehicle");
		ctx.Equal(0, blind.m_iAsked, "a missing DlcManager is never asked");
		ctx.EqualStr(LF_VehicleDlcGate.NOT_SOLD_KEY, reason, "a missing DlcManager reports not sold");
		ctx.False(LF_VehicleDlcGate.AllowsWith(null, null, LF_VehicleDlcGate.Z7211, reason), "no checker at all refuses the DLC vehicle");

		LF_Test_VehicleDlcChecker uaz = new LF_Test_VehicleDlcChecker();
		reason = "";
		ctx.True(LF_VehicleDlcGate.AllowsWith(uaz, null, LF_VehicleCatalog.UAZ469, reason), "a non-owner can still buy a base-game car");
		ctx.Equal(0, uaz.m_iAsked, "a base-game car never asks the engine");

		// The lot lists a price and a locked child for the tractor.
		LF_VehiclePricesConfig prices = LF_VehiclePricesConfig.Get();
		ctx.True(prices != null, "the vehicle prices config loads");
		if (prices)
			ctx.True(prices.PriceFor(LF_EVehiclePriceAction.BUY, LF_VehicleDlcGate.Z7211) > 0, "the tractor has a buy price");
	}
}
