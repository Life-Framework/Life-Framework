class EL_ShopMenu : ChimeraMenuBase
{
	//! Cap on rows drawn when no search filter is set; typing narrows the list to matches.
	protected const int MAX_BROWSE_ROWS = 150;

	protected EL_ShopComponent m_ShopComponent;
	protected EL_CharacterShopComponent m_CharacterShop;
	protected RplId m_ShopRplId;
	protected IEntity m_PlayerEntity;

	protected Widget m_wRoot;
	protected TextWidget m_wShopTitle;
	protected TextWidget m_wCash;
	protected EditBoxWidget m_wSearchBox;
	protected TextWidget m_wResultText;
	protected Widget m_wItemList;

	protected string m_sSearchFilter;
	protected ref array<ref EL_ShopCatalogItem> m_aCatalogItems;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		m_wRoot = GetRootWidget();
		m_wShopTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("ShopTitle"));
		m_wCash = TextWidget.Cast(m_wRoot.FindAnyWidget("CashText"));
		m_wSearchBox = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("SearchBox"));
		m_wResultText = TextWidget.Cast(m_wRoot.FindAnyWidget("ResultText"));
		m_wItemList = m_wRoot.FindAnyWidget("ItemList");

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	void SetShopComponent(EL_ShopComponent shopComponent)
	{
		m_ShopComponent = shopComponent;
		if (!shopComponent)
			return;

		IEntity shopEntity = shopComponent.GetOwner();
		if (shopEntity)
		{
			RplComponent rpl = RplComponent.Cast(shopEntity.FindComponent(RplComponent));
			if (rpl)
				m_ShopRplId = rpl.Id();
		}

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	void SetPlayerEntity(IEntity playerEntity)
	{
		m_PlayerEntity = playerEntity;
		if (playerEntity)
			m_CharacterShop = EL_CharacterShopComponent.Cast(playerEntity.FindComponent(EL_CharacterShopComponent));

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the item list and the header from the shop and the player's inventory. The list is
	//! sorted by name and narrowed by the search filter; without a filter only a browseable page is
	//! drawn so a several-thousand-item catalog stays responsive.
	void Refresh()
	{
		if (!m_wItemList || !m_ShopComponent)
			return;

		if (m_wShopTitle)
			m_wShopTitle.SetText(m_ShopComponent.GetShopName());

		if (m_wCash && m_PlayerEntity)
		{
			int cash = EL_MoneyUtils.GetCash(m_PlayerEntity);
			if (cash >= 0)
				m_wCash.SetText(WidgetManager.Translate("#EL-Shop_Cash") + " " + EL_MoneyFormat.FormatMoney(cash));
		}

		ClearRows();

		m_aCatalogItems = m_ShopComponent.GetShopItems();

		int total = m_aCatalogItems.Count();
		int shown = 0;
		bool hasFilter = !m_sSearchFilter.IsEmpty();

		map<ResourceName, int> ownedCounts = BuildOwnedCounts();

		foreach (EL_ShopCatalogItem item : m_aCatalogItems)
		{
			if (!item)
				continue;

			if (hasFilter)
			{
				if (item.m_sDisplayName.IndexOf(m_sSearchFilter) == -1)
					continue;
			}
			else if (shown >= MAX_BROWSE_ROWS)
			{
				break;
			}

			Widget rowWidget = GetGame().GetWorkspace().CreateWidgets("{3851DEDF6D111FEE}UI/Layouts/ShopMenuRow.layout", m_wItemList);
			if (!rowWidget)
			{
				EL_Debug.Error("Shop", string.Format("row layout failed to create for %1", item.m_ItemPrefab));
				continue;
			}

			EL_ShopMenuRowComponent row = EL_ShopMenuRowComponent.Cast(rowWidget.FindHandler(EL_ShopMenuRowComponent));
			if (row)
			{
				int ownedCount = 0;
				ownedCounts.Find(item.m_ItemPrefab, ownedCount);
				row.SetItem(this, item, ownedCount);
			}

			shown++;
		}

		UpdateResultText(shown, total);
	}

	//------------------------------------------------------------------------------------------------
	//! Result-count hint, so a capped or filtered list does not read as the whole shop.
	protected void UpdateResultText(int shown, int total)
	{
		if (!m_wResultText)
			return;

		if (m_sSearchFilter.IsEmpty() && shown >= total)
			m_wResultText.SetText("");
		else
			m_wResultText.SetText(WidgetManager.Translate("#EL-Shop_Results", shown, total));
	}

	//------------------------------------------------------------------------------------------------
	//! Live search: each keystroke narrows the item list.
	override bool OnChange(Widget w, bool finished)
	{
		if (w == m_wSearchBox)
		{
			m_sSearchFilter = m_wSearchBox.GetText();
			Refresh();
			return true;
		}

		return super.OnChange(w, finished);
	}

	//------------------------------------------------------------------------------------------------
	//! Clears the runtime rows from the list container.
	protected void ClearRows()
	{
		if (!m_wItemList)
			return;

		while (m_wItemList.GetChildren())
			m_wItemList.GetChildren().RemoveFromHierarchy();
	}

	//------------------------------------------------------------------------------------------------
	//! How many of each prefab the player holds, computed in one inventory walk per refresh.
	protected map<ResourceName, int> BuildOwnedCounts()
	{
		map<ResourceName, int> counts = new map<ResourceName, int>();
		if (!m_PlayerEntity)
			return counts;

		SCR_InventoryStorageManagerComponent storageManager = SCR_InventoryStorageManagerComponent.Cast(m_PlayerEntity.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!storageManager)
			return counts;

		array<IEntity> rootItems();
		storageManager.GetAllRootItems(rootItems);

		foreach (IEntity item : rootItems)
		{
			if (!item)
				continue;

			ResourceName prefab = EL_Utils.GetPrefabName(item);
			if (prefab.IsEmpty())
				continue;

			int quantity = 1;
			EL_QuantityComponent quantityComponent = EL_Component<EL_QuantityComponent>.Find(item);
			if (quantityComponent)
				quantity = quantityComponent.GetQuantity();

			int current = 0;
			if (counts.Find(prefab, current))
				counts.Set(prefab, current + quantity);
			else
				counts.Set(prefab, quantity);
		}

		return counts;
	}

	//------------------------------------------------------------------------------------------------
	//! Buy one unit of an item through the owning player's transaction bridge.
	void BuyItem(EL_ShopCatalogItem item)
	{
		if (m_CharacterShop && m_ShopRplId.IsValid() && item)
			m_CharacterShop.RequestBuy(m_ShopRplId, item.m_ItemPrefab, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Sell one unit of an item through the owning player's transaction bridge.
	void SellItem(EL_ShopCatalogItem item)
	{
		if (m_CharacterShop && m_ShopRplId.IsValid() && item)
			m_CharacterShop.RequestSell(m_ShopRplId, item.m_ItemPrefab, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Server reply to a buy or sell: the server already notified the outcome, so just re-read.
	void OnTransactionResult(bool success, string prefab, int quantity)
	{
		Refresh();
	}
}