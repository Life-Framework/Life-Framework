//------------------------------------------------------------------------------------------------
//! One good a seller carries that a trader buys: the action rows' unit of sale.
class LF_TraderSellGood
{
	ResourceName m_sPrefab;
	//! Carried entities of this good (each stack is one entity).
	int m_iItems;
	//! Units over all those entities (a stack counts its quantity).
	int m_iUnits;
	//! Units of the first entity (a stack's quantity).
	int m_iFirstUnits;
}

//------------------------------------------------------------------------------------------------
//! The trader's in-world sell rows (A21, B11 of the first-two-hours review): no inventory window. Look at
//! the buyer and scroll: one "Sell 1 Apple Juice: $18" row (one unit) per good the player carries that the buyer
//! takes, in the buyer's list order, and a "Sell all goods (6 items): $108" row. The goods come from the
//! player's own replicated inventory; the prices are the server's (LF_TraderQuoteLive), because the
//! market curve and the premium buyers' purses live on the server only.
class LF_TraderSellRows
{
	//! Rows for single goods on one trader; more goods than this are still sold by "Sell all".
	static const int MAX_GOOD_ROWS = 5; // lf-fixed: the action list fits five rows plus "Sell all" without scrolling far
	//! A price the client does not know yet.
	static const int UNKNOWN = -1;

	//------------------------------------------------------------------------------------------------
	//! Pure: groups carried entities (one prefab and unit count per entity) into the goods `list` buys,
	//! in the list's order. A list row worth 0 or less is not bought and gives no good.
	static array<ref LF_TraderSellGood> Group(array<ref LF_TraderItem> list, array<ResourceName> prefabs, array<int> units)
	{
		array<ref LF_TraderSellGood> goods = {};
		if (!list || !prefabs || !units)
			return goods;

		foreach (LF_TraderItem row : list)
		{
			if (!row || row.m_ValuePerItem <= 0 || row.m_ItemPrefab.IsEmpty())
				continue;

			LF_TraderSellGood good;
			for (int i = 0; i < prefabs.Count() && i < units.Count(); i++)
			{
				if (units[i] <= 0 || !LF_Utils.SamePrefab(row.m_ItemPrefab, prefabs[i]))
					continue;

				if (!good)
				{
					// A good listed twice keeps its first row.
					if (Find(goods, prefabs[i]))
						break;

					good = new LF_TraderSellGood();
					good.m_sPrefab = prefabs[i];
					good.m_iFirstUnits = units[i];
					goods.Insert(good);
				}

				good.m_iItems++;
				good.m_iUnits += units[i];
			}
		}

		return goods;
	}

	//------------------------------------------------------------------------------------------------
	//! Groups real entities (see Group). Stack size is the LF_QuantityComponent quantity, else 1.
	static array<ref LF_TraderSellGood> GroupEntities(array<ref LF_TraderItem> list, array<IEntity> items)
	{
		array<ResourceName> prefabs = {};
		array<int> units = {};
		if (items)
		{
			foreach (IEntity item : items)
			{
				if (!item)
					continue;

				prefabs.Insert(LF_Utils.GetPrefabName(item));
				units.Insert(UnitsOf(item));
			}
		}

		return Group(list, prefabs, units);
	}

	//------------------------------------------------------------------------------------------------
	static int UnitsOf(IEntity item)
	{
		LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(item);
		if (!quantity)
			return 1;

		return quantity.GetQuantity();
	}

