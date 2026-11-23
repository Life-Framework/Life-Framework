// red-proof: `tools\cli test --no-build --red LF_Test_SignStylesLivery` (2026-11-23, #4468): RED-OK signs/styles-livery (the hook skips the family merge, so the four livery ids are missing).

//! The livery sign styles (#4468): block, stripe, stencil and mud live in Configs/Signs/LF_SignStyles_Livery.conf,
//! a family file beside the core LF_SignStyles.conf. The loader merges every LF_SignStyles_<Family>.conf, so the
//! four ids resolve beside the core ones (chalk, poster) and each names layouts that load. Lettering is US English
//! capitals.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SignStylesLivery : LF_Test
{
	override string GetName()
	{
		return "signs/styles-livery";
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
		LF_SignStyleConfig core = LF_SignStyles.Load(LF_SignStyles.CONFIG_PATH);
		ctx.NotNull(core, "LF_SignStyles.conf loads");
		if (!core)
			return;

		ctx.True(!LF_SignStyles.FindStyleIn(core, "block"),"the core file alone holds no livery style: they come from the family file");

		LF_SignStyleConfig config = core;
		if (!RedProof())
			LF_SignStyles.ApplyFamilyFiles(config);

		array<ResourceName> files = {};
		LF_SignStyles.FamilyFiles(files);
		bool foundLivery;
		foreach (ResourceName file : files)
		{
			if (file.GetPath().EndsWith("LF_SignStyles_Livery.conf"))
				foundLivery = true;
		}

		ctx.True(foundLivery, "the family search finds LF_SignStyles_Livery.conf");
		ctx.NotNull(LF_SignStyles.FindStyleIn(config, "chalk"), "a style from LF_SignStyles.conf resolves");
		ctx.NotNull(LF_SignStyles.FindStyleIn(config, "poster"), "a second core style resolves");

		array<string> ids = { "block", "stripe", "stencil", "mud" };
		foreach (string id : ids)
		{
			LF_SignStyleDef style = LF_SignStyles.FindStyleIn(config, id);
			ctx.NotNull(style, "livery style " + id + " resolves from LF_SignStyles_Livery.conf");
			if (!style)
				continue;

			ctx.True(Loads(style.m_sLayout), id + ": the face layout resolves");
			ctx.True(Loads(style.m_sRowLayout), id + ": the row layout resolves");
			ctx.True(style.m_bUpperTitle, id + ": lettering is capitals");
			ctx.True(style.m_sLayout.GetPath().Contains("LF_SignStyle_Livery"), id + ": its layout is a livery layout");
		}

		LF_SignStyleDef block = LF_SignStyles.FindStyleIn(config, "block");
		LF_SignStyleDef stripe = LF_SignStyles.FindStyleIn(config, "stripe");
		LF_SignStyleDef stencil = LF_SignStyles.FindStyleIn(config, "stencil");
		LF_SignStyleDef mud = LF_SignStyles.FindStyleIn(config, "mud");
		if (!block || !stripe || !stencil || !mud)
			return;

		ctx.True(block.m_sLayout != stripe.m_sLayout && stripe.m_sLayout != stencil.m_sLayout && stencil.m_sLayout != mud.m_sLayout && mud.m_sLayout != block.m_sLayout, "each style has its own face layout");
		ctx.True(block.m_bPlacementColors && stripe.m_bPlacementColors && mud.m_bPlacementColors, "block, stripe and mud take the vehicle's plate colours");
		ctx.False(stencil.m_bPlacementColors, "stencil is fixed olive drab");
		ctx.True(mud.m_fGhostAlpha > 0, "mud smears a ghost of the lettering");
		ctx.True(mud.m_fInkAlpha < 1, "mud ink is dulled by dirt");
		ctx.EqualFloat(1, block.m_fInkAlpha, 0.001, "block ink is solid");

		// Re-applying the family files is idempotent: a known id patches, it does not duplicate.
		int before = config.m_aStyles.Count();
		LF_SignStyles.ApplyFamilyFiles(config);
		ctx.Equal(before, config.m_aStyles.Count(), "merging the family files again adds no second row");
	}
}
