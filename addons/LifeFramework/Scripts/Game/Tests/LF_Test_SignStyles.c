// red-proof: `tools\cli test --no-build --red LF_Test_SignFacePlan,LF_Test_SignStyles` (2026-11-18): RED-OK signs/styles ("LF_SignStyles.conf loads" failed with the hook on).

//! The sign style catalog (docs/architecture/sign-system.md): LF_SignStyles.conf loads with the chalk
//! style and the 16:9 size, their layouts and carrier resolve, the sample chalk sign carries its rows
//! and style, a sign's own style wins over the prop's, and a pack overlay patches and adds styles by id.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SignStyles : LF_Test
{
	static const ResourceName OVERLAY = "{99B6F807B2B1206E}Configs/Tests/LF_Test_SignStylesOverlay.conf";

	override string GetName()
	{
		return "signs/styles";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Loads(ResourceName name)
	{
		if (name.IsEmpty())
			return false;

		Resource resource = Resource.Load(name);
		return resource && resource.IsValid();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SignStyleConfig config;
		if (!RedProof())
			config = LF_SignStyles.Load(LF_SignStyles.CONFIG_PATH);
		ctx.NotNull(config, "LF_SignStyles.conf loads");
		if (!config)
			return;

		LF_SignStyleDef chalk = LF_SignStyles.FindStyleIn(config, "chalk");
		ctx.NotNull(chalk, "the chalk style is in the core config");
		if (chalk)
		{
			ctx.True(Loads(chalk.m_sLayout), "the chalk face layout resolves");
			ctx.True(Loads(chalk.m_sRowLayout), "the chalk row layout resolves");
			ctx.True(chalk.m_fInkAlpha > 0 && chalk.m_fInkAlpha < 1, "chalk ink lets the slate show through");
		}

		LF_SignSizeDef quad = LF_SignStyles.FindSizeIn(config, "quad16x9");
		ctx.NotNull(quad, "the 16:9 size is in the core config");
		if (quad)
		{
			ctx.True(Loads(quad.m_sCarrier), "its carrier prefab resolves");
			ctx.True(quad.m_vShown[0] > 0 && quad.m_vShown[1] > 0, "it has a shown area");
		}

		LF_SignDef sample = LF_SignCatalog.Find("chalk_sample");
		ctx.NotNull(sample, "the sample chalk sign is in the catalog");
		if (sample)
		{
			ctx.EqualStr("chalk", sample.m_sStyle, "the sample sign names the chalk style");
			ctx.True(sample.m_aRows && sample.m_aRows.Count() == 4, "the sample sign carries four price rows");
			ctx.EqualStr("chalk", LF_SignComponent.StyleIdFor(sample, "poster"), "a sign's own style wins over the prop's");
		}

		LF_SignDef plain = new LF_SignDef();
		ctx.EqualStr("chalk", LF_SignComponent.StyleIdFor(plain, "chalk"), "a sign with no style takes the prop's");
		ctx.EqualStr("", LF_SignComponent.StyleIdFor(plain, ""), "neither: the legacy face");

		Resource resource = BaseContainerTools.LoadContainer(OVERLAY);
		ctx.True(resource && resource.IsValid() && resource.GetResource(), "the overlay fixture loads");
		if (!resource || !resource.IsValid() || !resource.GetResource())
			return;

		int changed = config.ApplyContainer(resource.GetResource().ToBaseContainer(), OVERLAY);
		ctx.Equal(2, changed, "the overlay patches one style and adds one");
		LF_SignStyleDef patched = LF_SignStyles.FindStyleIn(config, "chalk");
		ctx.EqualFloat(0.5, patched.m_fInkAlpha, 0.001, "patch: chalk takes the overlay's ink alpha");
		ctx.EqualFloat(200, patched.m_iTitleFontMax, 0.001, "and keeps the core title font it did not set");
		ctx.NotNull(LF_SignStyles.FindStyleIn(config, "test_pack_style"), "add: a pack style joins the catalog");
	}
}
