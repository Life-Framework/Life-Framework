//------------------------------------------------------------------------------------------------
//! Shop stock (#fence-shop): what one kind of shop sells and what it buys back, as data. The core
//! ships the mechanism and an empty list (Configs/Shop/LF_ShopStock.conf); a pack adds its stocks in
//! an overlay of this class listed in its mission header (`m_aLF_ConfigOverlays`). A shop names its
//! stock with LF_ShopComponent.m_sStockId. Prices here are the shop's own; the server re-reads them
//! on every buy and sell, so a client's copy only draws the list.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_ShopStockItem
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Item the shop sells", "et")]
	ResourceName m_Prefab;

	[Attribute("", UIWidgets.Auto, desc: "Name on the list, a localization key; empty uses the item's own name")]
	string m_sDisplayName;

	[Attribute("100", UIWidgets.Auto, desc: "What the player pays for one", params: "0 1000000")]
	int m_iPrice;

	[Attribute("0", UIWidgets.Auto, desc: "Fence shops: completed crimes on the buyer's record before this item is sold. 0 is no threshold")]
	int m_iMinCrimes;
}

//------------------------------------------------------------------------------------------------
//! What a shop takes off a player's hands, and at what share of the goods' street value (the
//! price-table buy price, LF_ShopPricesConfig).
[BaseContainerProps()]
class LF_ShopBuyback
{
	[Attribute("1", UIWidgets.CheckBox, desc: "Buys any goods flagged stolen (LF_StolenGoods), listed or not")]
	bool m_bStolen;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Goods it buys whether flagged or not (chop-shop parts, loot)", "et")]
	ref array<ResourceName> m_aPrefabs;

	[Attribute("0.4", UIWidgets.Slider, desc: "Share of the street value it pays, 0 to 1", params: "0 1 0.05")]
	float m_fFraction;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_ShopStock
{
	[Attribute("", UIWidgets.EditBox, desc: "Stock id a shop names in LF_ShopComponent.m_sStockId (fence, ...)")]
	string m_sId;

	[Attribute("", UIWidgets.Object, desc: "What the shop sells, in list order")]
	ref array<ref LF_ShopStockItem> m_aItems;

	[Attribute("", UIWidgets.Object, desc: "What the shop buys back; empty buys nothing")]
	ref LF_ShopBuyback m_Buyback;

	//------------------------------------------------------------------------------------------------
	LF_ShopStockItem FindItem(ResourceName prefab)
	{
		if (!m_aItems || prefab.IsEmpty())
			return null;

		foreach (LF_ShopStockItem item : m_aItems)
		{
			if (item && LF_Utils.SamePrefab(item.m_Prefab, prefab))
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the buyback list names this prefab (flag or no flag).
	bool ListsBuyback(ResourceName prefab)
	{
		if (!m_Buyback || !m_Buyback.m_aPrefabs || prefab.IsEmpty())
			return false;

		foreach (ResourceName listed : m_Buyback.m_aPrefabs)
		{
			if (LF_Utils.SamePrefab(listed, prefab))
				return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_ShopStockConfig
{
	static const ResourceName CORE = "{41D148C6BCDC5917}Configs/Shop/LF_ShopStock.conf";
	//! Names the overlays in log lines; no owner config.json section reads stocks yet.
	static const string OVERLAY_TAG = "shopStock";

	[Attribute("", UIWidgets.Object, desc: "Every stock; a pack overlay's stock replaces a core stock of the same id")]
	ref array<ref LF_ShopStock> m_aStocks;

	protected static ref LF_ShopStockConfig s_Instance;

	//------------------------------------------------------------------------------------------------
	//! The merged stocks: the core file, then each pack overlay of this class. Read once per world.
	static LF_ShopStockConfig Get()
	{
		if (s_Instance)
			return s_Instance;

		s_Instance = Load(CORE);
		if (!s_Instance)
		{
			LF_Debug.Error(LF_ShopStockRules.FEATURE, "shop stock config did not load; every stocked shop is empty: " + CORE);
			s_Instance = new LF_ShopStockConfig();
		}

		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CORE, LF_ShopStockConfig, OVERLAY_TAG, resources, names);
		foreach (int i, Resource resource : resources)
		{
			LF_ShopStockConfig overlay = LF_ShopStockConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
			int merged = s_Instance.Merge(overlay);
			LF_Debug.Info(LF_ShopStockRules.FEATURE, string.Format("pack overlay %1 set %2 stock(s)", names[i], merged));
		}

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests install a fixed config; null drops it so the next Get reads the files again.
	static void Set(LF_ShopStockConfig config)
	{
		s_Instance = config;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ShopStockConfig Load(ResourceName conf)
	{
		Resource resource = BaseContainerTools.LoadContainer(conf);
		if (!resource || !resource.IsValid() || !resource.GetResource())
			return null;

		return LF_ShopStockConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
	}

	//------------------------------------------------------------------------------------------------
	//! Puts each stock of \p other over this one by id (replace, or append a new id).
	//! \return How many stocks it set.
	int Merge(LF_ShopStockConfig other)
	{
		if (!other || !other.m_aStocks)
			return 0;

		if (!m_aStocks)
			m_aStocks = {};

		int merged;
		foreach (LF_ShopStock stock : other.m_aStocks)
		{
			if (!stock || stock.m_sId.IsEmpty())
				continue;

			int at = IndexOf(stock.m_sId);
			if (at >= 0)
				m_aStocks.Set(at, stock);
			else
				m_aStocks.Insert(stock);

			merged++;
		}

		return merged;
	}

	//------------------------------------------------------------------------------------------------
	protected int IndexOf(string id)
	{
		if (!m_aStocks)
			return -1;

		foreach (int i, LF_ShopStock stock : m_aStocks)
		{
			if (stock && stock.m_sId == id)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	LF_ShopStock Find(string id)
	{
		int at = IndexOf(id);
		if (at < 0)
			return null;

		return m_aStocks[at];
	}
}

//------------------------------------------------------------------------------------------------
//! The pure decisions of a stocked shop, so a LOGIC test can hold them.
class LF_ShopStockRules
{
	static const string FEATURE = "ShopStock";

	//------------------------------------------------------------------------------------------------
	//! What the shop pays for one unit worth \p streetValue at \p fraction. The fraction is held to
	//! 0..1, so a buyback never pays more than the goods cost new (no buy-low, sell-high loop).
	static int BuybackPrice(int streetValue, float fraction)
	{
		if (streetValue <= 0 || fraction <= 0)
			return 0;

		if (fraction > 1)
			fraction = 1;

		// The epsilon keeps 200 * 0.4 at 80 when the float lands a hair under it.
		int paid = Math.Floor(streetValue * fraction + 0.0001);
		return paid;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a stock buys one unit: the list names it, or it carries the stolen flag and the
	//! stock takes flagged goods.
	static bool Buys(LF_ShopStock stock, ResourceName prefab, bool flaggedStolen)
	{
		if (!stock || !stock.m_Buyback || prefab.IsEmpty())
			return false;

		if (stock.ListsBuyback(prefab))
			return true;

		return stock.m_Buyback.m_bStolen && flaggedStolen;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of this file's statics (Core/LF_StaticReset.c runs every feature's hook).
modded class LF_StaticResetHooks4
{
	override void Run(notnull array<string> done, bool apply)
	{
		super.Run(done, apply);
		done.Insert("LF_ShopStockConfig");
		if (apply)
			LF_ShopStockConfig.ResetStatics();
	}
}
