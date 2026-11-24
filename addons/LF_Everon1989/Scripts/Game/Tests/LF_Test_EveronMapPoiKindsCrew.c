// red-proof: RedProof() skips the row lookups, so the bar row reads as missing and every check below fails.
//! everon/map-poi-kinds-crew (#4701): under the Everon 1989 header (the pack leg), the POI table has a public row
//! for the village bars (`PubBar`: Chotain and Levie) with its own icon and a pack string that resolves, so both bars
//! show on the public map. The gang registry (`OrgDesk`) and the labor office (`LabourOffice`) have public rows too,
//! each with its own pictogram badge (#4748).
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

		CheckKind(ctx, config, "PubBar", "#LF-MapPoi_PubBar", "poi_Pub");
		CheckKind(ctx, config, "OrgDesk", "#LF-MapPoi_OrgDesk", "poi_OrgDesk");
		CheckKind(ctx, config, "LabourOffice", "#LF-MapPoi_LabourOffice", "poi_LabourOffice");
	}
}
