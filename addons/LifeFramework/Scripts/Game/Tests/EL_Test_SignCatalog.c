// red-proof: expected 5 parts for a sign with an empty How and a blank Needs;
// `tools\cli test --no-build --tier all` (2026-11-08) failed signs/catalog with
// "empty How and blank Needs are left off the card (expected 5, got 4)".

// tier: LOGIC
class EL_Test_SignCatalog : EL_Test
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
	override void Run(EL_TestContext ctx)
	{
		LF_SignConfig config = new LF_SignConfig();
		config.m_aSigns = {};
		config.m_aSigns.Insert(MakeSign("bank", "Bank", "Money is kept here.", "", "  ", "No guns inside."));
		config.m_aSigns.Insert(MakeSign("mining", "#LF-Sign_Mining_Title", "#LF-Sign_Mining_What", "#LF-Sign_Mining_How", "#LF-Sign_Mining_Needs", "#LF-Sign_Mining_Rules"));

		LF_SignDef bank = LF_SignCatalog.FindIn(config, "bank");
		ctx.NotNull(bank, "bank is found by id");
		if (!bank)
			return;

		ctx.EqualStr("Bank", bank.m_sTitle, "lookup returns the bank row");
		LF_SignDef mining = LF_SignCatalog.FindIn(config, "mining");
		ctx.NotNull(mining, "mining is found by id");
		ctx.True(LF_SignCatalog.IsUsable(bank), "a titled sign is usable");

		array<ref LF_UIPart> parts = LF_SignCatalog.BuildParts(bank);
		ctx.Equal(4, parts.Count(), "empty How and blank Needs are left off the card");
		if (parts.Count() != 4)
			return;

		ctx.Equal(LF_EUIPartKind.TITLE, parts.Get(0).GetKind(), "the card opens with the title");
		ctx.EqualStr("Bank", parts.Get(0).GetText(), "a literal title is shown as written");
		ctx.EqualStr("What", parts.Get(1).GetName(), "What comes first");
		ctx.True(parts.Get(1).GetText().EndsWith(": Money is kept here."), "a literal section is shown as written");
		ctx.EqualStr("Rules", parts.Get(2).GetName(), "Rules follows What when How and Needs are empty");
		ctx.Equal(LF_EUIPartKind.CLOSE, parts.Get(3).GetKind(), "the card ends with Close");

		if (!mining)
			return;

		array<ref LF_UIPart> full = LF_SignCatalog.BuildParts(mining);
		ctx.Equal(6, full.Count(), "a full sign shows title, four sections, and Close");
		if (full.Count() != 6)
			return;

		ctx.EqualStr("How", full.Get(2).GetName(), "How is the second section");
		ctx.EqualStr("Needs", full.Get(3).GetName(), "Needs is the third section");
	}
}
