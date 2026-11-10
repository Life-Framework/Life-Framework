// red-proof: with ParseItems no longer splitting on "|", `tools\cli test --no-build --only LF_Test_SignCardFormat`
// (2026-11-10) failed signs/card-format with "a | separated string is a list (3 items) (expected 3, got 1)".

// tier: LOGIC
class LF_Test_SignCardFormat : LF_Test
{
	override string GetName()
	{
		return "signs/card-format";
	}

	override void Run(LF_TestContext ctx)
	{
		array<string> items;
		ctx.False(LF_SignCatalog.ParseItems("Delivery work for cash.", items), "plain prose is not a list");
		ctx.Equal(1, items.Count(), "prose is one paragraph");

		ctx.True(LF_SignCatalog.ParseItems("One|Two | Three", items), "a | separated string is a list");
		ctx.Equal(3, items.Count(), "a | separated string is a list (3 items)");
		if (items.Count() < 2)
			return;

		ctx.EqualStr("Two", items.Get(1), "items are trimmed");

		ctx.True(LF_SignCatalog.ParseItems("- Only one", items), "a '- ' line is a list even with one item");
		if (items.Count() < 1)
			return;

		ctx.EqualStr("Only one", items.Get(0), "the marker is dropped");

		ctx.True(LF_SignCatalog.ParseItems("- A\n- B\r\n\n- C", items), "newlines separate '- ' items");
		ctx.Equal(3, items.Count(), "blank lines are skipped");

		ctx.False(LF_SignCatalog.ParseItems("   ", items), "blank text is not a list");
		ctx.Equal(0, items.Count(), "blank text has no items");

		ctx.EqualStr("Press <color rgba='1,2,3,255'>Take work</color> now", LF_SignCatalog.Highlight("Press *Take work* now", "1,2,3,255"), "*word* takes the accent colour");
		ctx.EqualStr("Press Take work now", LF_SignCatalog.Highlight("Press *Take work* now", ""), "no accent drops the stars");
		ctx.EqualStr("5 * 3 cash", LF_SignCatalog.Highlight("5 * 3 cash", "1,2,3,255"), "a lone star stays as written");
		ctx.EqualStr("bAda/b says hi", LF_SignCatalog.Highlight("<b>Ada</b> says hi", "1,2,3,255"), "typed angle brackets are dropped, so text never opens a tag");

		ctx.EqualStr("A sentence.", LF_SignCatalog.RichBody("A sentence.", ""), "prose has no bullet");
		string list = LF_SignCatalog.RichBody("One|Two", "");
		ctx.EqualStr(LF_SignCatalog.BULLET + " One<br/>" + LF_SignCatalog.BULLET + " Two", list, "a list gets a bullet and a break per item");

		// Painted signs take the darkest of their colours as the accent, never lighter than the cap.
		vector accent = LF_SignCatalog.DarkestOf("0.95 0.72 0.08", "0.06 0.18 0.5", "0.06 0.18 0.5");
		ctx.True(accent[2] > accent[0], "the post office accent is the navy lettering, not the yellow plate");
		vector light = LF_SignCatalog.DarkestOf("1 1 1", "0.9 0.9 0.9", "1 1 1");
		ctx.True(light[0] < 0.45, "an all-light sign still gets a readable accent");

		LF_UISkin plain = LF_SignCatalog.CardSkin(false, vector.Zero, vector.Zero, vector.Zero);
		ctx.NotNull(plain, "an unpainted sign has a card skin");
		ctx.EqualStr("118,38,28,255", plain.m_sAccentRgba, "an unpainted sign prints its labels in oxblood");

		LF_UISkin painted = LF_SignCatalog.CardSkin(true, "0.06 0.18 0.5", "0.95 0.72 0.08", "0.06 0.18 0.5");
		ctx.True(painted.m_sAccentRgba != plain.m_sAccentRgba, "a painted sign takes its own accent");

		LF_SignDef sign = LF_Test_SignCatalog.MakeSign("t", "T", "", "Take *work*|Return", "", "");
		array<ref LF_UIPart> parts = LF_SignCatalog.BuildParts(sign, painted);
		ctx.Equal(5, parts.Count(), "title, rule, How label and body, Close");
		if (parts.Count() == 5)
			ctx.True(parts.Get(3).GetText().Contains("<color rgba='" + painted.m_sAccentRgba + "'>work</color>"), "the body carries the skin accent");
		// A card fits the sheet: short ones at full size, long ones shrunk, and every real sign within the cap.
		LF_SignDef shortSign = LF_Test_SignCatalog.MakeSign("s", "Short", "One line.", "", "", "");
		float shortScale = LF_SignCatalog.FitScale(LF_SignCatalog.BuildParts(shortSign, painted), painted.m_fWidth);
		ctx.True(shortScale > 0.99, "a short card keeps full size");

		string wall = "word word word word word word word word word word word word word word word word word word word word word word word word word";
		string wallList = wall + "|" + wall + "|" + wall + "|" + wall + "|" + wall + "|" + wall;
		LF_SignDef longSign = LF_Test_SignCatalog.MakeSign("l", "Long", wallList, wallList, wallList, wallList);
		float longScale = LF_SignCatalog.FitScale(LF_SignCatalog.BuildParts(longSign, painted), painted.m_fWidth);
		ctx.True(longScale < 0.99, "a card too long for the sheet shrinks its type");

		LF_SignConfig config = LF_SignCatalog.Load(LF_SignCatalog.CONFIG_PATH);
		ctx.NotNull(config, "LF_Signs.conf loads");
		if (!config || !config.m_aSigns)
			return;

		foreach (LF_SignDef def : config.m_aSigns)
		{
			array<ref LF_UIPart> cardParts = LF_SignCatalog.BuildParts(def, painted);
			float scale = LF_SignCatalog.FitScale(cardParts, painted.m_fWidth);
			float height = LF_SignCatalog.EstimateHeight(cardParts, painted.m_fWidth, scale);
			ctx.True(height <= LF_SignCatalog.MAX_CARD_HEIGHT, string.Format("sign '%1' fits the sheet (%2 px at scale %3)", def.m_sId, height, scale));
		}
	}
}
