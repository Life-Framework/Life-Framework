// red-proof: before any sign was placed, `tools\cli test --no-build --only EL_Test_SignCoverage`
// (2026-11-08) failed signs/coverage with "catalog sign 'police_whitelist' is placed in DebugWorld".

// tier: WORLD
class EL_Test_SignCoverage : EL_Test
{
	protected static ref array<string> s_aIds;
	protected static ref array<string> s_aUnusable;

	override string GetName()
	{
		return "signs/coverage";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectSign(IEntity entity)
	{
		if (!entity)
			return true;

		LF_SignComponent sign = LF_SignComponent.Cast(entity.FindComponent(LF_SignComponent));
		if (!sign)
			return true;

		s_aIds.Insert(sign.GetSignId());
		if (!LF_SignCatalog.IsUsable(sign.GetSign()))
			s_aUnusable.Insert(string.Format("'%1' at %2", sign.GetSignId(), entity.GetOrigin()));

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		s_aIds = {};
		s_aUnusable = {};
		BaseWorld world = GetGame().GetWorld();
		ctx.True(world != null, "DebugWorld is loaded");
		if (!world)
			return;

		world.QueryEntitiesBySphere("256 0 256", 1000, CollectSign, null, EQueryEntitiesFlags.ALL);
		ctx.True(s_aIds.Count() > 0, "DebugWorld places signs");

		foreach (string unusable : s_aUnusable)
		{
			ctx.True(false, "placed sign " + unusable + " resolves a titled LF_Signs.conf entry");
		}

		LF_SignConfig config = LF_SignCatalog.Load(LF_SignCatalog.CONFIG_PATH);
		ctx.NotNull(config, "LF_Signs.conf loads");
		if (!config || !config.m_aSigns)
			return;

		foreach (LF_SignDef def : config.m_aSigns)
		{
			ctx.True(s_aIds.Contains(def.m_sId), string.Format("catalog sign '%1' is placed in DebugWorld", def.m_sId));
			ExpectTranslated(ctx, def.m_sId, "title", def.m_sTitle);
			ExpectTranslated(ctx, def.m_sId, "what", def.m_sWhat);
		}

		s_aIds = null;
		s_aUnusable = null;
	}

	//------------------------------------------------------------------------------------------------
	//! A section is set, and a #key resolves to text instead of echoing the key.
	protected void ExpectTranslated(EL_TestContext ctx, string id, string field, string value)
	{
		string bare = value;
		bare.Replace("#", "");
		string text = LF_SignCatalog.Resolve(value);
		bool ok = !value.IsEmpty() && !text.IsEmpty() && text != value && text != bare;
		if (!value.IsEmpty() && !LF_SignCatalog.IsKey(value))
			ok = true;

		ctx.True(ok, string.Format("sign '%1' %2 has text (%3)", id, field, value));
	}
}
