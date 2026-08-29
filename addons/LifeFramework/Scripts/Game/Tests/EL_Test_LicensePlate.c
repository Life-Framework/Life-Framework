// red-proof: change the format in EL_LicensePlateGeneratorGeneric (e.g. drop a
// space or switch to single letters) and run the fast tier; the shape
// assertions fail.

//! The generator is RNG-driven, so no value-level assertion is deterministic.
//! This tests the SHAPE only: two uppercase letters, space, two uppercase
//! letters, space, then 3-4 digits. The known off-by-one (RandomInt(0,25)
//! excludes Z; RandomInt(100,9999) excludes 9999) is a code-review finding in
//! features.md, not a test here, because it cannot be observed deterministically.
class EL_Test_LicensePlateFormat : EL_Test
{
	override string GetName()
	{
		return "licenseplate/format";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_LicensePlateGeneratorGeneric generator = new EL_LicensePlateGeneratorGeneric();
		string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		string digits = "0123456789";

		for (int i = 0; i < 100; i++)
		{
			string plate = generator.GenerateLicensePlate();
			ctx.InRange(plate.Length(), 9, 10, "plate is 9 or 10 chars (AA BB NNN or AA BB NNNN)");
			ctx.True(plate.Get(2) == " ", "third char is a space");
			ctx.True(plate.Get(5) == " ", "sixth char is a space");

			ctx.True(IsIn(plate.Get(0), alphabet), "first char is an uppercase letter");
			ctx.True(IsIn(plate.Get(1), alphabet), "second char is an uppercase letter");
			ctx.True(IsIn(plate.Get(3), alphabet), "fourth char is an uppercase letter");
			ctx.True(IsIn(plate.Get(4), alphabet), "fifth char is an uppercase letter");

			ctx.True(IsIn(plate.Get(6), digits), "seventh char is a digit");
			ctx.True(IsIn(plate.Get(7), digits), "eighth char is a digit");
			ctx.True(IsIn(plate.Get(8), digits), "ninth char is a digit");
			if (plate.Length() == 10)
				ctx.True(IsIn(plate.Get(9), digits), "tenth char is a digit");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsIn(string value, string set)
	{
		return set.IndexOf(value) != -1;
	}
};