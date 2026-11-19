// red-proof: RedProof() skips the row lookups, so the mine and plant rows read as missing and every check below fails.
//! everon/mine-plant-pois (#3241): under the Everon 1989 header (the pack leg), the POI table has a public work row
//! for the `mine` kind and one for the `plant` kind, each named by a pack string that resolves.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronMinePlantPois : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/mine-plant-pois";
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckKind(LF_TestContext ctx, LF_MapPoiConfig config, string kind, string label)
	{
		LF_MapPoiKind row;
		if (!RedProof())
			row = config.Find(kind);

		ctx.NotNull(row, string.Format("the pack has a POI row for the %1 kind", kind));
		if (!row)
			return;

		ctx.EqualStr(label, row.m_sLabel, string.Format("%1 is named by its own string", kind));
		ctx.EqualStr("work", row.m_sCategory, string.Format("%1 sits in the work group", kind));
		ctx.True(row.m_eVisibility == LF_EMapPoiVisibility.PUBLIC, string.Format("%1 shows on the map once live", kind));
		string caption = WidgetManager.Translate(row.m_sLabel);
		ctx.True(caption != row.m_sLabel && !caption.IsEmpty(), string.Format("the %1 name is in the string table", kind));
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapPoiConfig.Reload();
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		ctx.NotNull(config, "the POI table loads under the pack");
		if (!config)
			return;

		CheckKind(ctx, config, "mine", "#LF-MapPoi_Mine");
		CheckKind(ctx, config, "plant", "#LF-MapPoi_Plant");
	}
}
