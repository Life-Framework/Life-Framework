// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_VehicleLotSheet` (lf-testbox) and
// LF_VehicleLot.Shortfall comparing bank > price:
// [LFTEST]   - a $1,000 car is buyable with $1,000 in the bank (expected '', got 'needs $1,000 in the bank')
// Restored to >=; green.
// red-proof (truck license hint): observed 2026-11-27 with `tools\cli test --no-build --red LF_Test_VehicleLotSheet`:
// red-proof (harness read-back): observed 2026-11-27 with `tools\cli test --no-build --red LF_Test_VehicleLotSheet`:
// RED-OK vehicles/lot-sheet (25 assertions); red: a row with no Buy button reads back as no button (expected 'Car1=Ural-4320 (needs truck license) | $12,000 | no button', got 'no row line')
// RED-OK vehicles/lot-sheet; red: a truck row carries the truck license hint (expected 'Ural-4320 (needs truck license)', got 'Ural-4320')

//! The car lot's price sheet (maintainer playtest 2026-11-08): every car is listed; a row has Buy
//! when the reader can pay, otherwise the line says what is missing. Cars up to the cash ceiling
//! are paid in cash, dearer ones from the bank.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_VehicleLotSheet : LF_Test
{
	override string GetName()
	{
		return "vehicles/lot-sheet";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("", LF_VehicleLot.Shortfall(1000, 500, 0, 1000), "a $1,000 car is buyable with $1,000 in the bank");
		ctx.False(LF_VehicleLot.Shortfall(1000, 500, 5000, 650).IsEmpty(), "$650 in the bank does not buy a $1,000 car, whatever the cash");
		ctx.EqualStr("", LF_VehicleLot.Shortfall(400, 500, 400, 0), "a car under the cash ceiling is paid in cash");
		ctx.False(LF_VehicleLot.Shortfall(400, 500, 100, 5000).IsEmpty(), "a cheap car needs the cash in hand, not the bank");

		ctx.Equal(0, LF_VehicleLotSheet.RowIndex("Car0"), "row Car0 is offer 0");
		ctx.Equal(12, LF_VehicleLotSheet.RowIndex("Car12"), "row Car12 is offer 12");
		ctx.Equal(-1, LF_VehicleLotSheet.RowIndex("CarX"), "a row name without a number is no offer");
		ctx.Equal(-1, LF_VehicleLotSheet.RowIndex("LotClose"), "the close button is no offer");

		// A lot with no id, or an id with no title row, keeps the default sheet title.
		ctx.EqualStr(LF_VehicleLot.DEFAULT_TITLE, LF_VehicleLot.TitleKeyFor(""), "a lot with no id keeps the default title");
		ctx.EqualStr(LF_VehicleLot.DEFAULT_TITLE, LF_VehicleLot.TitleKeyFor("no-such-lot"), "a lot id with no title row keeps the default title");

		// A truck row says it needs the truck license before Buy; a car row stays silent (#4417,
		// playtest 2026-11-27). The hint comes through the same class Buy checks.
		string truckLabel = "Ural-4320";
		if (!RedProof())
			truckLabel = LF_VehicleLot.RowLabel("Ural-4320", LF_VehicleLotComponent.CLASS_TRUCK);

		ctx.True(truckLabel.Contains("Ural-4320"), "a truck row keeps the truck's name");
		ctx.EqualStr(WidgetManager.Translate("#LF-Lot_RowNeedsTruck", "Ural-4320"), truckLabel, "a truck row carries the truck license hint");
		ctx.EqualStr("Sedan", LF_VehicleLot.RowLabel("Sedan", ""), "a car row with no class gets no hint");
		ctx.EqualStr("Sedan", LF_VehicleLot.RowLabel("Sedan", "car"), "a car row of another class gets no hint");

		LF_VehicleOffer plain = LF_VehicleOffer.Make("", 1000, "#LF-Lot_Title");
		ctx.EqualStr("", plain.CapacityClassIn(null), "an offer with no price config names no class, so no hint");

		// The harness `lotsheet` verb reads rows back as "<row>=<label> | <status> | <button>" and picks the
		// lot nearest the character; its words are open, rows and close.
		string rowLine = "no row line";
		if (!RedProof())
			rowLine = LF_VehicleLotSheet.RowLine("Car1", "Ural-4320 (needs truck license)", "$12,000", "");
		ctx.EqualStr("Car1=Ural-4320 (needs truck license) | $12,000 | no button", rowLine, "a row with no Buy button reads back as no button");
		ctx.EqualStr("Car0=S105 | $1,000 | Buy", LF_VehicleLotSheet.RowLine("Car0", "S105", "$1,000", "Buy"), "a row with Buy reads back with the button");
		ctx.EqualStr("", LF_VehicleLotSheet.ReadBack(), "no sheet open reads back nothing");
		array<vector> origins = {};
		ctx.Equal(-1, LF_VehicleLotComponent.NearestIndex(origins, vector.Zero), "no lots, no nearest one");
		origins.Insert("100 0 0");
		origins.Insert("10 0 5");
		origins.Insert("-40 0 0");
		ctx.Equal(1, LF_VehicleLotComponent.NearestIndex(origins, "12 3 0"), "the nearest lot is the closest on the ground plane");
		ctx.EqualStr("open", LF_VehicleLotHarness.ParseWord("open"), "open is a lotsheet word");
		ctx.EqualStr("", LF_VehicleLotHarness.ParseWord("buy"), "buy is not a lotsheet word (the harness never buys)");

		LF_SpawnLogic spawnLogic = LF_SpawnLogic.GetInstance();
		if (spawnLogic)
			ctx.True(spawnLogic.GetStartingBank() >= 1000, "a new character's bank covers the cheapest car (the S105, $1,000)");
	}
}
