//------------------------------------------------------------------------------------------------
//! One priced entry in the shop's full catalog: a prefab with its resolved buy and sell prices.
//------------------------------------------------------------------------------------------------
class EL_ShopCatalogItem
{
	ResourceName m_ItemPrefab;
	string m_sDisplayName;
	SCR_EArsenalItemType m_eItemType;
	SCR_EArsenalItemMode m_eItemMode;
	int m_iBuyPrice;
	int m_iSellPrice;
	bool m_bHidden;
};

class EL_ShopManagerComponentClass : GameComponentClass
{
};

//------------------------------------------------------------------------------------------------
//! Builds the priced catalog every shop shows: every in-game inventory item prefab from the
//! resource database, priced from the EL_ShopPricesConfig. Lives on the game mode; the same game
//! data (config + resources) exists on server and client, so both build the identical catalog and
//! the menu's displayed prices can never drift from what the server charges.
//------------------------------------------------------------------------------------------------
class EL_ShopManagerComponent : GameComponent
{
	//! Price config resource, referenced from the game mode prefab.
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Shop prices config", "conf class=EL_ShopPricesConfig")]
	protected ResourceName m_sConfigPath;

	protected ref EL_ShopPricesConfig m_Config;
	protected ref array<ref EL_ShopCatalogItem> m_aCatalog;
	protected bool m_bBuilt;

