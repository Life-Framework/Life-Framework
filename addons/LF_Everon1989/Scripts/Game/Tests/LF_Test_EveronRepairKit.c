// red-proof: new test with no failing state; `tools\cli test --no-build --red LF_Test_EveronRepairKit` must print `RED-OK everon/repair-kit` (the RedProof() hook probes the stocks with an empty prefab, so every row check fails).
//! #3334: the 1989 flagship sells the repair kit (#3332) at the mechanic's garage and the hardware store
//! for less than a tow (`LF_Garage.TowFee()`), and one kit restores part of a car's health, not all of it
//! (a wreck gets home, it does not come back new).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronRepairKit : LF_Test
{
	protected static const ResourceName KIT = "{3F5A9C1E7B2D4806}Prefabs/Items/Vehicles/LF_RepairKit.et";
	protected static const int PRICE_HARDWARE = 90;
	protected static const int PRICE_MECHANIC = 80;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/repair-kit";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ResourceName probe = KIT;
		if (RedProof())
			probe = "";

		CheckOutlet(ctx, "hardware", probe, PRICE_HARDWARE);
		CheckOutlet(ctx, "mechanic", probe, PRICE_MECHANIC);

		int tow = LF_GarageConfig.TowFee();
		ctx.True(PRICE_HARDWARE < tow, string.Format("a kit at the hardware store ($%1) costs less than a tow ($%2)", PRICE_HARDWARE, tow));
		ctx.True(PRICE_MECHANIC < tow, string.Format("a kit at the garage ($%1) costs less than a tow ($%2)", PRICE_MECHANIC, tow));

		// The amount: the kit's configured share is real but leaves a wreck short of new.
		vector at = "160 0 80";
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + 1;
		IEntity kit = LF_Utils.SpawnEntityPrefab(KIT, at);
		ctx.NotNull(kit, "the repair kit prefab spawns");
		if (!kit)
			return;

		LF_RepairKitComponent comp = LF_Component<LF_RepairKitComponent>.Find(kit);
		ctx.NotNull(comp, "the kit carries LF_RepairKitComponent");
		if (comp)
		{
			float share = comp.GetRestoreScaled();
			ctx.True(share >= 0.3 && share <= 0.5, string.Format("one kit restores 30-50 %% of full health (got %1)", share));
			ctx.EqualFloat(share, LF_RepairKit.Restored(0, share), 0.001, "a wreck at 0 health ends at the kit's share");
			ctx.True(LF_RepairKit.Restored(0, share) < 1, "one kit never brings a wreck back to new");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(kit);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckOutlet(LF_TestContext ctx, string stockId, ResourceName probe, int price)
	{
		LF_ShopStock stock = LF_ShopStockConfig.Get().Find(stockId);
		ctx.NotNull(stock, "the " + stockId + " stock exists under the pack header");
		if (!stock)
			return;

		LF_ShopStockItem row = stock.FindItem(probe);
		ctx.NotNull(row, "the repair kit is a row of the " + stockId + " stock");
		if (row)
			ctx.Equal(price, row.m_iPrice, "the " + stockId + " stock prices the kit");
	}
}
