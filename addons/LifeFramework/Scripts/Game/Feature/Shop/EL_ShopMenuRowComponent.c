//------------------------------------------------------------------------------------------------
//! Per-row handler for the shop menu. Each row is an instance of ShopMenuRow.layout; this
//! component fills the row's widgets and routes the Buy/Sell button clicks back to the menu.
//------------------------------------------------------------------------------------------------
class EL_ShopMenuRowComponent : ScriptedWidgetComponent
{
	protected Widget m_wRoot;
	protected TextWidget m_wItemName;
	protected TextWidget m_wOwned;
	protected TextWidget m_wBuyPrice;
	protected TextWidget m_wSellPrice;
	protected ButtonWidget m_wBuyButton;
	protected ButtonWidget m_wSellButton;

	protected EL_ShopMenu m_Menu;
	protected EL_ShopCatalogItem m_Item;

	//------------------------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wRoot = w;
		m_wItemName = TextWidget.Cast(m_wRoot.FindAnyWidget("ItemName"));
		m_wOwned = TextWidget.Cast(m_wRoot.FindAnyWidget("OwnedText"));
		m_wBuyPrice = TextWidget.Cast(m_wRoot.FindAnyWidget("BuyPriceText"));
		m_wSellPrice = TextWidget.Cast(m_wRoot.FindAnyWidget("SellPriceText"));
		m_wBuyButton = ButtonWidget.Cast(m_wRoot.FindAnyWidget("BuyButton"));
		m_wSellButton = ButtonWidget.Cast(m_wRoot.FindAnyWidget("SellButton"));
	}

	//------------------------------------------------------------------------------------------------
	//! Fills the row for one catalog item.
	//! \param[in] menu The owning menu, for buy/sell callbacks.
	//! \param[in] item The catalog item this row shows.
	//! \param[in] ownedCount How many the player holds.
	void SetItem(EL_ShopMenu menu, EL_ShopCatalogItem item, int ownedCount)
	{
		m_Menu = menu;
		m_Item = item;

		if (!item)
			return;

		if (m_wItemName)
			m_wItemName.SetText(item.m_sDisplayName);

		if (m_wOwned)
			m_wOwned.SetText(ownedCount.ToString());

		if (m_wBuyPrice)
			m_wBuyPrice.SetText(EL_MoneyFormat.FormatMoney(item.m_iBuyPrice));

		if (m_wSellPrice)
			m_wSellPrice.SetText(EL_MoneyFormat.FormatMoney(item.m_iSellPrice));
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_wBuyButton && w == m_wBuyButton)
		{
			if (m_Menu && m_Item)
				m_Menu.BuyItem(m_Item);
			return true;
		}

		if (m_wSellButton && w == m_wSellButton)
		{
			if (m_Menu && m_Item)
				m_Menu.SellItem(m_Item);
			return true;
		}

		return super.OnClick(w, x, y, button);
	}
};