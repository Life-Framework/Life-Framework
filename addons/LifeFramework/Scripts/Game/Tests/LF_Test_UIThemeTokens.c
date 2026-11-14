// red-proof: observed 2026-11-14, cli test --no-build --only LF_Test_UIThemeTokens, with ParseHex
// accepting only six digits: "[LFTEST]   - lower case, no #, with alpha parses" and
// "[LFTEST]   - alpha byte (expected 68, got 255)".

//------------------------------------------------------------------------------------------------
//! The theme's pure parts (#966): hex colours, inheritance from the paper theme, and the built-in
//! 1989 values the sheets fall back to. No resource is loaded.
// tier: LOGIC
class LF_Test_UIThemeTokens : LF_Test
{
	override string GetName()
	{
		return "ui-kit/theme-tokens";
	}

	override void Run(LF_TestContext ctx)
	{
		CheckHex(ctx);
		CheckInherit(ctx);
		CheckBuiltIn(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckHex(LF_TestContext ctx)
	{
		int r, g, b, a;
		ctx.True(LF_UIThemeRecord.ParseHex("#EEE6D4", r, g, b, a), "#RRGGBB parses");
		ctx.Equal(238, r, "red byte");
		ctx.Equal(230, g, "green byte");
		ctx.Equal(212, b, "blue byte");
		ctx.Equal(255, a, "no alpha means opaque");

		ctx.True(LF_UIThemeRecord.ParseHex("eee6d444", r, g, b, a), "lower case, no #, with alpha parses");
		ctx.Equal(68, a, "alpha byte");

		ctx.False(LF_UIThemeRecord.ParseHex("", r, g, b, a), "empty is not a colour");
		ctx.False(LF_UIThemeRecord.ParseHex("#12345", r, g, b, a), "five digits is not a colour");
		ctx.False(LF_UIThemeRecord.ParseHex("#GG0000", r, g, b, a), "a non-hex digit is not a colour");
		ctx.EqualStr("0,184,169,255", LF_UIThemeRecord.ToRgba("#00B8A9"), "the rich-text markup bytes");
		ctx.EqualStr("", LF_UIThemeRecord.ToRgba("teal"), "a name is not a colour");

		Color fallback = Color.FromSRGBA(1, 2, 3, 255);
		Color bad = LF_UIThemeRecord.ParseColor("not-a-colour", fallback);
		ctx.Equal(fallback.PackToInt(), bad.PackToInt(), "a malformed colour returns the fallback");
		ctx.Equal(Color.FromSRGBA(238, 230, 212, 255).PackToInt(), LF_UIThemeRecord.ParseColor("#EEE6D4", fallback).PackToInt(), "a good colour is the sRGB colour");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckInherit(LF_TestContext ctx)
	{
		LF_UIThemeRecord base = LF_UITheme.BuiltIn(LF_UIThemeConfig.BASE_ID);
		base.m_sAccent = "#112233";
		base.m_PaperTexture = LF_UITheme.FONT;

		LF_UIThemeRecord child = new LF_UIThemeRecord();
		child.m_sThemeId = "terminal";
		child.m_sInk = "#60EC78";
		LF_UIThemeRecord resolved = child.Inherit(base);
		ctx.EqualStr("#60EC78", resolved.m_sInk, "a field the theme sets is its own");
		ctx.EqualStr(base.m_sPaper, resolved.m_sPaper, "an empty colour takes the paper theme's");
		ctx.EqualStr(base.m_sBlur, resolved.m_sBlur, "the backdrop tint is inherited");
		ctx.EqualStr(base.m_Font, resolved.m_Font, "an empty face takes the paper theme's");
		ctx.Equal(base.m_iBodySize, resolved.m_iBodySize, "a 0 size takes the paper theme's");
		ctx.EqualStr("", resolved.m_sAccent, "the accent is not inherited");
		ctx.True(resolved.m_PaperTexture.IsEmpty(), "the paper texture is not inherited: a flat terminal stays flat");
		ctx.EqualStr("", child.m_sPaper, "inheriting leaves the source theme as it was");

		LF_UIThemeRecord alone = child.Inherit(null);
		ctx.EqualStr("", alone.m_sPaper, "no base leaves empty fields empty");

		array<string> bad = {};
		LF_UIThemeRecord typo = new LF_UIThemeRecord();
		typo.m_sPaper = "#ZZZZZZ";
		typo.m_sInk = "#1C1A16";
		typo.CollectBadColors(bad);
		ctx.Equal(1, bad.Count(), "one malformed colour is reported");
		if (bad.Count() > 0)
			ctx.EqualStr("paper=#ZZZZZZ", bad[0], "the report names the field and the value");
	}

	//------------------------------------------------------------------------------------------------
	//! The flagship look in code: the 1989 values the issue says must not move.
	protected void CheckBuiltIn(LF_TestContext ctx)
	{
		LF_UIThemeRecord paper = LF_UITheme.BuiltIn(LF_UIThemeConfig.BASE_ID);
		CheckColor(ctx, 238, 230, 212, paper.m_sPaper, "paper");
		CheckColor(ctx, 28, 26, 22, paper.m_sInk, "ink");
		CheckColor(ctx, 122, 102, 64, paper.m_sRule, "brass rule");
		CheckColor(ctx, 62, 64, 58, paper.m_sMetal, "rim");
		CheckColor(ctx, 220, 208, 182, paper.m_sRow, "row strip");
		CheckColor(ctx, 168, 164, 156, paper.m_sBlur, "backdrop");
		CheckColor(ctx, 150, 40, 30, paper.m_sButtonFocus, "button focus");
		ctx.Equal(28, paper.m_iTitleSize, "title size");
		ctx.Equal(18, paper.m_iBodySize, "body size");
		ctx.Equal(20, paper.m_iButtonSize, "button size");
		ctx.Equal(18, paper.m_iFieldSize, "field size");
		ctx.Equal(16, paper.m_iCaptionSize, "caption size");
		ctx.Equal(15, paper.m_iHeadingSize, "heading size");
		ctx.True(paper.m_PaperTexture.IsEmpty(), "the 1989 sheet has no texture");

		LF_UIThemeRecord terminal = LF_UITheme.BuiltIn("terminal");
		CheckColor(ctx, 96, 236, 120, terminal.m_sInk, "terminal phosphor");
		CheckColor(ctx, 255, 176, 0, terminal.m_sButtonFocus, "terminal amber focus");
		ctx.EqualStr(LF_UITheme.FONT_MONO, terminal.m_Font, "the terminal is set in the mono face");

		LF_UIThemeRecord card = LF_UITheme.BuiltIn("id-card");
		CheckColor(ctx, 228, 233, 218, card.m_sPaper, "id card paper");
		CheckColor(ctx, 40, 66, 118, card.m_sAccent, "id card accent");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckColor(LF_TestContext ctx, int r, int g, int b, string hex, string what)
	{
		Color expected = Color.FromSRGBA(r, g, b, 255);
		Color actual = LF_UIThemeRecord.ParseColor(hex, null);
		ctx.NotNull(actual, what + " is a valid hex colour");
		if (actual)
			ctx.Equal(expected.PackToInt(), actual.PackToInt(), what + " is the 1989 value");
	}
}
