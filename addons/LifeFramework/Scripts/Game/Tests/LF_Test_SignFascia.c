// red-proof: `tools\cli test --no-build --red LF_Test_SignFascia` (2026-11-18): RED-OK signs/fascia ("LF_SignStyles.conf loads" failed with the hook on).

//! The shop fascia style (docs/architecture/sign-system.md, S3): the `fascia` style and `fascia` size
//! resolve (the vanilla body is the face, no carrier, 1024 x 213 shown), the style's layouts load with
//! the widgets the painter needs, a placement's paint and lettering colours override the style's ground
//! and ink only on a style that takes placement colours, and LF_Sign_Fascia carries the style and size.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SignFascia : LF_Test
{
	static const ResourceName FASCIA_PREFAB = "{4697D9464B596C24}Prefabs/Props/Signs/LF_Sign_Fascia.et";

	override string GetName()
	{
		return "signs/fascia";
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

		LF_SignStyleDef fascia = LF_SignStyles.FindStyleIn(config, "fascia");
		ctx.NotNull(fascia, "the fascia style is in the core config");
		if (!fascia)
			return;

		ctx.True(Loads(fascia.m_sLayout), "the fascia face layout resolves");
		ctx.True(Loads(fascia.m_sRowLayout), "the fascia row layout resolves");
		ctx.True(fascia.m_bPlacementColors, "the fascia takes its colours from the placement");
		ctx.True(fascia.m_bUpperTitle, "fascia lettering is capitals");
		ctx.True(fascia.m_fGhostAlpha > 0.3, "the fascia has a drop shadow you can see");
		ctx.True(fascia.m_fInkAlpha > 0.99, "painted lettering is opaque");

		LF_SignSizeDef size = LF_SignStyles.FindSizeIn(config, "fascia");
		ctx.NotNull(size, "the fascia size is in the core config");
		if (size)
		{
			ctx.True(size.m_sCarrier.IsEmpty(), "the vanilla body is the face: no carrier");
			ctx.EqualFloat(1024, size.m_vShown[0], 0.01, "the face shows 1024 units across");
			ctx.EqualFloat(213, size.m_vShown[1], 0.01, "and 213 down");
		}

		// Placement colours: the fascia takes them, the chalk style and an unpainted prop do not.
		vector paint = "0.42 0.07 0.07";
		vector text = "0.92 0.78 0.42";
		vector ground;
		vector ink;
		LF_SignStyledFace.FaceColors(fascia, true, paint, text, ground, ink);
		ctx.EqualFloat(0.42, ground[0], 0.001, "a placed fascia's paint colour is the ground");
		ctx.EqualFloat(0.78, ink[1], 0.001, "and its lettering colour is the ink");

		LF_SignStyledFace.FaceColors(fascia, false, paint, text, ground, ink);
		ctx.EqualFloat(fascia.m_vGround[0], ground[0], 0.001, "an unpainted prop keeps the style ground");

		LF_SignStyleDef chalk = LF_SignStyles.FindStyleIn(config, "chalk");
		if (chalk)
		{
			LF_SignStyledFace.FaceColors(chalk, true, paint, text, ground, ink);
			ctx.EqualFloat(chalk.m_vGround[0], ground[0], 0.001, "chalk ignores a painted prop's colours");
			ctx.EqualFloat(chalk.m_vInk[0], ink[0], 0.001, "chalk keeps its own ink");
		}

		// An all-zero ghost colour means the ink; a set one is the shadow colour.
		vector shadow = LF_SignStyledFace.ShadowColor(fascia, ink);
		ctx.True(shadow[0] < 0.2 && shadow[1] < 0.2 && shadow[2] < 0.2, "the fascia shadow is dark");
		LF_SignStyleDef plain = new LF_SignStyleDef();
		vector tint = "0.5 0.6 0.7";
		vector same = LF_SignStyledFace.ShadowColor(plain, tint);
		ctx.EqualFloat(0.6, same[1], 0.001, "a style with no ghost colour shadows in its ink");

		// The prop: same GUID, now styled.
		Resource resource = Resource.Load(FASCIA_PREFAB);
		ctx.True(resource && resource.IsValid(), "LF_Sign_Fascia loads under its original GUID");
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity prop = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.NotNull(prop, "LF_Sign_Fascia spawns");
		if (!prop)
			return;

		LF_SignComponent sign = LF_Component<LF_SignComponent>.Find(prop);
		ctx.NotNull(sign, "it carries LF_SignComponent");
		if (sign)
		{
			ctx.EqualStr("fascia", sign.GetStyleId(), "the prop names the fascia style");
			ctx.EqualStr("fascia", sign.GetSizeId(), "and the fascia size");
			ctx.True(sign.HasPaintedFace(), "it still has a painted face, so placement colours apply");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(prop);
	}
}
