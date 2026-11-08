// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only EL_Test_VehicleLotSheet` (lf-testbox) and
// LF_VehicleLot.Shortfall comparing bank > price:
// [ELTEST]   - a $1,000 car is buyable with $1,000 in the bank (expected '', got 'needs $1,000 in the bank')
// Restored to >=; green.

//! The car lot's price sheet (maintainer playtest 2026-11-08): every car is listed; a row has Buy
//! when the reader can pay, otherwise the line says what is missing. Cars up to the cash ceiling
//! are paid in cash, dearer ones from the bank.
// tier: LOGIC
class EL_Test_VehicleLotSheet : EL_Test
{
	override string GetName()
	{
		return "vehicles/lot-sheet";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.EqualStr("", LF_VehicleLot.Shortfall(1000, 500, 0, 1000), "a $1,000 car is buyable with $1,000 in the bank");
		ctx.False(LF_VehicleLot.Shortfall(1000, 500, 5000, 650).IsEmpty(), "$650 in the bank does not buy a $1,000 car, whatever the cash");
		ctx.EqualStr("", LF_VehicleLot.Shortfall(400, 500, 400, 0), "a car under the cash ceiling is paid in cash");
		ctx.False(LF_VehicleLot.Shortfall(400, 500, 100, 5000).IsEmpty(), "a cheap car needs the cash in hand, not the bank");

		ctx.Equal(0, LF_VehicleLotSheet.RowIndex("Car0"), "row Car0 is offer 0");
		ctx.Equal(12, LF_VehicleLotSheet.RowIndex("Car12"), "row Car12 is offer 12");
		ctx.Equal(-1, LF_VehicleLotSheet.RowIndex("CarX"), "a row name without a number is no offer");
		ctx.Equal(-1, LF_VehicleLotSheet.RowIndex("LotClose"), "the close button is no offer");

		EL_SpawnLogic spawnLogic = EL_SpawnLogic.GetInstance();
		if (spawnLogic)
			ctx.True(spawnLogic.GetStartingBank() >= 1000, "a new character's bank covers the cheapest car (the S105, $1,000)");
	}
}
