// red-proof: before BuildParts added the Next section, `tools\cli test --no-build
// --only LF_Test_SignNext` (2026-11-08) failed signs/next with
// "title, What, How, Rules, Next, and Close (expected 6, got 5)".

// tier: LOGIC
class LF_Test_SignNext : LF_Test
{
	override string GetName()
	{
		return "signs/next";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_SignDef sign = LF_Test_SignCatalog.MakeSign("post", "Post office", "Delivery work.", "Take courier work.", "", "Pays cash.");
		sign.m_sNext = "Bank your pay at the ATM next door.";

		array<ref LF_UIPart> parts = LF_SignCatalog.BuildParts(sign);
		ctx.Equal(11, parts.Count(), "title, rule, What, How, Rules, Next (label and body each), and Close");
		if (parts.Count() != 11)
			return;

		ctx.EqualStr("Next", parts.Get(8).GetName(), "Next comes after Rules");
		ctx.EqualStr("Bank your pay at the ATM next door.", parts.Get(9).GetText(), "Next shows its text");
		ctx.Equal(LF_EUIPartKind.CLOSE, parts.Get(10).GetKind(), "the card still ends with Close");

		// Every placed site tells a stranger where to go after it.
		LF_SignConfig config = LF_SignCatalog.Load(LF_SignCatalog.CONFIG_PATH);
		ctx.NotNull(config, "LF_Signs.conf loads");
		if (!config || !config.m_aSigns)
			return;

		foreach (LF_SignDef def : config.m_aSigns)
		{
			ctx.False(def.m_sNext.IsEmpty(), string.Format("sign '%1' says what comes next", def.m_sId));
		}
	}
}
