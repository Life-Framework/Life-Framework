// red-proof: RedProof() skips the row lookups, so the gang registry and bar rows read as missing and every check below fails.
//! everon/map-poi-kinds-crew (#4701): under the Everon 1989 header (the pack leg), the POI table has a public row
//! for the gang registry (`OrgDesk`) and one for the village bars (`PubBar`: Chotain and Levie), each with an icon
//! and a pack string that resolves, so both show on the public map. The under-the-counter trade has no row.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronMapPoiKindsCrew : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/map-poi-kinds-crew";
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckKind(LF_TestContext ctx, LF_MapPoiConfig config, string kind, string label, string icon)
	{
		LF_MapPoiKind row;
		if (!RedProof())
			row = config.Find(kind);

		ctx.NotNull(row, string.Format("the pack has a POI row for the %1 kind", kind));
		if (!row)
			return;

		ctx.True(row.m_eVisibility == LF_EMapPoiVisibility.PUBLIC, string.Format("%1 shows on the public map", kind));
		ctx.EqualStr(label, row.m_sLabel, string.Format("%1 is named by its own string", kind));
		ctx.EqualStr(icon, row.m_sIcon, string.Format("%1 has its own icon", kind));
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

		CheckKind(ctx, config, "OrgDesk", "#LF-MapPoi_OrgDesk", "poi_Board");
		CheckKind(ctx, config, "PubBar", "#LF-MapPoi_PubBar", "poi_Pub");
	}
}
