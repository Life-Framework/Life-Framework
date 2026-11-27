// red-proof: RED-OK everon/lot-sheet-title, observed 2026-11-27 with `tools\cli test --no-build --red LF_Test_EveronLotSheetTitle` (the hook skips the truck-yard lookup and uses the default key): `the truck yard sheet reads Truck yard - price list` (expected 'Truck yard - price list', got 'Car lot - price list'). Hook off: green.
//! Pack leg: the truck yard's price sheet carries its own title, a lot with no title row of its own keeps
//! the car lot's default.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronLotSheetTitle : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/lot-sheet-title";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string yardKey = LF_VehicleLot.DEFAULT_TITLE;
		if (!RedProof())
			yardKey = LF_VehicleLot.TitleKeyFor("truck-yard");

		ctx.EqualStr("Truck yard - price list", WidgetManager.Translate(yardKey), "the truck yard sheet reads Truck yard - price list");
		ctx.EqualStr("Car lot - price list", WidgetManager.Translate(LF_VehicleLot.TitleKeyFor("")), "the car lot (no lot id) keeps the default title");
		ctx.EqualStr(LF_VehicleLot.DEFAULT_TITLE, LF_VehicleLot.TitleKeyFor("no-such-lot"), "a lot id with no title row keeps the default key");
	}
}
