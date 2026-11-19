// red-proof: RedProof() skips the sign lookup and drops the Smokehouse row from the POI table, so the plate and the pin read as missing and every check below fails.
//! everon/smokehouse-site (#3453): under the Everon 1989 header (the pack leg), the dock smokehouse has a pole
//! plate in the sign catalog (its own folder file) and a map POI row for the Smokehouse place kind that the
//! fitted slot's place marker registers, named by a pack string that resolves.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronSmokehouseSite : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/smokehouse-site";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SignDef sign;
		if (!RedProof())
			sign = LF_SignCatalog.Find("smokehouse");

		ctx.NotNull(sign, "the smokehouse plate is in the sign catalog");
		if (sign)
		{
			ctx.True(LF_SignCatalog.ResolveTitle(sign.m_sTitle) != "", "the plate has a title to print");
			ctx.True(LF_SignCatalog.Resolve(sign.m_sWhat) != sign.m_sWhat, "its text resolves to the pack's string");
		}

		LF_MapPoiConfig.Reload();
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		ctx.NotNull(config, "the POI table loads under the pack");
		if (!config)
			return;

		LF_MapPoiKind row;
		if (!RedProof())
			row = config.Find("Smokehouse");

		ctx.NotNull(row, "the pack has a POI row for the Smokehouse kind");
		if (!row)
			return;

		ctx.EqualStr("#LF-MapPoi_Smokehouse", row.m_sLabel, "the smokehouse is named by its own string");
		string caption = WidgetManager.Translate(row.m_sLabel);
		ctx.True(caption != row.m_sLabel && !caption.IsEmpty(), "the smokehouse name is in the string table");
	}
}
