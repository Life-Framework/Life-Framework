// red-proof: RedProof() skips the rule calls, so the expectations see their defaults and `an owner with the price and an empty compartment is fitted` fails (run `tools\cli test --no-build --red LF_Test_ChopFitRules`).
//! #3776: the pure rules of fitting a hidden compartment at the chop shop. The owner of a car in the
//! drop area is fitted when the table prices the car, it has no compartment yet and the purse covers the
//! price. Each other case names its own refusal; the bench picks the worker's own car over a nearer
//! stranger's; every refusal has a toast.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ChopFitRules : LF_Test
{
	override string GetName()
	{
		return "crime/chop-fit-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EFitCheck ok = LF_EFitCheck.NO_VEHICLE;
		LF_EFitCheck noCar = LF_EFitCheck.NONE;
		LF_EFitCheck stranger = LF_EFitCheck.NONE;
		LF_EFitCheck unsupported = LF_EFitCheck.NONE;
		LF_EFitCheck fitted = LF_EFitCheck.NONE;
		LF_EFitCheck poor = LF_EFitCheck.NONE;
		LF_EFitCheck exact = LF_EFitCheck.NO_VEHICLE;
		if (!RedProof())
		{
			ok = LF_ChopFitRules.Check(true, true, 320, false, 1000);
			noCar = LF_ChopFitRules.Check(false, false, -1, false, 1000);
			stranger = LF_ChopFitRules.Check(true, false, 320, false, 1000);
			unsupported = LF_ChopFitRules.Check(true, true, -1, false, 1000);
			fitted = LF_ChopFitRules.Check(true, true, 320, true, 1000);
			poor = LF_ChopFitRules.Check(true, true, 320, false, 319);
			exact = LF_ChopFitRules.Check(true, true, 320, false, 320);
		}

		ctx.Equal(LF_EFitCheck.NONE, ok, "an owner with the price and an empty compartment is fitted");
		ctx.Equal(LF_EFitCheck.NO_VEHICLE, noCar, "no car in the drop area is refused");
		ctx.Equal(LF_EFitCheck.NOT_OWNER, stranger, "a car the worker does not own is refused");
		ctx.Equal(LF_EFitCheck.UNSUPPORTED, unsupported, "a car with no table row is refused");
		ctx.Equal(LF_EFitCheck.ALREADY_FITTED, fitted, "a car that already has a compartment is refused");
		ctx.Equal(LF_EFitCheck.SHORT_PURSE, poor, "a purse one short of the price is refused");
		ctx.Equal(LF_EFitCheck.NONE, exact, "a purse equal to the price is enough");
		ctx.Equal(LF_EFitCheck.NOT_OWNER, LF_ChopFitRules.Check(true, false, -1, true, 0), "ownership is read before the table, so a stranger learns nothing about the car");

		array<float> distances = {3, 6};
		array<bool> ownedBy = {false, true};
		ctx.Equal(1, LF_ChopFitRules.PickIndex(distances, ownedBy), "the worker's own car further out beats a stranger's nearer one");
		ownedBy = {false, false};
		ctx.Equal(0, LF_ChopFitRules.PickIndex(distances, ownedBy), "with no owned car the nearest car is named");
		distances = {};
		ownedBy = {};
		ctx.Equal(-1, LF_ChopFitRules.PickIndex(distances, ownedBy), "an empty drop area has no car");

		ctx.EqualStr("", LF_ChopFitRules.RefusalKey(LF_EFitCheck.NONE), "a fitted car needs no refusal");
		for (int i = LF_EFitCheck.NO_VEHICLE; i <= LF_EFitCheck.FIT_FAILED; i++)
		{
			LF_EFitCheck check = i;
			ctx.True(!LF_ChopFitRules.RefusalKey(check).IsEmpty(), "every refusal has a toast: " + typename.EnumToString(LF_EFitCheck, check));
		}

		ctx.Equal(-1, LF_ChopFitRules.PriceFor(""), "an empty prefab has no price");
	}
}
