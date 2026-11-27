// red-proof: `tools\cli test --no-build --red LF_Test_ShopListRows` puts the old 170-unit height (two or three 76-unit rows) on the check and fails
// both row-count assertions.
//! 2026-11-27 UI sweep: a shop's price list and the car lot's list show 6 to 8 whole rows at once, not two. Rows are the kit's compact rows
//! (39 units); the list height is eight of them, and the sheet's screen cap squeezes it to six when the slip's preview is open.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ShopListRows : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "shop/list-rows";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		float shopHeight = LF_ShopPriceList.LIST_HEIGHT;
		float lotHeight = LF_VehicleLotSheet.LIST_HEIGHT;
		if (RedProof())
		{
			shopHeight = 170;
			lotHeight = 170;
		}

		float shopRows = shopHeight / LF_UIScreen.COMPACT_ROW_PITCH;
		float lotRows = lotHeight / LF_UIScreen.COMPACT_ROW_PITCH;
		ctx.True(shopRows >= 6 && shopRows <= 9, string.Format("the shop list holds 6 to 9 compact rows (%1)", shopRows));
		ctx.True(lotRows >= 6 && lotRows <= 9, string.Format("the car lot list holds 6 to 9 compact rows (%1)", lotRows));

		// A compact list takes a shop's rows and keeps their action buttons by name.
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.CreateList("Prices").Compact());
		LF_UIScreen list = LF_UIScreen.Open(parts, LF_UISkin.PriceList());
		ctx.NotNull(list, "the compact price list assembles");
		if (!list)
			return;

		ctx.True(list.SetListHeight("Prices", LF_ShopPriceList.LIST_HEIGHT), "the compact price list sets its height");
		for (int i = 0; i < 10; i++)
			ctx.True(list.AddListRow("Prices", LF_ShopPriceList.RowName(i), "Water", "$6", "#LF-Shop_Order"), "a compact price line is added " + i.ToString());

		ctx.NotNull(list.GetOnButton(), "the compact rows report their buttons");
		list.Close();
	}
}
