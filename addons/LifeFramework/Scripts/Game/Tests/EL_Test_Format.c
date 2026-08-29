// red-proof: change an expected string (e.g. 999 -> "9999") or break the
// threshold in EL_FormatUtils.AbbreviateNumber, then run the fast tier.

// tier: LOGIC
class EL_Test_FormatAbbreviate : EL_Test
{
	override string GetName()
	{
		return "core/format-abbreviate";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.EqualStr("999", EL_FormatUtils.AbbreviateNumber(999), "values below 1000 stay raw");
		ctx.EqualStr("0", EL_FormatUtils.AbbreviateNumber(0), "zero stays raw");
		ctx.EqualStr("-5", EL_FormatUtils.AbbreviateNumber(-5), "negatives stay raw");

		ctx.True(EL_FormatUtils.AbbreviateNumber(1000).EndsWith("K"), "1000 abbreviates with a K suffix");
		ctx.True(EL_FormatUtils.AbbreviateNumber(1000000).EndsWith("M"), "1e6 abbreviates with an M suffix");
		ctx.True(EL_FormatUtils.AbbreviateNumber(1000000000).EndsWith("B"), "1e9 abbreviates with a B suffix");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_UtilsMinMax : EL_Test
{
	override string GetName()
	{
		return "core/max-min-int";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.Equal(5, EL_Utils.MaxInt(5, 3), "MaxInt picks the larger");
		ctx.Equal(5, EL_Utils.MaxInt(3, 5), "MaxInt is order independent");
		ctx.Equal(3, EL_Utils.MinInt(5, 3), "MinInt picks the smaller");
		ctx.Equal(3, EL_Utils.MinInt(3, 5), "MinInt is order independent");
		ctx.Equal(0, EL_Utils.MaxInt(0, 0), "MaxInt of equals returns the value");
	}
};