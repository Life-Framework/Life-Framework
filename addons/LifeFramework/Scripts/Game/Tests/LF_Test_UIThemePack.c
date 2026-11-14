// red-proof: observed 2026-11-14, cli test --no-build --only LF_Test_UIThemePack, with
// LF_UIThemeList.PatchFromContainer skipping m_sPaper: "[LFTEST]   - the pack's paper colour wins
// over core (expected -3219219, got -2438488)".

//------------------------------------------------------------------------------------------------
//! A stand-in for the pack source: no overlay (the flagship), or the test pack's theme overlay.
class LF_Test_UIThemePackSource : LF_ConfigPackSource
{
	static const ResourceName OVERLAY = "{B050696FB54746B9}Configs/Tests/LF_Test_UIThemePack.conf";

	protected bool m_bPack;

	void LF_Test_UIThemePackSource(bool pack)
	{
		m_bPack = pack;
	}

	override void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		if (m_bPack && coreConf == LF_UIThemeConfig.CONFIG_PATH)
			overlays.Insert(OVERLAY);
	}

	override string GetName()
	{
		return "test-ui-theme";
	}
};

//------------------------------------------------------------------------------------------------
//! The theme config end to end (#966): the core Configs/UI/LF_UITheme.conf is the 1989 look (the
//! flagship keeps it exactly), and a pack overlay patches a theme by id so every sheet that
//! inherits it, and only those, takes the new tokens. The fixture is Configs/Tests/LF_Test_UIThemePack.conf.
// tier: WORLD
class LF_Test_UIThemePack : LF_Test
{
	override string GetName()
	{
		return "ui-kit/theme-pack";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1}", "test");
		LF_ConfigService.SetPackSource(new LF_Test_UIThemePackSource(false));
		LF_UITheme.Reload();
		CheckFlagship(ctx);

		LF_ConfigService.SetPackSource(new LF_Test_UIThemePackSource(true));
		LF_UITheme.Reload();
		CheckPack(ctx);

