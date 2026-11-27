// LF_ShopPriceList.c — the counter's typed price list with an order slip, built on LF_UIScreen.
// Client-only, one at a time.
//
// Pick a line (Order), set the quantity on the slip (- / +, or MenuLeft/MenuRight along the
// row), then Pay with carried cash or Sell that many back (the Sell key is there only when the
// counter pays for at least one of its lines, LF_ShopPriceList.BuysBack). The buy and sell go through the
// existing LF_CharacterShopComponent RPCs; the reply (reason, money, carried cash) is printed on
// the slip. Typing in "Look for" narrows the list. It closes on Leave, MenuBack, death, losing
// the character, or walking CLOSE_DISTANCE from the counter.
class LF_ShopPriceList
{
	// lf-fixed: UI close distance of an interaction sheet, an interaction-feel constant, not a balance value
	static const float SHOW_DISTANCE = 3.0;
	// lf-fixed: UI close distance of an interaction sheet, an interaction-feel constant, not a balance value
	static const float CLOSE_DISTANCE = 3.5;
	//! Lines drawn at once; typing narrows a larger catalog to its matches.
	static const int MAX_ROWS = 40;
	//! UI sweep 2026-11-27: the list showed two or three 76-unit rows at once between the search field and the slip, so a shop of
	//! 20 to 80 lines was all scrolling. Compact rows (LF_UICompactRow, 39 units each) and eight of them give 6 to 8 whole rows
	//! at 1080p and at 720p alike (the sheet is laid out in 1080-unit height either way) with the slip, keys and Leave kept.
	static const float LIST_HEIGHT = 312;	// lf-fixed: eight compact rows of LF_UIScreen.COMPACT_ROW_PITCH (39), leaving the slip, preview, keys and Leave room under the sheet's screen cap
	static const int SEARCH_POLL_MS = 250;
	// lf-fixed: client wait for a server reply, a protocol mechanic, not a balance value
	static const int REPLY_TIMEOUT_MS = 5000;
	//! After a till reply the Cash line keeps the server's figure this long before the poll reads the
	//! carried cash again, so a replicated change that lands a frame later does not flicker the old figure.
	static const int REPLY_HOLD_MS = 1000;	// lf-fixed: a UI redraw delay, not a gameplay tunable

	static const string ROW_PREFIX = "Item";
	static const string ROW_SUFFIX = "Row";
	//! A licence row (catalogue m_sGrantsLicence): Licence<i>Row, above the item lines.
	static const string LICENCE_PREFIX = "Licence";
	static const string LESS = "QtyLessButton";
	static const string MORE = "QtyMoreButton";
	static const string PAY = "PayButton";
	static const string SELL = "SellButton";

	protected static ref LF_ShopPriceList s_Current;

