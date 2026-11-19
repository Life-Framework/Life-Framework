// red-proof: the carrier kit (#3307) had no size rows: with LF_SignStyles.conf as it stood, `tools\cli test --no-build --only LF_Test_SignCarriers`
// failed "the quad1x1 size is in the core config" (and the five rows after it) before the rows and carriers existed.

//! The carrier kit (docs/architecture/sign-system.md "Sizes"): one-mesh quads at six aspects beside the 16:9 one. Every
//! `quad*` size row in LF_SignStyles.conf resolves its carrier prefab, the aspect its shown area keeps is the aspect
//! its id names, the render target keeps that aspect (no stretched lettering), and the carrier mesh itself is that
//! shape. A row may name only the id's own ratio, so a size added later is held to the same rule.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SignCarriers : LF_Test
{
	override string GetName()
	{
		return "signs/carriers";
	}

	//------------------------------------------------------------------------------------------------
	//! "quad4x3" gives 4 over 3; 0 when the id is not quad<W>x<H>. The 16:9 row reads "quad16x9".
	static float AspectOfId(string id)
	{
		if (!id.StartsWith("quad"))
			return 0;

		string rest = id.Substring(4, id.Length() - 4);
		int cut = rest.IndexOf("x");
		if (cut <= 0)
			return 0;

		float w = rest.Substring(0, cut).ToFloat();
		float h = rest.Substring(cut + 1, rest.Length() - cut - 1).ToFloat();
		if (w <= 0 || h <= 0)
			return 0;

		return w / h;
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

		array<string> expected = { "quad1x1", "quad4x3", "quad3x2", "quad2x1", "quad4x1", "quad1x2", "quad16x9" };
		foreach (string id : expected)
		{
			ExpectCarrier(ctx, config, id);
		}

		// Every quad row there is, listed or not, keeps its id's shape.
		int quads;
		foreach (LF_SignSizeDef size : config.m_aSizes)
		{
			if (!size || !size.m_sId.StartsWith("quad"))
				continue;

			quads++;
			float aspect = AspectOfId(size.m_sId);
			ctx.True(aspect > 0, "the size id names its ratio: " + size.m_sId);
			if (aspect > 0 && size.m_vShown[1] > 0)
				ctx.EqualFloat(aspect, size.m_vShown[0] / size.m_vShown[1], aspect * 0.01, "shown area keeps the id's aspect: " + size.m_sId);
		}

		ctx.True(quads >= expected.Count(), "the kit has at least its seven quad rows");
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpectCarrier(LF_TestContext ctx, LF_SignStyleConfig config, string id)
	{
		LF_SignSizeDef size = LF_SignStyles.FindSizeIn(config, id);
		ctx.NotNull(size, "the " + id + " size is in the core config");
		if (!size)
			return;

		float aspect = AspectOfId(id);
		ctx.True(aspect > 0, id + " names a ratio");
		if (aspect <= 0)
			return;

		ctx.True(size.m_vShown[0] > 0 && size.m_vShown[1] > 0, id + " has a shown area");
		if (size.m_vShown[1] > 0)
			ctx.EqualFloat(aspect, size.m_vShown[0] / size.m_vShown[1], aspect * 0.01, id + " shown area keeps the aspect");
		if (size.m_vTarget[1] > 0)
			ctx.EqualFloat(aspect, size.m_vTarget[0] / size.m_vTarget[1], aspect * 0.01, id + " render target keeps the aspect, so lettering is not stretched");

		ctx.True(!size.m_sCarrier.IsEmpty(), id + " names a carrier prefab");
		if (size.m_sCarrier.IsEmpty())
			return;

		Resource resource = Resource.Load(size.m_sCarrier);
		ctx.True(resource && resource.IsValid(), id + " carrier prefab resolves");
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity carrier = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.NotNull(carrier, id + " carrier spawns");
		if (!carrier)
			return;

		// The mesh faces +X: width along z, height along y (LF_BoardFace.xob). A mesh that did not load has no bounds.
		vector mins, maxs;
		carrier.GetBounds(mins, maxs);
		float width = maxs[2] - mins[2];
		float height = maxs[1] - mins[1];
		ctx.True(width > 0.01 && height > 0.01, id + " carrier has a mesh");
		if (width > 0.01 && height > 0.01)
			ctx.EqualFloat(aspect, width / height, aspect * 0.01, id + " carrier mesh is the id's shape");

		SCR_EntityHelper.DeleteEntityAndChildren(carrier);
	}
}
