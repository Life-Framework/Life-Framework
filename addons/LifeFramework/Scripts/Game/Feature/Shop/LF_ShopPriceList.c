// LF_ShopPriceList.c — the counter's typed price list with an order slip, built on LF_UIScreen.
// Client-only, one at a time.
//
// Pick a line (Order), set the quantity on the slip (- / +, or MenuLeft/MenuRight along the
// row), then Pay with carried cash or Sell that many back. The buy and sell go through the
// existing LF_CharacterShopComponent RPCs; the reply (reason, money, carried cash) is printed on
// the slip. Typing in "Look for" narrows the list. It closes on Leave, MenuBack, death, losing
// the character, or walking CLOSE_DISTANCE from the counter.
class LF_ShopPriceList
{
	static const float SHOW_DISTANCE = 3.0;
	static const float CLOSE_DISTANCE = 3.5;
	//! Lines drawn at once; typing narrows a larger catalog to its matches.
	static const int MAX_ROWS = 40;
	static const float LIST_HEIGHT = 400;
	static const int SEARCH_POLL_MS = 250;
	static const int REPLY_TIMEOUT_MS = 5000;

	static const string ROW_PREFIX = "Item";
	static const string ROW_SUFFIX = "Row";
	static const string LESS = "QtyLessButton";
	static const string MORE = "QtyMoreButton";
	static const string PAY = "PayButton";
	static const string SELL = "SellButton";

	protected static ref LF_ShopPriceList s_Current;

	protected ref LF_UIScreen m_Screen;
	protected ref array<ref LF_ShopCatalogItem> m_aShown = {};
	protected LF_ShopComponent m_Shop;
	protected RplId m_ShopId;
	protected string m_sFilter;
	protected ResourceName m_Selected;
	protected string m_sSelectedName;
	protected int m_iSelectedPrice;
	protected int m_iQuantity = 1;
	protected bool m_bWaiting;
	protected string m_sKeySound;
	protected string m_sTillSound;
	protected string m_sRefuseSound;

