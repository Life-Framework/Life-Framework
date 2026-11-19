// red-proof: RedProof() skips the lantern spawn, the sign lookups and the POI row, so every check below fails.
//! everon/fishing-places (#3667): under the Everon 1989 header (the pack leg), the Black Lake plate is in the sign catalog
//! (its own folder file) with strings that resolve, the FishingSpot place kind has a map POI row named by a pack string, and the Black Lake
//! lantern is a lamp prefab that spawns.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronFishingPlaces : LF_Test
{
	protected static const ResourceName LANTERN = "{57354E1D82BCF650}Prefabs/Props/Fishing/LF_LakeLantern.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/fishing-places";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckSign(ctx, "black-lake");

		LF_MapPoiConfig.Reload();
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		ctx.NotNull(config, "the POI table loads under the pack");
		if (config)
		{
			LF_MapPoiKind row;
			if (!RedProof())
				row = config.Find("FishingSpot");

			ctx.NotNull(row, "the pack has a POI row for the FishingSpot kind");
			if (row)
			{
				string caption = WidgetManager.Translate(row.m_sLabel);
				ctx.True(caption != row.m_sLabel && !caption.IsEmpty(), "the fishing spot name is in the string table");
			}
		}

		IEntity lantern;
		if (!RedProof())
			lantern = LF_Utils.SpawnEntityPrefab(LANTERN, Vector(0, 500, 0));

		ctx.NotNull(lantern, "the lake lantern prefab spawns");
		if (lantern)
		{
			ctx.NotNull(SCR_LampComponent.Cast(lantern.FindComponent(SCR_LampComponent)), "the lantern is a lamp");
			SCR_EntityHelper.DeleteEntityAndChildren(lantern);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSign(LF_TestContext ctx, string id)
	{
		LF_SignDef sign;
		if (!RedProof())
			sign = LF_SignCatalog.Find(id);

		ctx.NotNull(sign, "the " + id + " plate is in the sign catalog");
		if (sign)
		{
			ctx.True(LF_SignCatalog.ResolveTitle(sign.m_sTitle) != "", id + " has a title to print");
			ctx.True(LF_SignCatalog.Resolve(sign.m_sWhat) != sign.m_sWhat, id + " text resolves to the pack's string");
		}
	}
}
