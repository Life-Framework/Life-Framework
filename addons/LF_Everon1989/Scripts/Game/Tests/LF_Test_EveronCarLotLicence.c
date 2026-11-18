// red-proof: RED-OK everon/car-lot-licence, observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_EveronCarLotLicence`
// (the hook turns the gate off): `the flagship's vehicles overlay turns the lot's driving licence gate on` fails. Hook off: green (--only, pack leg).
//! #3151 in game: under the Everon 1989 header (the pack leg of `tools\cli test`) the pack's vehicles
//! overlay turns the car lot's driving licence gate on, so the flagship's lot asks for the licence
//! (the core ships it off; LF_Test_CarLotLicence proves the refusal itself).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronCarLotLicence : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/car-lot-licence";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_VehiclePricesConfig.Reload();
		if (RedProof())
			LF_VehiclePricesConfig.SetLotNeedsDrivingLicenceForTest(false);

		ctx.True(LF_VehiclePricesConfig.LotNeedsDrivingLicence(), "the flagship's vehicles overlay turns the lot's driving licence gate on");
		LF_VehiclePricesConfig.Reload();
	}
}