	protected static EL_ShopManagerComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	static EL_ShopManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode gameMode = GetGame().GetGameMode();
			if (gameMode)
				s_Instance = EL_ShopManagerComponent.Cast(gameMode.FindComponent(EL_ShopManagerComponent));
		}

		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! The priced catalog, built on first use. Build is lazy so it survives catalog initialization
	//! ordering on both server and client.
	//! \return All non-hidden priced items, or an empty array when the feature is not configured.
	array<ref EL_ShopCatalogItem> GetCatalogItems()
	{
		EnsureBuilt();
		if (!m_aCatalog)
			return new array<ref EL_ShopCatalogItem>();
		return m_aCatalog;
	}

	//------------------------------------------------------------------------------------------------
	//! Finds one item in the priced catalog.
	//! \param[in] prefab Prefab path of the item.
	//! \return The catalog item, or null when the prefab is not catalogued.
	EL_ShopCatalogItem GetItem(ResourceName prefab)
	{
		EnsureBuilt();
		if (!m_aCatalog || prefab.IsEmpty())
			return null;

		foreach (EL_ShopCatalogItem item : m_aCatalog)
		{
			if (item.m_ItemPrefab == prefab)
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolved prices for any prefab. Catalogued items use their rule; uncatalogued ones fall back
	//! to the config defaults so a looted or inherited item still prices.
	//! \param[in] prefab Prefab path of the item.
	//! \param[out] buyPrice What the player pays.
	//! \param[out] sellPrice What the player is paid.
	//! \param[out] hidden True when the item is not sold anywhere.
	//! \return False when no config is loaded at all.
	bool GetPrices(ResourceName prefab, out int buyPrice, out int sellPrice, out bool hidden)
	{
		EnsureBuilt();
		if (!m_Config)
			return false;

		EL_ShopCatalogItem item = GetItem(prefab);
		if (item)
		{
			buyPrice = item.m_iBuyPrice;
			sellPrice = item.m_iSellPrice;
			hidden = item.m_bHidden;
			return true;
		}

		buyPrice = m_Config.m_iDefaultBuyPrice;
		sellPrice = m_Config.m_iDefaultSellPrice;
		hidden = false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Buy price for one prefab. -1 when the feature is not configured.
	int GetBuyPrice(ResourceName prefab)
	{
		int buyPrice;
		int sellPrice;
		bool hidden;
		if (!GetPrices(prefab, buyPrice, sellPrice, hidden))
			return -1;
		return buyPrice;
	}

	//------------------------------------------------------------------------------------------------
	//! Sell price for one prefab. -1 when the feature is not configured.
	int GetSellPrice(ResourceName prefab)
	{
		int buyPrice;
		int sellPrice;
		bool hidden;
		if (!GetPrices(prefab, buyPrice, sellPrice, hidden))
			return -1;
		return sellPrice;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether one prefab is priced but hidden from every shop.
	bool IsHidden(ResourceName prefab)
	{
		int buyPrice;
		int sellPrice;
		bool hidden;
		if (!GetPrices(prefab, buyPrice, sellPrice, hidden))
			return false;
		return hidden;
	}

	//------------------------------------------------------------------------------------------------
	protected void EnsureBuilt()
	{
		if (m_bBuilt)
			return;
		m_bBuilt = true;

		if (!LoadConfig())
			return;

		BuildCatalog();
	}

	//------------------------------------------------------------------------------------------------
	protected bool LoadConfig()
	{
		if (m_sConfigPath.IsEmpty())
		{
			EL_Debug.Error("Shop", "no prices config path set on EL_ShopManagerComponent");
			return false;
		}

		Resource container = BaseContainerTools.LoadContainer(m_sConfigPath);
		if (!container || !container.GetResource())
		{
			EL_Debug.Error("Shop", string.Format("prices config does not resolve: %1", m_sConfigPath));
			return false;
		}

		m_Config = EL_ShopPricesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!m_Config)
		{
			EL_Debug.Error("Shop", string.Format("prices config has an unexpected root class: %1", m_sConfigPath));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Walks every inventory item prefab in the resource database and prices the ones that are real
	//! inventory items (they can actually be bought into, and sold out of, a player's inventory).
	//! The mod's game mode carries no vanilla entity catalog, so the resource database is the source
	//! of truth: base-game and mod items alike are enumerated across every loaded addon, and the
	//! config's extra list tops up anything living outside Prefabs/Items (tools, mined resources).
	protected void BuildCatalog()
	{
		m_aCatalog = new array<ref EL_ShopCatalogItem>();

		SearchResourcesFilter filter();
		filter.fileExtensions = { "et" };
		filter.recursive = true;

		s_aSearchResults = new array<ResourceName>();
		ResourceDatabase.SearchResources(filter, OnResourceFound);

		if (s_aSearchResults.IsEmpty())
			EL_Debug.Error("Shop", "resource search returned no prefabs, shop catalog is empty");

		foreach (ResourceName prefab : s_aSearchResults)
		{
			if (prefab.IsEmpty())
				continue;
			if (!LooksLikeItemPath(prefab))
				continue;
			if (!IsInventoryItem(prefab))
				continue;

			InsertCatalogItem(prefab, 0, SCR_EArsenalItemMode.DEFAULT);
		}

		BuildExtraItems();
		SortByName();

		EL_Debug.Log("Shop", string.Format("catalog built: %1 priced items", m_aCatalog.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Cheap path pre-filter before the expensive prefab load. Carryable inventory item prefabs live
	//! under Prefabs/Items, Prefabs/Weapons and Prefabs/Characters (clothing). Everything else with
	//! an InventoryItemComponent is a false positive for the shop: vehicles carry one so they can be
	//! loaded as cargo, and Props carry one as supply containers - none of them can be bought into a
	//! player's backpack. The config's extra list tops up the mod's own items outside those roots.
	protected bool LooksLikeItemPath(ResourceName prefab)
	{
		string path = prefab.GetPath();
		return path.IndexOf("Prefabs/Items/") != -1
			|| path.IndexOf("Prefabs/Weapons/") != -1
			|| path.IndexOf("Prefabs/Characters/") != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Sorts the catalog by display name so the shop menu browses in a stable, searchable order.
	protected void SortByName()
	{
		if (!m_aCatalog)
			return;

		for (int i = 1; i < m_aCatalog.Count(); i++)
		{
			EL_ShopCatalogItem key = m_aCatalog[i];
			int j = i - 1;
			while (j >= 0 && m_aCatalog[j] && key && m_aCatalog[j].m_sDisplayName > key.m_sDisplayName)
			{
				m_aCatalog[j + 1] = m_aCatalog[j];
				j--;
			}
			m_aCatalog[j + 1] = key;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Search callback buffer. Static because the engine calls the callback by plain function, not
	//! on an instance.
	protected static ref array<ResourceName> s_aSearchResults;

	//------------------------------------------------------------------------------------------------
	static void OnResourceFound(ResourceName resourceName)
	{
		if (s_aSearchResults && !resourceName.IsEmpty())
			s_aSearchResults.Insert(resourceName);
	}

	//------------------------------------------------------------------------------------------------
	//! Prices and inserts one prefab into the catalog unless already present.
	protected void InsertCatalogItem(ResourceName prefab, SCR_EArsenalItemType itemType, SCR_EArsenalItemMode itemMode)
	{
		if (GetItem(prefab))
			return;

		int buyPrice;
		int sellPrice;
		bool hidden;
		EL_ShopPriceResolver.ResolvePrices(m_Config.m_aPriceRules, prefab, itemType, itemMode, m_Config.m_iDefaultBuyPrice, m_Config.m_iDefaultSellPrice, buyPrice, sellPrice, hidden);

		EL_ShopCatalogItem catalogItem = new EL_ShopCatalogItem();
		catalogItem.m_ItemPrefab = prefab;
		catalogItem.m_eItemType = itemType;
		catalogItem.m_eItemMode = itemMode;
		catalogItem.m_iBuyPrice = buyPrice;
		catalogItem.m_iSellPrice = sellPrice;
		catalogItem.m_bHidden = hidden;
		catalogItem.m_sDisplayName = ResolveDisplayName(prefab);
		m_aCatalog.Insert(catalogItem);
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the config's extra prefabs (the mod's own tools and mined resources, which live
	//! outside Prefabs/Items) into the catalog. They carry no arsenal data, so the name-substring
	//! rules price them.
	protected void BuildExtraItems()
	{
		if (!m_Config || !m_Config.m_aExtraItemPrefabs)
			return;

		foreach (ResourceName prefab : m_Config.m_aExtraItemPrefabs)
		{
			if (prefab.IsEmpty())
				continue;
			if (!IsInventoryItem(prefab))
				continue;

			InsertCatalogItem(prefab, 0, SCR_EArsenalItemMode.DEFAULT);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! True when the prefab carries an inventory item component, i.e. it can live in an inventory.
	protected bool IsInventoryItem(ResourceName prefab)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return false;
		return SCR_BaseContainerTools.FindComponentSource(resource, InventoryItemComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Best available name for the menu: the item's own UI display name, else the prefab path tail.
	protected string ResolveDisplayName(ResourceName prefab)
	{
		UIInfo info = EL_UIInfoUtils.GetInfo(prefab);
		if (info)
		{
			string name = info.GetName();
			if (!name.IsEmpty())
				return name;
		}

		string path = prefab.GetPath();
		int lastSlash = path.LastIndexOf("/");
		if (lastSlash >= 0 && lastSlash < path.Length() - 1)
			return path.Substring(lastSlash + 1, path.Length() - lastSlash - 1);
		return path;
	}
};