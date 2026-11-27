// red-proof: observed 2026-11-27 (#5080): with the hardware row ETool_ALICE and the pharmacy row AspirinBottle unnamed, `tools\cli test --no-build --only LF_Test_EveronShopRowNames` failed `pharmacy: AspirinBottle.et lists as a raw prefab name (no row name and no item name)` (and the hardware row before it was named); `--red` ignores every row's own name and printed `red: hardware: ETool_ALICE.et lists as a raw prefab name` and the pharmacy line (RED PROOF OK). With both rows named it passes.
//! #5080: no shop list row shows a raw prefab or class name. Every row of every stock (the merged core and
//! pack config) resolves to a name the way the list resolves it (LF_ShopComponent: the row's own key, else
//! the item's UIInfo name, else the prefab file name), and that name must translate to readable text, not
//! fall back to the file name (`ETool_ALICE`) or an untranslated `#key`.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronShopRowNames : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/shop-row-names";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ShopStockConfig config = LF_ShopStockConfig.Get();
		ctx.NotNull(config, "the shop stock config loads");
		if (!config || !config.m_aStocks)
			return;

		ctx.True(config.m_aStocks.Count() > 0, "the shop stock config lists at least one stock");

		int rows;
		foreach (LF_ShopStock stock : config.m_aStocks)
		{
			if (!stock || !stock.m_aItems)
				continue;

			foreach (LF_ShopStockItem row : stock.m_aItems)
			{
				if (!row)
					continue;

				rows++;
				CheckRow(ctx, stock.m_sId, row);
			}
		}

		ctx.True(rows > 0, "at least one shop row was checked");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRow(LF_TestContext ctx, string stockId, LF_ShopStockItem row)
	{
		string file = FileName(row.m_Prefab);

		string name = row.m_sDisplayName;
		if (RedProof())
			name = "";

		//! The list falls back to the file name when the item has no UI name (LF_ShopManagerComponent.ResolveDisplayName);
		//! that fallback is the bug, so an empty name here means the row would show the raw prefab name.
		string source = "row key";
		if (name.IsEmpty())
		{
			source = "item UIInfo";
			UIInfo info = LF_UIInfoUtils.GetInfo(row.m_Prefab);
			if (info)
				name = info.GetName();
		}

		ctx.True(!name.IsEmpty(), string.Format("%1: %2.et lists as a raw prefab name (no row name and no item name)", stockId, file));
		if (name.IsEmpty())
			return;

		string shown = name;
		if (name.StartsWith("#"))
			shown = WidgetManager.Translate(name);

		ctx.True(!shown.IsEmpty() && !shown.StartsWith("#"), string.Format("%1: %2 name %3 (%4) has no translation", stockId, file, name, source));
		ctx.True(!shown.Contains("_"), string.Format("%1: %2 name '%3' (%4) reads like a class or prefab name", stockId, file, shown, source));
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab's file name without its folder and the .et extension.
	protected string FileName(ResourceName prefab)
	{
		string path = prefab.GetPath();
		int lastSlash = path.LastIndexOf("/");
		if (lastSlash >= 0 && lastSlash < path.Length() - 1)
			path = path.Substring(lastSlash + 1, path.Length() - lastSlash - 1);
		if (path.EndsWith(".et"))
			path = path.Substring(0, path.Length() - 3);
		return path;
	}
}
