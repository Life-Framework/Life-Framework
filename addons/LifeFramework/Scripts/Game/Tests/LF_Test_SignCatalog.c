// red-proof: expected 5 parts for a sign with an empty How and a blank Needs;
// `tools\cli test --no-build --tier all` (2026-11-08) failed signs/catalog with
// "empty How and blank Needs are left off the card (expected 5, got 4)".
// Card redesign (2026-11-10): each section is a label plus a body part, with a rule under the title.

// tier: LOGIC
class LF_Test_SignCatalog : LF_Test
{
	override string GetName()
	{
		return "signs/catalog";
	}

	//------------------------------------------------------------------------------------------------
	static LF_SignDef MakeSign(string id, string title, string what, string how, string needs, string rules)
	{
		LF_SignDef sign = new LF_SignDef();
		sign.m_sId = id;
		sign.m_sTitle = title;
		sign.m_sWhat = what;
		sign.m_sHow = how;
		sign.m_sNeeds = needs;
		sign.m_sRules = rules;
		return sign;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SignConfig config = new LF_SignConfig();
		config.m_aSigns = {};
		config.m_aSigns.Insert(MakeSign("bank", "Bank", "Money is kept here.", "", "  ", "No guns inside."));
		// Literal text: the per-sign keys are the pack's (#325), and this test must pass on the core alone.
		config.m_aSigns.Insert(MakeSign("mining", "Mining area", "Ore in the rocks.", "Swing the pickaxe.", "A pickaxe.", "Keep the area clean."));

		LF_SignDef bank = LF_SignCatalog.FindIn(config, "bank");
		ctx.NotNull(bank, "bank is found by id");
		if (!bank)
			return;

		ctx.EqualStr("Bank", bank.m_sTitle, "lookup returns the bank row");
		LF_SignDef mining = LF_SignCatalog.FindIn(config, "mining");
		ctx.NotNull(mining, "mining is found by id");
		ctx.True(LF_SignCatalog.IsUsable(bank), "a titled sign is usable");

		array<ref LF_UIPart> parts = LF_SignCatalog.BuildParts(bank);
		ctx.Equal(7, parts.Count(), "empty How and blank Needs are left off the card (title, rule, 2 x label and body, Close)");
		if (parts.Count() != 7)
			return;

		ctx.Equal(LF_EUIPartKind.TITLE, parts.Get(0).GetKind(), "the card opens with the title");
		ctx.EqualStr("Bank", parts.Get(0).GetText(), "a literal title is shown as written");
		ctx.Equal(LF_EUIPartKind.RULE, parts.Get(1).GetKind(), "a rule sits under the title");
		ctx.EqualStr("What", parts.Get(2).GetName(), "What comes first");
		ctx.Equal(LF_EUIPartKind.HEADING, parts.Get(2).GetKind(), "a section opens with its label");
		ctx.EqualStr("WHAT IT IS", parts.Get(2).GetText(), "the label is upper case");
		ctx.Equal(LF_EUIPartKind.RICH, parts.Get(3).GetKind(), "then its body");
		ctx.EqualStr("Money is kept here.", parts.Get(3).GetText(), "a literal section is shown as written");
		ctx.EqualStr("Rules", parts.Get(4).GetName(), "Rules follows What when How and Needs are empty");
		ctx.Equal(LF_EUIPartKind.CLOSE, parts.Get(6).GetKind(), "the card ends with Close");

		if (!mining)
			return;

		array<ref LF_UIPart> full = LF_SignCatalog.BuildParts(mining);
		ctx.Equal(11, full.Count(), "a full sign shows title, rule, four sections of two parts, and Close");
		if (full.Count() != 11)
			return;

		ctx.EqualStr("How", full.Get(4).GetName(), "How is the second section");
		ctx.EqualStr("Needs", full.Get(6).GetName(), "Needs is the third section");
	}
}
