// red-proof: RedProof() reads the sign rows without the pack's specials, so the store and market boards carry no rows and every check below fails.
//! everon/pavement-boards (#3305): under the Everon 1989 header (the pack leg), the general store and
//! the market A-frames are chalk signs with the place's name and two or three specials, each a label
//! the pack's strings resolve and a price. The store board's prices are the store shelf's (#3547): each
//! row's price equals the general_store stock price of the item it names, so the board cannot drift.
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
	//! Row i of the general_store board names this item (chocolate bar, fresh milk, tinned fish).
	protected void CheckStorePrices(LF_TestContext ctx)
	{
		array<ResourceName> items = {
			"{F407598417C980D2}Prefabs/Items/Store1989/LF_ChocolateBar.et",
			"{D77ECA12D070F735}Prefabs/Items/Store1989/LF_MilkBottle.et",
			"{6FD0FBD61A6F7B0D}Prefabs/Items/Store1989/LF_CannedFish.et"
		};

		LF_SignDef sign = LF_SignCatalog.Find("general_store");
		LF_ShopStock stock = null;
		if (!RedProof())
			stock = LF_ShopStockConfig.Get().Find("general_store");

		ctx.NotNull(stock, "the general_store stock exists");
		if (!sign || !sign.m_aRows || !stock)
			return;

		ctx.Equal(items.Count(), sign.m_aRows.Count(), "the store board has one row per item the test names");
		for (int i = 0; i < items.Count() && i < sign.m_aRows.Count(); i++)
		{
			int shelf = 0;
			foreach (LF_ShopStockItem row : stock.m_aItems)
			{
				if (row && row.m_Prefab == items[i])
					shelf = row.m_iPrice;
			}

			ctx.True(shelf > 0, "store board row " + i + " is an item the store shelf sells");
			ctx.Equal(shelf, sign.m_aRows[i].m_sValue.ToInt(), "store board row " + i + " shows the shelf price");
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckBoard(ctx, "general_store");
		CheckBoard(ctx, "traders");
		CheckStorePrices(ctx);
	}
}
