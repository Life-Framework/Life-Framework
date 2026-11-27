// red-proof: the pack leg's first run (2026-11-27, #5080) failed every row whose item has no inventory name of its own
// (`ETool_ALICE` among them) with `shows a raw prefab name`; `tools\cli test --no-build --red LF_Test_EveronShopNames` puts the raw file
// name on every row and fails them all.
//! 2026-11-27 UI sweep: every name a shop list can print resolves to words. A row takes its name from the stock line's key, else from
//! the item's own inventory name, else (the bug) from the prefab's file name, which is how the hardware store showed `ETool_ALICE`
//! (#5080). Checked over every merged pack stock row, every buyback line, every catalogue item an outlet sells, every licence row
//! and every extra priced item: the name is not empty, is not the prefab file name, and its translation holds no `#` key, `%` slot
//! or `Foo_Bar` id.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronShopNames : LF_Test
{
	protected static const string LOWER = "abcdefghijklmnopqrstuvwxyz";
	protected static const string UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	protected int m_iRows;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/shop-names";
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab's file name without folder or extension: what a row shows when nothing names the item.
	static string FileName(ResourceName prefab)
	{
		string path = prefab.GetPath();
		int slash = path.LastIndexOf("/");
		if (slash >= 0 && slash < path.Length() - 1)
			path = path.Substring(slash + 1, path.Length() - slash - 1);
		if (path.EndsWith(".et"))
			path = path.Substring(0, path.Length() - 3);
		return path;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: why a shown name is not words a player can read, or an empty string when it is.
	static string Problem(string key, string shown)
	{
		if (key.IsEmpty() || shown.IsEmpty())
			return "has no name";
		if (shown.StartsWith("#"))
			return "shows an untranslated key";
		if (shown.Contains("%"))
			return "shows a stray placeholder";
		// A one-word file name that is also the item's real name ("Apple", "Axe") is fine; ETool_ALICE or AspirinBottle is an id.
		if (LooksLikeId(shown))
			return "shows a raw id";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: one word with an underscore or a lower-to-upper case step inside it ("ETool_ALICE", "AspirinBottle", "LF_Reaper_01").
	static bool LooksLikeId(string text)
	{
		if (text.Contains(" "))
			return false;
		if (text.Contains("_"))
			return true;
		for (int i = 1; i < text.Length(); i++)
		{
			if (LOWER.Contains(text.Substring(i - 1, 1)) && UPPER.Contains(text.Substring(i, 1)))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRow(LF_TestContext ctx, string where, ResourceName prefab, string stockName)
	{
		string key = stockName;
		if (RedProof())
			key = "Raw_" + FileName(prefab);
		else if (key.IsEmpty())
			key = LF_ShopManagerComponent.ResolveDisplayName(prefab);

		string shown = WidgetManager.Translate(key);
		string problem = Problem(key, shown);
		m_iRows++;
		ctx.True(problem.IsEmpty(), string.Format("%1: %2 %3 (key '%4', shows '%5')", where, FileName(prefab), problem, key, shown));
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		m_iRows = 0;
		LF_ShopStockConfig stocks = LF_ShopStockConfig.Get();
		ctx.NotNull(stocks, "the merged shop stocks load");
		if (stocks && stocks.m_aStocks)
		{
			foreach (LF_ShopStock stock : stocks.m_aStocks)
			{
				if (!stock)
					continue;

				if (stock.m_aItems)
				{
					foreach (LF_ShopStockItem line : stock.m_aItems)
					{
						if (line && !line.m_Prefab.IsEmpty())
							CheckRow(ctx, "stock " + stock.m_sId, line.m_Prefab, line.m_sDisplayName);
					}
				}

				if (stock.m_Buyback && stock.m_Buyback.m_aPrefabs)
				{
					foreach (ResourceName wanted : stock.m_Buyback.m_aPrefabs)
						CheckRow(ctx, "stock " + stock.m_sId + " buyback", wanted, "");
				}
			}
		}

		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(catalogue, "the merged item catalogue loads");
		if (catalogue && catalogue.m_aOffers)
		{
			foreach (LF_OutletOffer offer : catalogue.m_aOffers)
			{
				if (!offer || offer.m_bRemoved)
					continue;

				if (!offer.m_sGrantsLicence.IsEmpty())
				{
					LF_ELicenseType licence;
					if (!LF_ItemCatalogue.ParseLicence(offer.m_sGrantsLicence, licence))
					{
						ctx.Fail(string.Format("licence row %1 names no licence: %2", offer.Key(), offer.m_sGrantsLicence));
						continue;
					}

					string licenceKey = LF_LicenceDesk.NameKey(licence);
					string licenceShown = WidgetManager.Translate(licenceKey);
					string licenceProblem = Problem(licenceKey, licenceShown);
					m_iRows++;
					ctx.True(licenceProblem.IsEmpty(), string.Format("licence row %1: %2 (key '%3', shows '%4')", offer.Key(), licenceProblem, licenceKey, licenceShown));
					continue;
				}

				LF_CatalogueItem entry = LF_ItemCatalogue.Item(catalogue, offer.m_sItem);
				if (!entry || entry.m_bRemoved)
				{
					ctx.Fail(string.Format("offer %1 names no catalogue item", offer.Key()));
					continue;
				}

				CheckRow(ctx, "outlet " + offer.m_sOutlet, entry.m_Prefab, "");
			}
		}

		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		if (manager && manager.GetConfig() && manager.GetConfig().m_aExtraItemPrefabs)
		{
			foreach (ResourceName extra : manager.GetConfig().m_aExtraItemPrefabs)
				CheckRow(ctx, "priced extra", extra, "");
		}

		ctx.True(m_iRows > 50, string.Format("the pack's shops list more than 50 rows (checked %1)", m_iRows));
	}
}
