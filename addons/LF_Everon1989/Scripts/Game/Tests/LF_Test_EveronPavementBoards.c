// red-proof: RedProof() reads the sign rows without the pack's specials, so the store and market boards carry no rows and every check below fails.
//! everon/pavement-boards (#3305): under the Everon 1989 header (the pack leg), the general store and
//! the market A-frames are chalk signs with the place's name and two or three specials, each a label
//! the pack's strings resolve and a price.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronPavementBoards : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/pavement-boards";
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBoard(LF_TestContext ctx, string id)
	{
		LF_SignDef sign = LF_SignCatalog.Find(id);
		ctx.NotNull(sign, "the sign " + id + " is in the catalog");
		if (!sign)
			return;

		ctx.EqualStr("chalk", sign.m_sStyle, id + " is a chalk sign");
		ctx.True(LF_SignCatalog.ResolveTitle(sign.m_sTitle) != "", id + " has a title to print");

		int rows;
		if (sign.m_aRows && !RedProof())
			rows = sign.m_aRows.Count();

		ctx.True(rows >= 2 && rows <= 3, id + " lists two or three specials, has " + rows);
		for (int i = 0; i < rows; i++)
		{
			LF_SignRow row = sign.m_aRows[i];
			ctx.True(LF_SignCatalog.Resolve(row.m_sLabel) != "" && LF_SignCatalog.Resolve(row.m_sLabel) != row.m_sLabel, id + " row " + i + " label resolves to the pack's text");
			ctx.True(row.m_sValue.ToInt() > 0, id + " row " + i + " has a price");
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckBoard(ctx, "general_store");
		CheckBoard(ctx, "traders");
	}
}