		LF_ConfigService.Reset();
		LF_UITheme.Reload();
	}

	//------------------------------------------------------------------------------------------------
	//! No pack: every theme of the core file equals the built-in 1989 theme, field for field.
	protected void CheckFlagship(LF_TestContext ctx)
	{
		LF_UIThemeConfig config = LF_UIThemeConfig.Get();
		ctx.NotNull(config, "the core theme config loads");
		if (!config)
			return;

		array<string> ids = {"paper", "id-card", "terminal", "price-list"};
		foreach (string id : ids)
		{
			LF_UIThemeRecord core = config.Find(id);
			ctx.NotNull(core, "the core config has the theme " + id);
			if (!core)
				continue;

			LF_UIThemeRecord code = LF_UITheme.BuiltIn(id);
			ctx.EqualStr(code.m_sMetal, core.m_sMetal, id + " rim equals the built-in");
			ctx.EqualStr(code.m_sPaper, core.m_sPaper, id + " paper equals the built-in");
			ctx.EqualStr(code.m_sInk, core.m_sInk, id + " ink equals the built-in");
			ctx.EqualStr(code.m_sRule, core.m_sRule, id + " rule equals the built-in");
			ctx.EqualStr(code.m_sBlur, core.m_sBlur, id + " backdrop equals the built-in");
			ctx.EqualStr(code.m_sRow, core.m_sRow, id + " row strip equals the built-in");
			ctx.EqualStr(code.m_sButtonText, core.m_sButtonText, id + " button text equals the built-in");
			ctx.EqualStr(code.m_sButtonFocus, core.m_sButtonFocus, id + " button focus equals the built-in");
			ctx.EqualStr(code.m_sAccent, core.m_sAccent, id + " accent equals the built-in");
			ctx.EqualStr(code.m_Font, core.m_Font, id + " face equals the built-in");
			ctx.EqualStr(code.m_FontRegular, core.m_FontRegular, id + " regular face equals the built-in");
			ctx.EqualStr(code.m_PaperTexture, core.m_PaperTexture, id + " texture equals the built-in");
			ctx.Equal(code.m_iTitleSize, core.m_iTitleSize, id + " title size equals the built-in");
			ctx.Equal(code.m_iBodySize, core.m_iBodySize, id + " body size equals the built-in");
			ctx.Equal(code.m_iButtonSize, core.m_iButtonSize, id + " button size equals the built-in");
			ctx.Equal(code.m_iFieldSize, core.m_iFieldSize, id + " field size equals the built-in");
			ctx.Equal(code.m_iCaptionSize, core.m_iCaptionSize, id + " caption size equals the built-in");
			ctx.Equal(code.m_iHeadingSize, core.m_iHeadingSize, id + " heading size equals the built-in");
		}

		ctx.Equal(Color.FromSRGBA(238, 230, 212, 255).PackToInt(), LF_UISkin.Paper().m_Paper.PackToInt(), "the paper skin is the 1989 paper");
		ctx.Equal(Color.FromSRGBA(96, 236, 120, 255).PackToInt(), LF_UISkin.Terminal().m_Ink.PackToInt(), "the terminal skin is green phosphor");
		ctx.Equal(28, LF_UITheme.TitleSize(), "the title size is 28");
		ctx.True(LF_UISkin.Paper().m_Accent == null, "the paper skin has no accent");
	}

	//------------------------------------------------------------------------------------------------
	//! The test pack patches "paper" (palette, accent, title size) and "terminal" (ink, rule).
	protected void CheckPack(LF_TestContext ctx)
	{
		LF_UISkin paper = LF_UISkin.Paper();
		ctx.Equal(Color.FromSRGBA(232, 241, 247, 255).PackToInt(), paper.m_Paper.PackToInt(), "the pack's paper colour wins over core");
		ctx.Equal(Color.FromSRGBA(11, 22, 32, 255).PackToInt(), paper.m_Ink.PackToInt(), "the pack's ink wins over core");
		ctx.Equal(Color.FromSRGBA(255, 90, 31, 255).PackToInt(), paper.m_ButtonFocus.PackToInt(), "the pack's button focus wins over core");
		ctx.NotNull(paper.m_Accent, "the pack gives the paper theme an accent");
		ctx.EqualStr("0,184,169,255", paper.m_sAccentRgba, "the accent reaches the rich-text markup");
		ctx.Equal(32, LF_UITheme.TitleSize(), "the pack's title size wins over core");
		ctx.Equal(18, LF_UITheme.BodySize(), "a size the pack leaves out stays core");
		ctx.EqualStr(LF_UITheme.FONT, LF_UITheme.Font(), "a face the pack leaves out stays core");
		ctx.True(paper.m_PaperTexture.IsEmpty(), "the pack asks for no paper texture");

		// A theme that inherits the paper theme follows it.
		ctx.Equal(paper.m_Paper.PackToInt(), LF_UISkin.PriceList().m_Paper.PackToInt(), "the price list inherits the pack's paper");
		ctx.EqualStr(LF_UITheme.FONT_MONO, LF_UISkin.PriceList().m_Font, "the price list keeps its own mono face");

		// A theme the pack patches keeps what the pack leaves out.
		LF_UISkin terminal = LF_UISkin.Terminal();
		ctx.Equal(Color.FromSRGBA(66, 214, 255, 255).PackToInt(), terminal.m_Ink.PackToInt(), "the pack recolours the terminal's ink");
		ctx.Equal(Color.FromSRGBA(6, 14, 8, 255).PackToInt(), terminal.m_Paper.PackToInt(), "a terminal colour the pack leaves out stays core");
		ctx.Equal(Color.FromSRGBA(255, 176, 0, 255).PackToInt(), terminal.m_ButtonFocus.PackToInt(), "the terminal keeps its amber focus");

		// A theme the pack does not name keeps its own look: the id card is not restyled by "paper".
		LF_UISkin card = LF_UISkin.IdCard();
		ctx.Equal(Color.FromSRGBA(228, 233, 218, 255).PackToInt(), card.m_Paper.PackToInt(), "the id card keeps its registry paper");
		ctx.Equal(Color.FromSRGBA(40, 66, 118, 255).PackToInt(), card.m_Accent.PackToInt(), "the id card keeps its state-blue accent");
	}
};