	//------------------------------------------------------------------------------------------------
	static LF_ShopPriceList Current()
	{
		return s_Current;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen CurrentScreen()
	{
		if (!s_Current)
			return null;

		return s_Current.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the price list of shop for the local player.
	static LF_ShopPriceList Open(LF_ShopComponent shop, LF_ShopAction action)
	{
		Hide();
		if (!shop || !shop.GetOwner())
		{
			LF_Debug.Error("Shop", "price list refused: no shop");
			return null;
		}

		RplComponent rpl = RplComponent.Cast(shop.GetOwner().FindComponent(RplComponent));
		if (!rpl)
		{
			LF_Debug.Error("Shop", "price list refused: the shop has no RplComponent, so the till cannot be reached");
			return null;
		}

		LF_ShopPriceList list = new LF_ShopPriceList();
		list.m_Shop = shop;
		list.m_ShopId = rpl.Id();
		if (action)
		{
			list.m_sKeySound = action.GetKeySound();
			list.m_sTillSound = action.GetTillSound();
			list.m_sRefuseSound = action.GetRefuseSound();
		}

		if (!list.Build())
			return null;

		s_Current = list;
		if (action)
			LF_UISound.Play(action.GetOpenSound());

		LF_Debug.Info("Shop", "price list open: " + shop.GetShopName());
		return list;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Current)
			return;

		LF_ShopPriceList current = s_Current;
		s_Current = null;
		if (current.m_Screen)
			current.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! True when an item's shown name contains the filter, ignoring case. An empty filter matches.
	static bool Matches(string shownName, string filter)
	{
		if (filter.IsEmpty())
			return true;

		string name = shownName;
		string wanted = filter;
		name.ToLower();
		wanted.ToLower();
		return name.IndexOf(wanted) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Row name for the line at index; its Order button is <row>_Action.
	static string RowName(int index)
	{
		return ROW_PREFIX + index.ToString() + ROW_SUFFIX;
	}

	//------------------------------------------------------------------------------------------------
	//! Line index from a row name, or -1.
	static int RowIndex(string rowName)
	{
		int prefix = ROW_PREFIX.Length();
		int suffix = ROW_SUFFIX.Length();
		if (rowName.Length() <= prefix + suffix)
			return -1;

		if (rowName.Substring(0, prefix) != ROW_PREFIX || rowName.Substring(rowName.Length() - suffix, suffix) != ROW_SUFFIX)
			return -1;

		string digits = rowName.Substring(prefix, rowName.Length() - prefix - suffix);
		int index = digits.ToInt();
		if (index.ToString() != digits)
			return -1;

		return index;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Build()
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", WidgetManager.Translate(m_Shop.GetShopName())));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Cash", CashText(LF_MoneyUtils.GetCash(LF_Utils.GetLocalCharacter()))));
		if (!m_Shop.GetShopHint().IsEmpty())
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Hint", m_Shop.GetShopHint()));

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, "Search", "#LF-Shop_LookFor"));
		parts.Insert(LF_UIPart.CreateList("Prices"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Order", "#LF-Shop_SlipEmpty"));
		parts.Insert(LF_ATMTerminal.Keys("Quantity", LESS + "," + MORE, "#LF-Shop_Less,#LF-Shop_More"));
		parts.Insert(LF_ATMTerminal.Keys("Till", PAY + "," + SELL, "#LF-Shop_Pay,#LF-Shop_SellBack"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Receipt", ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "CloseButton", "#LF-Shop_Leave"));

		m_Screen = LF_UIScreen.Open(parts, LF_UISkin.PriceList());
		if (!m_Screen)
		{
			LF_Debug.Error("Shop", "price list failed to open");
			return false;
		}

		m_Screen.SetListHeight("Prices", LIST_HEIGHT);
		m_Screen.GetOnButton().Insert(OnButton);
		m_Screen.GetOnClosed().Insert(OnClosed);
		m_Screen.CloseOnDeath();
		m_Screen.CloseWhenUncontrolled();
		m_Screen.CloseBeyond(CLOSE_DISTANCE, m_Shop.GetOwner().GetID());

		FillRows();
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(PollSearch, SEARCH_POLL_MS, true);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the lines from the shop's catalog and the search filter.
	protected void FillRows()
	{
		if (!m_Screen)
			return;

		m_Screen.ClearList("Prices");
		m_aShown.Clear();

		int total = 0;
		foreach (LF_ShopCatalogItem item : m_Shop.GetShopItems())
		{
			if (!item || item.m_iBuyPrice <= 0)
				continue;

			string shownName = WidgetManager.Translate(item.m_sDisplayName);
			if (!Matches(shownName, m_sFilter))
				continue;

			total++;
			if (m_aShown.Count() >= MAX_ROWS)
				continue;

			m_aShown.Insert(item);
			m_Screen.AddListRow("Prices", RowName(m_aShown.Count() - 1), shownName, LF_MoneyFormat.FormatMoney(item.m_iBuyPrice), "#LF-Shop_Order");
		}

		if (total > m_aShown.Count())
			LF_Debug.Info("Shop", string.Format("price list shows %1 of %2 lines; typing narrows it", m_aShown.Count(), total));
	}

	//------------------------------------------------------------------------------------------------
	//! The edit box has no change event on the kit, so the list re-reads it a few times a second.
	protected void PollSearch()
	{
		if (!m_Screen || !m_Screen.IsOpen())
			return;

		string filter = m_Screen.GetFieldText("Search");
		filter.TrimInPlace();
		if (filter == m_sFilter)
			return;

		m_sFilter = filter;
		FillRows();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		int index = RowIndex(name);
		if (index >= 0)
		{
			Select(index);
			return;
		}

		if (name == LESS || name == MORE)
		{
			int step = 1;
			if (name == LESS)
				step = -1;

			m_iQuantity = LF_ShopRefusal.StepQuantity(m_iQuantity, step);
			LF_UISound.Play(m_sKeySound);
			ShowSlip();
			return;
		}

		if (name == PAY)
		{
			Send(true);
			return;
		}

		if (name == SELL)
		{
			Send(false);
			return;
		}

		LF_Debug.Warn("Shop", "price list ignored button " + name);
	}

	//------------------------------------------------------------------------------------------------
	protected void Select(int index)
	{
		if (index < 0 || index >= m_aShown.Count())
		{
			LF_Debug.Warn("Shop", "order refused: no line " + index.ToString());
			return;
		}

		LF_ShopCatalogItem item = m_aShown.Get(index);
		m_Selected = item.m_ItemPrefab;
		m_sSelectedName = WidgetManager.Translate(item.m_sDisplayName);
		m_iSelectedPrice = item.m_iBuyPrice;
		m_iQuantity = 1;
		LF_UISound.Play(m_sKeySound);
		SetReceipt("");
		ShowSlip();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowSlip()
	{
		if (!m_Screen)
			return;

		if (m_Selected.IsEmpty())
		{
			m_Screen.SetLine("Order", "#LF-Shop_SlipEmpty");
			m_Screen.SetButtonText(PAY, "#LF-Shop_Pay");
			return;
		}

		int total = LF_ShopRules.StackTradeTotal(m_iSelectedPrice, m_iQuantity);
		m_Screen.SetLine("Order", WidgetManager.Translate("#LF-Shop_SlipLine", m_iQuantity, m_sSelectedName));
		m_Screen.SetButtonText(PAY, WidgetManager.Translate("#LF-Shop_PayTotal", LF_MoneyFormat.FormatMoney(total)));
	}

	//------------------------------------------------------------------------------------------------
	protected void Send(bool buy)
	{
		if (m_bWaiting)
		{
			LF_Debug.Info("Shop", "till busy: request ignored");
			return;
		}

		if (m_Selected.IsEmpty())
		{
			SetReceipt("#LF-Shop_PickFirst");
			LF_UISound.Play(m_sRefuseSound);
			return;
		}

		LF_CharacterShopComponent bridge = LF_Component<LF_CharacterShopComponent>.Find(LF_Utils.GetLocalCharacter());
		if (!bridge)
		{
			LF_Debug.Error("Shop", "till unreachable: no LF_CharacterShopComponent on the controlled character");
			SetReceipt("#LF-Shop_RefuseNoShop");
			LF_UISound.Play(m_sRefuseSound);
			return;
		}

		m_bWaiting = true;
		SetReceipt("#LF-Shop_Working");
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(ReplyTimedOut, REPLY_TIMEOUT_MS, false);

		if (buy)
			bridge.RequestBuy(m_ShopId, m_Selected, m_iQuantity);
		else
			bridge.RequestSell(m_ShopId, m_Selected, m_iQuantity);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReplyTimedOut()
	{
		if (!m_bWaiting)
			return;

		m_bWaiting = false;
		LF_Debug.Warn("Shop", "no reply from the till within " + REPLY_TIMEOUT_MS.ToString() + " ms");
		SetReceipt("#LF-Shop_NoReply");
		LF_UISound.Play(m_sRefuseSound);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the till's answer to a buy or sell.
	static void OnResult(bool buy, LF_EShopRefusal reason, string prefab, int quantity, int money, int cash)
	{
		if (!s_Current)
		{
			LF_Debug.Info("Shop", "till reply arrived after the price list closed");
			return;
		}

		s_Current.ApplyResult(buy, reason, money, cash);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyResult(bool buy, LF_EShopRefusal reason, int money, int cash)
	{
		m_bWaiting = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ReplyTimedOut);

		if (m_Screen)
			m_Screen.SetLine("Cash", CashText(cash));

		if (reason != LF_EShopRefusal.NONE)
		{
			SetReceipt(LF_ShopRefusal.Key(reason));
			LF_UISound.Play(m_sRefuseSound);
			LF_Debug.Info("Shop", "slip shows refusal " + typename.EnumToString(LF_EShopRefusal, reason));
			return;
		}

		if (buy)
			SetReceipt(WidgetManager.Translate("#LF-Shop_Paid", LF_MoneyFormat.FormatMoney(money)));
		else
			SetReceipt(WidgetManager.Translate("#LF-Shop_SoldFor", LF_MoneyFormat.FormatMoney(money)));

		LF_UISound.Play(m_sTillSound);
	}

	//------------------------------------------------------------------------------------------------
	protected static string CashText(int cash)
	{
		if (cash < 0)
			return WidgetManager.Translate("#LF-Shop_CashLine", WidgetManager.Translate("#LF-ATM_Unknown"));

		return WidgetManager.Translate("#LF-Shop_CashLine", LF_MoneyFormat.FormatMoney(cash));
	}

	//------------------------------------------------------------------------------------------------
	protected void SetReceipt(string text)
	{
		if (m_Screen)
			m_Screen.SetLine("Receipt", text);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the listeners and timers if the instance goes away without a close.
	void ~LF_ShopPriceList()
	{
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(PollSearch);
			GetGame().GetCallqueue().Remove(ReplyTimedOut);
		}

		if (m_Screen)
		{
			m_Screen.GetOnButton().Remove(OnButton);
			m_Screen.GetOnClosed().Remove(OnClosed);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosed()
	{
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(PollSearch);
			GetGame().GetCallqueue().Remove(ReplyTimedOut);
		}

		if (s_Current == this)
			s_Current = null;

		LF_Debug.Info("Shop", "price list closed");
	}
}