	protected ref LF_UIScreen m_Screen;
	protected ref array<ref LF_ShopCatalogItem> m_aShown = {};
	protected ref LF_ShopItemPreview m_Preview = new LF_ShopItemPreview();
	protected LF_ShopComponent m_Shop;
	protected RplId m_ShopId;
	protected string m_sFilter;
	protected ResourceName m_Selected;
	protected string m_sSelectedName;
	protected int m_iSelectedPrice;
	//! What the counter pays for one of the selected line (a buyback line has no buy price).
	protected int m_iSelectedSell;
	protected int m_iQuantity = 1;
	//! The licence rows shown (LF_ELicenseType names) and their fees, in row order.
	protected ref array<string> m_aLicences = {};
	protected ref array<int> m_aLicenceFees = {};
	//! The selected licence row's licence; empty when an item line (or nothing) is selected.
	protected string m_sSelectedLicence;
	protected bool m_bWaiting;
	//! The till has a Sell key: some line of this counter has a sell price (a buy-only till shows Pay alone).
	protected bool m_bSells;
	//! The cash the Cash line shows; -1 before the first read.
	protected int m_iShownCash = -1;
	//! System tick of the last till reply; the poll leaves the Cash line alone for REPLY_HOLD_MS after it.
	protected int m_iReplyTick;
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
		return PrefixedIndex(ROW_PREFIX, rowName);
	}

	//------------------------------------------------------------------------------------------------
	//! Row name for the licence row at index; its Order button is <row>_Action.
	static string LicenceRowName(int index)
	{
		return LICENCE_PREFIX + index.ToString() + ROW_SUFFIX;
	}

	//------------------------------------------------------------------------------------------------
	//! Licence row index from a row name, or -1.
	static int LicenceRowIndex(string rowName)
	{
		return PrefixedIndex(LICENCE_PREFIX, rowName);
	}

	//------------------------------------------------------------------------------------------------
	protected static int PrefixedIndex(string rowPrefix, string rowName)
	{
		int prefix = rowPrefix.Length();
		int suffix = ROW_SUFFIX.Length();
		if (rowName.Length() <= prefix + suffix)
			return -1;

		if (rowName.Substring(0, prefix) != rowPrefix || rowName.Substring(rowName.Length() - suffix, suffix) != ROW_SUFFIX)
			return -1;

		string digits = rowName.Substring(prefix, rowName.Length() - prefix - suffix);
		int index = digits.ToInt();
		if (index.ToString() != digits)
			return -1;

		return index;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a counter with these lines buys anything back, so its till needs a Sell key.
	static bool BuysBack(array<ref LF_ShopCatalogItem> items)
	{
		if (!items)
			return false;

		foreach (LF_ShopCatalogItem item : items)
		{
			if (item && item.m_iSellPrice > 0)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Build()
	{
		m_bSells = BuysBack(m_Shop.GetShopItems());
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", WidgetManager.Translate(m_Shop.GetShopName())));
		m_iShownCash = LF_MoneyUtils.GetCash(LF_Utils.GetLocalCharacter());
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Cash", CashText(m_iShownCash)));
		if (!m_Shop.GetShopHint().IsEmpty())
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Hint", m_Shop.GetShopHint()));

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, "Search", "#LF-Shop_LookFor"));
		parts.Insert(LF_UIPart.CreateList("Prices").Compact());
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Order", "#LF-Shop_SlipEmpty"));
		parts.Insert(LF_ShopItemPreview.Part());
		parts.Insert(LF_ATMTerminal.Keys("Quantity", LESS + "," + MORE, "#LF-Shop_Less,#LF-Shop_More"));
		if (m_bSells)
			parts.Insert(LF_ATMTerminal.Keys("Till", PAY + "," + SELL, "#LF-Shop_Pay,#LF-Shop_SellBack"));
		else
			parts.Insert(LF_ATMTerminal.Keys("Till", PAY, "#LF-Shop_Pay"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Receipt", ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "CloseButton", "#LF-Shop_Leave"));

		m_Screen = LF_UIScreen.Open(parts, LF_UISkin.PriceList());
		if (!m_Screen)
		{
			LF_Debug.Error("Shop", "price list failed to open");
			return false;
		}

		m_Screen.SetListHeight("Prices", LIST_HEIGHT);
		m_Preview.Attach(m_Screen);
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
		// The licence rows head the list: the permit is what opens the rest of a gun counter.
		FillLicenceRows();

		int total = 0;
		foreach (LF_ShopCatalogItem item : m_Shop.GetShopItems())
		{
			// A line the counter only buys (a fence's buyback) shows what it pays instead.
			if (!item || (item.m_iBuyPrice <= 0 && item.m_iSellPrice <= 0))
				continue;

			string shownName = WidgetManager.Translate(item.m_sDisplayName);
			if (!Matches(shownName, m_sFilter))
				continue;

			total++;
			if (m_aShown.Count() >= MAX_ROWS)
				continue;

			m_aShown.Insert(item);
			if (item.m_iBuyPrice > 0)
				m_Screen.AddListRow("Prices", RowName(m_aShown.Count() - 1), shownName, LF_MoneyFormat.FormatMoney(item.m_iBuyPrice), "#LF-Shop_Order");
			else
				m_Screen.AddListRow("Prices", RowName(m_aShown.Count() - 1), shownName, WidgetManager.Translate("#LF-Shop_WePay", LF_MoneyFormat.FormatMoney(item.m_iSellPrice)), "#LF-Shop_Offer");
		}

		if (total > m_aShown.Count())
			LF_Debug.Info("Shop", string.Format("price list shows %1 of %2 lines; typing narrows it", m_aShown.Count(), total));
	}

	//------------------------------------------------------------------------------------------------
	//! The counter's licence rows (a permit applied for and collected here), above the item lines.
	protected void FillLicenceRows()
	{
		m_aLicences.Clear();
		m_aLicenceFees.Clear();
		array<LF_OutletOffer> rows = {};
		m_Shop.GetLicenceRows(rows);
		foreach (LF_OutletOffer row : rows)
		{
			LF_ELicenseType licence;
			if (!LF_ItemCatalogue.ParseLicence(row.m_sGrantsLicence, licence))
				continue;

			string shownName = WidgetManager.Translate(LF_LicenceDesk.NameKey(licence));
			if (!Matches(shownName, m_sFilter))
				continue;

			int fee = row.LicenceFee();
			m_aLicences.Insert(row.m_sGrantsLicence);
			m_aLicenceFees.Insert(fee);
			m_Screen.AddListRow("Prices", LicenceRowName(m_aLicences.Count() - 1), shownName, LF_MoneyFormat.FormatMoney(fee), "#LF-Shop_Order");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The edit box has no change event on the kit, so the list re-reads it a few times a second. The
	//! Cash line follows the carried cash on the same tick, so a paid till shows the new figure as soon
	//! as the cash change replicates, whether or not the till's reply has come back (re-review N2).
	protected void PollSearch()
	{
		if (!m_Screen || !m_Screen.IsOpen())
			return;

		if (System.GetTickCount() - m_iReplyTick >= REPLY_HOLD_MS)
			ShowCash(LF_MoneyUtils.GetCash(LF_Utils.GetLocalCharacter()));

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

		int licenceIndex = LicenceRowIndex(name);
		if (licenceIndex >= 0)
		{
			SelectLicence(licenceIndex);
			return;
		}

		if (name == LESS || name == MORE)
		{
			// A licence is one document: the quantity stays 1.
			if (!m_sSelectedLicence.IsEmpty())
			{
				LF_UISound.Play(m_sRefuseSound);
				return;
			}

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
		m_sSelectedLicence = string.Empty;
		m_Selected = item.m_ItemPrefab;
		m_Preview.Draw(m_Screen, LF_ShopItemPreview.Resolve(m_aShown, index));
		m_sSelectedName = WidgetManager.Translate(item.m_sDisplayName);
		m_iSelectedPrice = item.m_iBuyPrice;
		m_iSelectedSell = item.m_iSellPrice;
		m_iQuantity = 1;
		LF_UISound.Play(m_sKeySound);
		SetReceipt("");
		ShowSlip();
	}

	//------------------------------------------------------------------------------------------------
	protected void SelectLicence(int index)
	{
		if (index < 0 || index >= m_aLicences.Count())
		{
			LF_Debug.Warn("Shop", "order refused: no licence row " + index.ToString());
			return;
		}

		LF_ELicenseType licence;
		LF_ItemCatalogue.ParseLicence(m_aLicences[index], licence);
		m_sSelectedLicence = m_aLicences[index];
		m_Selected = ResourceName.Empty;
		m_Preview.Draw(m_Screen, ResourceName.Empty);
		m_sSelectedName = WidgetManager.Translate(LF_LicenceDesk.NameKey(licence));
		m_iSelectedPrice = m_aLicenceFees[index];
		m_iSelectedSell = 0;
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

		if (!m_sSelectedLicence.IsEmpty())
		{
			m_Screen.SetLine("Order", WidgetManager.Translate("#LF-Shop_SlipLine", 1, m_sSelectedName));
			m_Screen.SetButtonText(PAY, WidgetManager.Translate("#LF-Shop_PayTotal", LF_MoneyFormat.FormatMoney(m_iSelectedPrice)));
			SetSellText("#LF-Shop_SellBack");
			return;
		}

		if (m_Selected.IsEmpty())
		{
			m_Screen.SetLine("Order", "#LF-Shop_SlipEmpty");
			m_Screen.SetButtonText(PAY, "#LF-Shop_Pay");
			return;
		}

		m_Screen.SetLine("Order", WidgetManager.Translate("#LF-Shop_SlipLine", m_iQuantity, m_sSelectedName));
		if (m_iSelectedPrice <= 0 && m_iSelectedSell > 0)
		{
			int paid = LF_ShopRules.StackTradeTotal(m_iSelectedSell, m_iQuantity);
			m_Screen.SetButtonText(PAY, "#LF-Shop_Pay");
			SetSellText(WidgetManager.Translate("#LF-Shop_SellTotal", LF_MoneyFormat.FormatMoney(paid)));
			return;
		}

		int total = LF_ShopRules.StackTradeTotal(m_iSelectedPrice, m_iQuantity);
		m_Screen.SetButtonText(PAY, WidgetManager.Translate("#LF-Shop_PayTotal", LF_MoneyFormat.FormatMoney(total)));
		// A line the counter also buys back says what it pays ("Sell for $75"), not a bare "Sell these".
		if (m_iSelectedSell > 0)
			SetSellText(WidgetManager.Translate("#LF-Shop_SellTotal", LF_MoneyFormat.FormatMoney(LF_ShopRules.StackTradeTotal(m_iSelectedSell, m_iQuantity))));
		else
			SetSellText("#LF-Shop_SellBack");
	}

	//------------------------------------------------------------------------------------------------
	//! Relabels the Sell key; a buy-only till has none.
	protected void SetSellText(string text)
	{
		if (m_bSells)
			m_Screen.SetButtonText(SELL, text);
	}

	//------------------------------------------------------------------------------------------------
	protected void Send(bool buy)
	{
		if (m_bWaiting)
		{
			LF_Debug.Info("Shop", "till busy: request ignored");
			return;
		}

		if (m_Selected.IsEmpty() && m_sSelectedLicence.IsEmpty())
		{
			SetReceipt("#LF-Shop_PickFirst");
			LF_UISound.Play(m_sRefuseSound);
			return;
		}

		if (!m_sSelectedLicence.IsEmpty() && !buy)
		{
			SetReceipt("#LF-Shop_LicenceNoSellBack");
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

		if (!m_sSelectedLicence.IsEmpty())
			bridge.RequestLicence(m_ShopId, m_sSelectedLicence);
		else if (buy)
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
	static void OnResult(bool buy, LF_EShopRefusal reason, string prefab, int quantity, int money, int cash, string missing = "")
	{
		if (!s_Current)
		{
			LF_Debug.Info("Shop", "till reply arrived after the price list closed");
			return;
		}

		s_Current.ApplyResult(buy, reason, money, cash, missing);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the counter's answer to a licence row (a counter refusal, or the licence office's outcome).
	static void OnLicenceResult(LF_EShopRefusal reason, int outcome, string arg, int cash)
	{
		if (!s_Current)
		{
			LF_Debug.Info("Shop", "licence reply arrived after the price list closed");
			return;
		}

		s_Current.ApplyLicenceResult(reason, outcome, arg, cash);
	}

	//------------------------------------------------------------------------------------------------
	//! The slip text for a licence answer: the counter's refusal, else the office's answer for outcome.
	static string LicenceReceipt(LF_EShopRefusal reason, int outcome, string arg)
	{
		if (reason != LF_EShopRefusal.NONE)
			return LF_ShopRefusal.Text(reason, string.Empty);

		LF_ELicenceDesk answer = outcome;
		return LF_Terms.Text(LF_LicenceDesk.CounterAnswerKey(answer), arg);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyLicenceResult(LF_EShopRefusal reason, int outcome, string arg, int cash)
	{
		m_bWaiting = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ReplyTimedOut);

		m_iReplyTick = System.GetTickCount();
		ShowCash(cash);
		SetReceipt(LicenceReceipt(reason, outcome, arg));
		bool good = reason == LF_EShopRefusal.NONE && (outcome == LF_ELicenceDesk.ISSUED || outcome == LF_ELicenceDesk.APPLIED || outcome == LF_ELicenceDesk.WAITING || outcome == LF_ELicenceDesk.ALREADY_HELD);
		if (good)
			LF_UISound.Play(m_sTillSound);
		else
			LF_UISound.Play(m_sRefuseSound);

		LF_Debug.Info("Shop", string.Format("slip shows licence answer %1 / %2", typename.EnumToString(LF_EShopRefusal, reason), typename.EnumToString(LF_ELicenceDesk, outcome)));
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyResult(bool buy, LF_EShopRefusal reason, int money, int cash, string missing)
	{
		m_bWaiting = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ReplyTimedOut);

		m_iReplyTick = System.GetTickCount();
		ShowCash(cash);
		if (ClearsSlipAfter(buy, reason))
			ClearSlip();

		if (reason != LF_EShopRefusal.NONE)
		{
			SetReceipt(LF_ShopRefusal.Text(reason, missing));
			LF_UISound.Play(m_sRefuseSound);
			LF_Debug.Info("Shop", string.Format("slip shows refusal %1 %2", typename.EnumToString(LF_EShopRefusal, reason), missing));
			return;
		}

		if (buy)
			SetReceipt(WidgetManager.Translate("#LF-Shop_Paid", LF_MoneyFormat.FormatMoney(money)));
		else
			SetReceipt(WidgetManager.Translate("#LF-Shop_SoldFor", LF_MoneyFormat.FormatMoney(money)));

		LF_UISound.Play(m_sTillSound);
	}

	//------------------------------------------------------------------------------------------------
	//! A paid order is done: the slip empties so the sheet shows the sale went through (re-review N2).
	//! A refusal or a sale back keeps the slip, so the player can change it and try again.
	static bool ClearsSlipAfter(bool buy, LF_EShopRefusal reason)
	{
		return buy && reason == LF_EShopRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the Cash line must be redrawn: a known carried amount that differs from the one shown.
	static bool CashLineStale(int shown, int carried)
	{
		return carried >= 0 && carried != shown;
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowCash(int cash)
	{
		if (!m_Screen || !CashLineStale(m_iShownCash, cash))
			return;

		m_iShownCash = cash;
		m_Screen.SetLine("Cash", CashText(cash));
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearSlip()
	{
		m_Selected = ResourceName.Empty;
		m_sSelectedLicence = string.Empty;
		m_sSelectedName = string.Empty;
		m_iSelectedPrice = 0;
		m_iSelectedSell = 0;
		m_iQuantity = 1;
		m_Preview.Draw(m_Screen, ResourceName.Empty);
		ShowSlip();
		SetSellText("#LF-Shop_SellBack");
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
		m_Preview.Clear();
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