	//------------------------------------------------------------------------------------------------
	static LF_TraderSellGood Find(array<ref LF_TraderSellGood> goods, ResourceName prefab)
	{
		foreach (LF_TraderSellGood good : goods)
		{
			if (good && LF_Utils.SamePrefab(good.m_sPrefab, prefab))
				return good;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the units over every good (the "Sell all" row's count).
	static int TotalUnits(array<ref LF_TraderSellGood> goods)
	{
		int total;
		foreach (LF_TraderSellGood good : goods)
		{
			if (good)
				total += good.m_iUnits;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the string key for a single good's row, "Sell 1 Apple Juice: $18" (one unit, also off a
	//! stack); without a known price the row has no "$" part.
	static string GoodKey(int price)
	{
		if (price > 0)
			return "#LF-Trader_SellOne";
		return "#LF-Trader_SellOneAsk";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the string key for the "Sell all" row.
	static string AllKey(int units, int price)
	{
		if (units <= 0)
			return "#LF-Trader_SellNothing";
		if (price > 0)
			return "#LF-Trader_SellAll";
		return "#LF-Trader_SellAllAsk";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a stable text for what the rows show; a change asks the server for fresh prices.
	static string Signature(array<ref LF_TraderSellGood> goods)
	{
		string sig;
		foreach (LF_TraderSellGood good : goods)
		{
			if (good)
				sig += string.Format("%1:%2:%3;", good.m_sPrefab, good.m_iItems, good.m_iUnits);
		}

		return sig;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the first carried entity of `prefab` (what a "Sell 1" row sells), or null.
	static IEntity FirstCarried(IEntity user, ResourceName prefab)
	{
		foreach (IEntity item : LF_TraderSale.CarriedItems(user))
		{
			if (item && LF_Utils.SamePrefab(LF_Utils.GetPrefabName(item), prefab))
				return item;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! Client only: the rows' goods and the server's prices for the trader the player stands at. The goods are
//! recounted once per frame at most; the prices are asked again when the goods change (a sale, a pickup),
//! when the player moves to another trader, and every few seconds while the rows are on screen (the market
//! moves). Nothing here is authority: the sale is priced again on the server.
class LF_TraderQuoteLive
{
	static const int ASK_GAP_MS = 1000;		// lf-fixed: at most one quote ask a second, inside the INVENTORY rate bucket
	static const int REFRESH_MS = 8000;		// lf-fixed: refresh of an unchanged quote while the rows show (market drift)

	protected static RplId s_TraderId = RplId.Invalid();
	protected static string s_sAskedSig;
	protected static float s_fAskedAt = -100000;

	protected static RplId s_ReplyTraderId = RplId.Invalid();
	protected static ref array<string> s_aGoods;
	protected static ref array<int> s_aOne;
	protected static ref array<int> s_aAll;

	protected static IEntity s_RowsOwner;
	protected static float s_fRowsAt = -1;
	protected static ref array<ref LF_TraderSellGood> s_aRows;

	//------------------------------------------------------------------------------------------------
	//! Client: the goods `user` carries that the trader on `owner` buys, counted at most once a frame.
	static array<ref LF_TraderSellGood> Rows(IEntity owner, IEntity user)
	{
		float now = Now();
		if (s_aRows && owner == s_RowsOwner && now == s_fRowsAt)
			return s_aRows;

		s_RowsOwner = owner;
		s_fRowsAt = now;
		s_aRows = {};
		LF_TraderManagerComponent trader;
		if (owner)
			trader = LF_Component<LF_TraderManagerComponent>.Find(owner);
		if (trader && user)
			s_aRows = LF_TraderSellRows.GroupEntities(trader.m_aTradableItems, LF_TraderSale.CarriedItems(user));

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks for prices when the trader or the goods changed, or the quote is old.
	static void Near(IEntity owner, IEntity user)
	{
		string signature = LF_TraderSellRows.Signature(Rows(owner, user));
		RplId traderId = LF_NetworkUtils.GetRplId(owner);
		if (!traderId.IsValid())
			return;

		float now = Now();
		float since = now - s_fAskedAt;
		if (since < ASK_GAP_MS)
			return;

		if (traderId == s_TraderId && signature == s_sAskedSig && since < REFRESH_MS)
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return;

		s_TraderId = traderId;
		s_sAskedSig = signature;
		s_fAskedAt = now;
		controller.LF_RequestTraderQuote(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server's prices arrived.
	static void OnReply(RplId traderId, array<string> goods, array<int> onePrices, array<int> allPrices)
	{
		s_ReplyTraderId = traderId;
		s_aGoods = {};
		s_aOne = {};
		s_aAll = {};
		if (!goods || !onePrices || !allPrices)
			return;

		int count = LF_Utils.MinInt(goods.Count(), LF_Utils.MinInt(onePrices.Count(), allPrices.Count()));
		for (int i = 0; i < count; i++)
		{
			s_aGoods.Insert(goods[i]);
			s_aOne.Insert(onePrices[i]);
			s_aAll.Insert(allPrices[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The server's price for one item (`all` false) or all carried items of `prefab` at the
	//! trader on `owner`, or LF_TraderSellRows.UNKNOWN before the answer came.
	static int Price(IEntity owner, ResourceName prefab, bool all)
	{
		RplId traderId = LF_NetworkUtils.GetRplId(owner);
		// The reply arrays are created by the first reply (no static initializer, #2755).
		if (!traderId.IsValid() || traderId != s_ReplyTraderId || !s_aGoods || !s_aOne || !s_aAll)
			return LF_TraderSellRows.UNKNOWN;

		for (int i = 0; i < s_aGoods.Count(); i++)
		{
			if (!LF_Utils.SamePrefab(s_aGoods[i], prefab))
				continue;

			if (all)
				return s_aAll[i];
			return s_aOne[i];
		}

		return LF_TraderSellRows.UNKNOWN;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The sum of the "all" prices for `goods`, UNKNOWN when one of them is not priced yet.
	static int TotalPrice(IEntity owner, array<ref LF_TraderSellGood> goods)
	{
		int total;
		foreach (LF_TraderSellGood good : goods)
		{
			int price = Price(owner, good.m_sPrefab, true);
			if (price == LF_TraderSellRows.UNKNOWN)
				return LF_TraderSellRows.UNKNOWN;

			total += price;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	protected static float Now()
	{
		World world = GetGame().GetWorld();
		if (!world)
			return 0;

		return world.GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_TraderId = RplId.Invalid();
		s_sAskedSig = string.Empty;
		s_fAskedAt = -100000;
		s_ReplyTraderId = RplId.Invalid();
		s_aGoods = {};
		s_aOne = {};
		s_aAll = {};
		s_RowsOwner = null;
		s_fRowsAt = -1;
		s_aRows = {};
	}
}

//------------------------------------------------------------------------------------------------
//! One row of a buyer's in-world sell list. m_iRow 0..4 is "Sell 1 <good>: $X" for the n-th good the
//! player carries that this buyer takes; m_iRow -1 is "Sell all goods (n items): $Y", which is always
//! there so the buyer reads as a buyer even to a player with empty pockets. No window opens: the server
//! sells (LF_TraderSale.Sell / SellAll) and the toast shows the cash.
[BaseContainerProps()]
class LF_TraderSellAction : ScriptedUserAction
{
	static const int ALL = -1;

	[Attribute("-1", desc: "Which row: 0..4 is the n-th good the player carries that this buyer takes; -1 is Sell all")]
	protected int m_iRow;

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The good this row sells, or null for the "all" row or a row past the carried goods.
	protected LF_TraderSellGood RowGood(IEntity user)
	{
		if (m_iRow < 0 || m_iRow >= LF_TraderSellRows.MAX_GOOD_ROWS)
			return null;

		array<ref LF_TraderSellGood> goods = LF_TraderQuoteLive.Rows(GetOwner(), user);
		if (m_iRow >= goods.Count())
			return null;

		return goods[m_iRow];
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!user || !LF_Systems.IsEnabled(LF_OpenTraderAction.SYSTEM))
			return false;

		// Only the local player's own rows ask the server for prices.
		if (m_iRow == ALL && LF_CounterAccess.IsLocalActor(user, LF_Utils.GetLocalCharacter()))
			LF_TraderQuoteLive.Near(GetOwner(), user);

		if (m_iRow == ALL)
			return true;

		return RowGood(user) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (m_iRow == ALL)
		{
			if (LF_TraderSellRows.TotalUnits(LF_TraderQuoteLive.Rows(GetOwner(), user)) > 0)
				return true;

			SetCannotPerformReason("#LF-Trader_NothingToSell");
			return false;
		}

		LF_TraderSellGood good = RowGood(user);
		if (!good)
			return false;

		// A known price of 0 is a buyer that is full or a part lot below the minimum: say so, sell nothing.
		if (LF_TraderQuoteLive.Price(GetOwner(), good.m_sPrefab, false) == 0)
		{
			SetCannotPerformReason("#LF-Trader_NotBuyingNow");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		IEntity user = LF_Utils.GetLocalCharacter();
		IEntity owner = GetOwner();
		if (m_iRow == ALL)
		{
			array<ref LF_TraderSellGood> goods = LF_TraderQuoteLive.Rows(owner, user);
			int units = LF_TraderSellRows.TotalUnits(goods);
			int total = LF_TraderQuoteLive.TotalPrice(owner, goods);
			string allKey = LF_TraderSellRows.AllKey(units, total);
			outName = LF_Terms.Text(allKey, units.ToString(), total.ToString());
			return true;
		}

		LF_TraderSellGood good = RowGood(user);
		if (!good)
			return false;

		int price = LF_TraderQuoteLive.Price(owner, good.m_sPrefab, false);
		string name = WidgetManager.Translate(LF_ShopManagerComponent.ResolveDisplayName(good.m_sPrefab));
		string key = LF_TraderSellRows.GoodKey(price);
		outName = LF_Terms.Text(key, name, price.ToString());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error("Trader", "sell row performed without a buyer or a user");
			return;
		}

		if (!LF_CounterAccess.IsLocalActor(pUserEntity, LF_Utils.GetLocalCharacter()))
			return;

		if (LF_Systems.Refuses(LF_OpenTraderAction.SYSTEM, "trader sell row"))
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Warn("Trader", "sell row: no local player controller");
			return;
		}

		if (m_iRow == ALL)
		{
			controller.LF_RequestTraderSellAll(pOwnerEntity);
			return;
		}

		LF_TraderSellGood good = RowGood(pUserEntity);
		if (!good)
			return;

		IEntity item = LF_TraderSellRows.FirstCarried(pUserEntity, good.m_sPrefab);
		if (!item)
		{
			LF_Debug.Warn("Trader", "sell row: the good is no longer carried: " + good.m_sPrefab);
			return;
		}

		controller.LF_RequestTraderSellOne(item, pOwnerEntity);
	}
}
