// red-proof: asserted FindIn(config, "no_such_sign") != null;
// `tools\cli test --no-build --tier all` (2026-11-08) failed signs/missing-id
// with "an unknown id finds nothing".

// tier: LOGIC
class LF_Test_SignMissingId : LF_Test
{
	override string GetName()
	{
		return "signs/missing-id";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_SignConfig config = new LF_SignConfig();
		config.m_aSigns = {};
		config.m_aSigns.Insert(LF_Test_SignCatalog.MakeSign("bank", "Bank", "Money is kept here.", "", "", ""));
		config.m_aSigns.Insert(LF_Test_SignCatalog.MakeSign("untitled", "", "No title.", "", "", ""));

		ctx.True(LF_SignCatalog.FindIn(config, "no_such_sign") == null, "an unknown id finds nothing");
		ctx.True(LF_SignCatalog.FindIn(config, "") == null, "an empty id finds nothing");
		ctx.True(LF_SignCatalog.FindIn(config, "BANK") == null, "ids match exactly");
		ctx.True(LF_SignCatalog.FindIn(null, "bank") == null, "a missing config finds nothing");

		LF_SignConfig empty = new LF_SignConfig();
		ctx.True(LF_SignCatalog.FindIn(empty, "bank") == null, "a config with no list finds nothing");

		ctx.False(LF_SignCatalog.IsUsable(null), "a missing sign is not usable");
		ctx.False(LF_SignCatalog.IsUsable(LF_SignCatalog.FindIn(config, "untitled")), "a sign with no title is not usable");
		ctx.Equal(0, LF_SignCatalog.BuildParts(null).Count(), "a missing sign builds no card");
		ctx.EqualStr("", LF_SignCatalog.ReadName(null), "a missing sign has no prompt");
	}
}
