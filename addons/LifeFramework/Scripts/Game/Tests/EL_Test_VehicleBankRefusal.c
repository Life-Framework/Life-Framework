// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only EL_Test_VehicleBankRefusal`: with the
// ceiling branch in EL_VehicleOwnership.RefusalKey made unreachable it failed with `[ELTEST] FAIL vehicles/bank-refusal`,
// `- a car above the cash ceiling points the buyer to the bank (expected '#LF-Vehicle_BankShort', got '#EL-Vehicle_CannotAfford')`. Restored; green.

// tier: LOGIC
class EL_Test_VehicleBankRefusal : EL_Test
{
	override string GetName()
	{
		return "vehicles/bank-refusal";
	}

	override void Run(EL_TestContext ctx)
	{
		// A new player holding 1020 cash at the lot was told "You cannot afford this vehicle" for
		// a 1000 car: above the cash ceiling a car is paid from the bank only. The refusal must say so.
		ctx.EqualStr("#LF-Vehicle_BankShort", EL_VehicleOwnership.RefusalKey(1000, 0), "a car above the cash ceiling points the buyer to the bank");
		ctx.EqualStr("#EL-Vehicle_CannotAfford", EL_VehicleOwnership.RefusalKey(100, 500), "a cash purchase short of cash keeps the plain refusal");
	}
}
