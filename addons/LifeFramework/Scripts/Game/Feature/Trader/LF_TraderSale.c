//------------------------------------------------------------------------------------------------
//! How a trader sale ended. SOLD is the only outcome that took the item.
enum LF_ETraderSale
{
	SOLD,
	NOT_SERVER,
	NO_SELLER,
	NO_ITEM,
	NO_TRADER,
	NOT_CARRIED,
	OUT_OF_REACH,
	NOT_BOUGHT,
	BLACK_MARKET,
	UNPAYABLE,
	NOT_REMOVED,
	SELLER_DOWN,	//!< A dead or downed seller (a forged request from a body)
	BUYER_FULL,		//!< A premium buyer with no demand left (#3599); the seller keeps the item
	BELOW_LOT		//!< The stack is smaller than one lot of a good priced per lot (#3622); the seller keeps the item
}

//------------------------------------------------------------------------------------------------
//! The one place a trader sale resolves, and it resolves on the server. The client's inventory
//! drag only forwards a request (SCR_PlayerController.LF_RequestTraderSale); money and items
//! change here. Pay before deleting: a sale that cannot pay out never consumes the item.
class LF_TraderSale
{
	//! How far the seller may stand from the trader's storage for a client request.
	protected static bool s_bFailRemoveForTest;
	//! True while SellAll runs: the per-item toasts are held back for one summary toast.
	protected static bool s_bQuiet;
	//! Cash paid by the sales since SellAll started counting.
	protected static int s_iBatchPaid;

	static const float REACH = 5.0; // lf-fixed: an anti-forgery bound at the inventory vicinity range, same as the hideout stash reach

	//------------------------------------------------------------------------------------------------
	//! \return true when a seller at sellerPos is close enough to the trader at traderPos.
	static bool InReach(vector sellerPos, vector traderPos)
	{
		return vector.Distance(sellerPos, traderPos) <= REACH;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sells `item` from `seller` to the trader on `traderOwner` (the entity that carries
	//! LF_TraderManagerComponent). The price comes from the trader's own list, never from the caller.
	//! \param untrusted true for a client request: the item must be in the seller's inventory and
	//! the seller within reach. false for server code and the listen host's own drag (the inventory
	//! UI there already enforces both).
	//! \param maxUnits sell at most this many units of the stack (a "Sell 1" row passes 1); 0 sells it all.
	//! \return SOLD, or why the sale was refused; a refusal changes nothing.
	static LF_ETraderSale Sell(IEntity seller, IEntity item, IEntity traderOwner, bool untrusted, int maxUnits = 0)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Error("Trader", "sale refused: it resolves on the server only");
			return LF_ETraderSale.NOT_SERVER;
		}

		if (!seller)
			return Refuse(null, LF_ETraderSale.NO_SELLER, "no seller character");

		if (!item)
			return Refuse(seller, LF_ETraderSale.NO_ITEM, "no item");

		LF_TraderManagerComponent trader;
		if (traderOwner)
			trader = LF_TraderManagerComponent.Cast(traderOwner.FindComponent(LF_TraderManagerComponent));
		if (!trader)
			return Refuse(seller, LF_ETraderSale.NO_TRADER, "not a trader");

		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(seller);
		if (!inventory)
			return Refuse(seller, LF_ETraderSale.NO_SELLER, "seller has no inventory");

		if (untrusted)
		{
			if (!LF_Utils.IsAliveState(seller))
				return Refuse(seller, LF_ETraderSale.SELLER_DOWN, "seller is dead or downed");

			if (!InReach(seller.GetOrigin(), traderOwner.GetOrigin()))
				return Refuse(seller, LF_ETraderSale.OUT_OF_REACH, "seller is out of reach of the trader");

			if (!inventory.Contains(item))
				return Refuse(seller, LF_ETraderSale.NOT_CARRIED, "item is not in the seller inventory");
		}

		ResourceName itemPrefab = LF_Utils.GetPrefabName(item);
		int unitValue = LF_TraderManagerComponent.GetUnitValue(trader.m_aTradableItems, itemPrefab);
		if (unitValue <= 0)
			return Refuse(seller, LF_ETraderSale.NOT_BOUGHT, "the trader does not buy " + itemPrefab);

		if (trader.m_bBlackMarket && !LF_TraderManagerComponent.CanAccessBlackMarket(GetPlayerAccount(seller)))
			return Refuse(seller, LF_ETraderSale.BLACK_MARKET, "the black market refuses this seller");

		// One output unit per LF_QuantityComponent quantity, then one cash unit per
		// output. Ported from rekterakathom/EveronLife (MIT), commit
		// 482b74a5a7e39e999d129b0657253696b9b8be15. This trader pays cash rather
		// than spawning a barter prefab; the count is the same rule.
		int stackUnits = LF_ShopRules.StackTradeOutputCount(GetItemQuantity(item));
		int outputUnits = stackUnits;
		if (maxUnits > 0)
			outputUnits = LF_Utils.MinInt(stackUnits, maxUnits);

		// Premium buyer (#3599): a trader that names a buyer id pays that buyer's row, up to its demand.
		if (!trader.m_sBuyerId.IsEmpty())
		{
			LF_BuyerDemandRow demandRow = LF_BuyerDemandBook.FindRow(trader.m_sBuyerId, itemPrefab);
			if (demandRow)
				return SellToBuyer(seller, item, inventory, trader.m_sBuyerId, itemPrefab, outputUnits, stackUnits, untrusted);
		}

		int totalValue = LF_ShopRules.StackTradeTotal(unitValue, outputUnits);

		// Market (#2845): a curved good pays the curve's marginal sum instead of the fixed value. The
		// trader list still decides whether the good is bought at all; market off changes nothing.
		bool curved = LF_MarketBook.Curved(itemPrefab) != null;
		int nowUnix = System.GetUnixTime();
		// A good priced per lot (#3622) sells whole lots only; the part lot stays in the stack.
		int soldUnits = outputUnits;
		if (curved)
		{
			soldUnits = LF_MarketBook.SellableUnits(itemPrefab, outputUnits);
			if (soldUnits <= 0)
			{
				NotifyBelowLot(seller, LF_MarketBook.LotOf(itemPrefab));
				LF_Debug.Warn("Trader", string.Format("sale refused (BELOW_LOT): %1 of %2 is less than one lot of %3", outputUnits, itemPrefab, LF_MarketBook.LotOf(itemPrefab)));
				return LF_ETraderSale.BELOW_LOT;
			}

			totalValue = LF_MarketBook.QuoteAt(itemPrefab, soldUnits, nowUnix);
			if (totalValue <= 0)
				return Refuse(seller, LF_ETraderSale.NOT_BOUGHT, "the market pays nothing for " + itemPrefab);
		}

		// Pay first, then move the price, then take the item; any failure rolls back what was done.
		int paid = LF_MoneyUtils.AddCash(seller, totalValue, "trader-sell");
		if (paid != totalValue)
		{
			if (paid > 0)
				LF_MoneyUtils.RemoveCash(seller, paid, "trader-sell-rollback");
			return Refuse(seller, LF_ETraderSale.UNPAYABLE, string.Format("cash payout failed (%1 of %2)", paid, totalValue));
		}

		LF_MarketPrice priceBefore;
		int oldPrice;
		int newPrice;
		if (curved)
		{
			priceBefore = LF_MarketBook.Snapshot(itemPrefab);
			newPrice = LF_MarketBook.ApplySaleAt(itemPrefab, soldUnits, nowUnix, oldPrice);
		}

		bool removed = false;
		if (!s_bFailRemoveForTest)
			removed = TakeUnits(inventory, item, soldUnits, stackUnits);

		if (!removed)
		{
			LF_MoneyUtils.RemoveCash(seller, paid, "trader-sell-rollback");
			if (curved)
				LF_MarketBook.Restore(itemPrefab, priceBefore);
			return Refuse(seller, LF_ETraderSale.NOT_REMOVED, "the item could not be removed");
		}

		if (curved && newPrice != oldPrice)
			LF_EventBus.Publish(LF_OnMarketPriceMoved.Create(itemPrefab, oldPrice, newPrice));

		LF_Debug.Info("Trader", string.Format("sold %1 x%2 of %3 for %4 (untrusted=%5)", itemPrefab, soldUnits, outputUnits, totalValue, untrusted));
		s_iBatchPaid += totalValue;
		Tell(seller, "#EL-Item_Sold", "#EL-Trade_Successful", 3.0, totalValue.ToString());
		if (curved && newPrice != oldPrice && !s_bQuiet)
			LF_MarketSaleNotice.OnSale(seller, itemPrefab, oldPrice, newPrice);
		LF_QuestArrivalSignals.ReportTraderSale(seller, itemPrefab);
		return LF_ETraderSale.SOLD;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the premium-buyer half of Sell (#3599). The buyer's purse decides how many of the stack's
	//! units are taken (the count is the server's own, from the item); the seller is paid for those, the
	//! rest stays in the stack, and a buyer that has no demand left refuses with the full toast. Pay first,
	//! then take the units; a failure puts the cash and the purse back.
	//! \return SOLD (also for a part-stack sale), BUYER_FULL, or why the sale was refused.
	protected static LF_ETraderSale SellToBuyer(IEntity seller, IEntity item, SCR_InventoryStorageManagerComponent inventory, string buyer, ResourceName itemPrefab, int outputUnits, int stackUnits, bool untrusted)
	{
		int nowUnix = System.GetUnixTime();
		LF_BuyerPurse purseBefore = LF_BuyerDemandBook.Snapshot(buyer, itemPrefab);
		LF_BuyerDemandSale sale = LF_BuyerDemandBook.Sell(buyer, itemPrefab, outputUnits, nowUnix);
		if (!sale)
			return Refuse(seller, LF_ETraderSale.NOT_BOUGHT, "the buyer has no demand row for " + itemPrefab);

		if (sale.m_iAccepted <= 0 && sale.m_iRefused <= sale.m_iUnderLot)
		{
			// Nothing short of a whole lot to sell (#3622): the buyer has room, the stack is just too small.
			int lot = LF_BuyerDemandBook.LotOf(buyer, itemPrefab);
			NotifyBelowLot(seller, lot);
			LF_Debug.Warn("Trader", string.Format("sale refused (BELOW_LOT): %1 of %2 is less than one lot of %3 for %4", outputUnits, itemPrefab, lot, buyer));
			return LF_ETraderSale.BELOW_LOT;
		}

		if (sale.m_iAccepted <= 0)
		{
			NotifyBuyerFull(seller, sale.m_iSecondsToNext);
			LF_Debug.Warn("Trader", string.Format("sale refused (BUYER_FULL): %1 has enough %2 (back in %3 s)", buyer, itemPrefab, sale.m_iSecondsToNext));
			return LF_ETraderSale.BUYER_FULL;
		}

		if (sale.m_iPayout <= 0)
		{
			LF_BuyerDemandBook.Restore(buyer, itemPrefab, purseBefore);
			return Refuse(seller, LF_ETraderSale.NOT_BOUGHT, "the buyer pays nothing for " + itemPrefab);
		}

		int paid = LF_MoneyUtils.AddCash(seller, sale.m_iPayout, "trader-sell");
		if (paid != sale.m_iPayout)
		{
			if (paid > 0)
				LF_MoneyUtils.RemoveCash(seller, paid, "trader-sell-rollback");
			LF_BuyerDemandBook.Restore(buyer, itemPrefab, purseBefore);
			return Refuse(seller, LF_ETraderSale.UNPAYABLE, string.Format("cash payout failed (%1 of %2)", paid, sale.m_iPayout));
		}

		bool removed = false;
		if (!s_bFailRemoveForTest)
			removed = TakeUnits(inventory, item, sale.m_iAccepted, stackUnits);

		if (!removed)
		{
			LF_MoneyUtils.RemoveCash(seller, paid, "trader-sell-rollback");
			LF_BuyerDemandBook.Restore(buyer, itemPrefab, purseBefore);
			return Refuse(seller, LF_ETraderSale.NOT_REMOVED, "the item could not be removed");
		}

		LF_Debug.Info("Trader", string.Format("sold %1 x%2 of %3 to %4 for %5 (refused %6, untrusted=%7)", itemPrefab, sale.m_iAccepted, outputUnits, buyer, sale.m_iPayout, sale.m_iRefused, untrusted));
		s_iBatchPaid += sale.m_iPayout;
		Tell(seller, "#EL-Item_Sold", "#EL-Trade_Successful", 3.0, sale.m_iPayout.ToString());
		LF_QuestArrivalSignals.ReportTraderSale(seller, itemPrefab);
		// Only what the purse could not hold says "enough"; a part lot just stays with the seller.
		if (sale.m_iRefused > sale.m_iUnderLot)
			NotifyBuyerFull(seller, sale.m_iSecondsToNext);

		return LF_ETraderSale.SOLD;
	}

	//------------------------------------------------------------------------------------------------
	//! Toast to the seller: the stack is smaller than one lot of a good priced per lot (#3622).
	protected static void NotifyBelowLot(IEntity seller, int lot)
	{
		Tell(seller, "#LF-Market_BelowLot", "#LF-Trader_SaleRefused_Title", 3.0, lot.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Takes `accepted` of the stack's `total` units: the whole item when all are taken, else the stack
	//! shrinks by `accepted`. \return false when the item could not be reduced.
	protected static bool TakeUnits(SCR_InventoryStorageManagerComponent inventory, IEntity item, int accepted, int total)
	{
		if (accepted >= total)
			return inventory.TryDeleteItem(item);

		LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(item);
		if (!quantity)
			return false;

		int change;
		quantity.RemoveQuantity(accepted, false, change);
		return quantity.GetQuantity() == total - accepted;
	}

	//------------------------------------------------------------------------------------------------
	//! Toast to the seller: the buyer has enough, and when it refills, in about how many minutes.
	protected static void NotifyBuyerFull(IEntity seller, int secondsToNext)
	{
		if (secondsToNext < 0)
		{
			Tell(seller, "#LF-Market_BuyerNever", "#LF-Market_BuyerFull_Title", 4.0);
			return;
		}

		int minutes = LF_Utils.MaxInt((secondsToNext + 59) / 60, 1);
		Tell(seller, "#LF-Market_BuyerFull", "#LF-Market_BuyerFull_Title", 4.0, minutes.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! A toast to the seller, unless SellAll is collecting the sales for one summary.
	protected static void Tell(IEntity seller, string message, string title, float duration, string arg = "")
	{
		if (s_bQuiet)
			return;

		LF_Utils.NotifyEntity(seller, message, title, duration, arg);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: what the trader would pay for units of a prefab right now, booking nothing. The same
	//! three prices Sell uses: a premium buyer's row, the market curve, else the trader's list value.
	//! \return 0 when the trader does not buy it, or a part lot that would be refused.
	static int QuoteUnits(LF_TraderManagerComponent trader, ResourceName prefab, int units, int nowUnix)
	{
		if (!trader || units <= 0)
			return 0;

		int unitValue = LF_TraderManagerComponent.GetUnitValue(trader.m_aTradableItems, prefab);
		if (unitValue <= 0)
			return 0;

		if (!trader.m_sBuyerId.IsEmpty() && LF_BuyerDemandBook.FindRow(trader.m_sBuyerId, prefab))
			return LF_BuyerDemandBook.QuoteAt(trader.m_sBuyerId, prefab, units, nowUnix);

		if (LF_MarketBook.Curved(prefab))
		{
			int sellable = LF_MarketBook.SellableUnits(prefab, units);
			if (sellable <= 0)
				return 0;

			return LF_MarketBook.QuoteAt(prefab, sellable, nowUnix);
		}

		return LF_ShopRules.StackTradeTotal(unitValue, units);
	}

	//------------------------------------------------------------------------------------------------
	//! Every item the seller carries, as a flat list (worn clothing and what sits inside it).
	static array<IEntity> CarriedItems(IEntity seller)
	{
		array<IEntity> found = {};
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(seller);
		if (inventory)
			inventory.GetItems(found);

		// GetItems can list one entity twice (seen on the island: a juice stack counted double), which
		// doubled the "Sell all" quote; each entity counts once.
		array<IEntity> items = {};
		foreach (IEntity item : found)
		{
			if (item && items.Find(item) < 0)
				items.Insert(item);
		}

		return items;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the prices for the trader's sell rows (LF_TraderSellAction). One entry per good the seller
	//! carries and the trader buys: the price of the one unit a "Sell 1" row takes, and of all of them.
	//! \return false when the seller may not trade here (out of reach, down, not a trader).
	static bool Quote(IEntity seller, IEntity traderOwner, notnull array<string> goods, notnull array<int> onePrices, notnull array<int> allPrices)
	{
		goods.Clear();
		onePrices.Clear();
		allPrices.Clear();
		if (!seller || !traderOwner || !LF_Utils.IsAliveState(seller) || !InReach(seller.GetOrigin(), traderOwner.GetOrigin()))
			return false;

		LF_TraderManagerComponent trader = LF_Component<LF_TraderManagerComponent>.Find(traderOwner);
		if (!trader)
			return false;

		array<ref LF_TraderSellGood> rows = LF_TraderSellRows.GroupEntities(trader.m_aTradableItems, CarriedItems(seller));
		int nowUnix = System.GetUnixTime();
		foreach (LF_TraderSellGood row : rows)
		{
			goods.Insert(row.m_sPrefab);
			onePrices.Insert(QuoteUnits(trader, row.m_sPrefab, 1, nowUnix));
			allPrices.Insert(QuoteUnits(trader, row.m_sPrefab, row.m_iUnits, nowUnix));
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sells every item the seller carries that this trader buys, each through Sell (same checks,
	//! same prices, same rollback), then one toast with the total instead of one per item.
	//! \return The cash paid in all; 0 when nothing sold (the seller then gets the refusal toast).
	static int SellAll(IEntity seller, IEntity traderOwner, bool untrusted)
	{
		if (!Replication.IsServer() || !seller || !traderOwner)
			return 0;

		LF_TraderManagerComponent trader = LF_Component<LF_TraderManagerComponent>.Find(traderOwner);
		if (!trader)
		{
			Refuse(seller, LF_ETraderSale.NO_TRADER, "sell all: not a trader");
			return 0;
		}

		array<IEntity> wanted = {};
		foreach (IEntity item : CarriedItems(seller))
		{
			if (item && LF_TraderManagerComponent.GetUnitValue(trader.m_aTradableItems, LF_Utils.GetPrefabName(item)) > 0)
				wanted.Insert(item);
		}

		s_bQuiet = true;
		s_iBatchPaid = 0;
		int sold = 0;
		foreach (IEntity good : wanted)
		{
			if (Sell(seller, good, traderOwner, untrusted) == LF_ETraderSale.SOLD)
				sold++;
		}

		s_bQuiet = false;
		int paid = s_iBatchPaid;
		s_iBatchPaid = 0;
		if (sold == 0 || paid <= 0)
		{
			Refuse(seller, LF_ETraderSale.NOT_BOUGHT, string.Format("sell all: nothing sold (%1 offered)", wanted.Count()));
			return 0;
		}

		LF_Debug.Info("Trader", string.Format("sell all: %1 of %2 items for %3", sold, wanted.Count(), paid));
		LF_Utils.NotifyEntity(seller, "#EL-Item_Sold", "#EL-Trade_Successful", 3.0, paid.ToString());
		return paid;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: while true, every sale fails at the item removal step (proves the rollback).
	static void FailRemoveForTest(bool fail)
	{
		s_bFailRemoveForTest = fail;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ETraderSale Refuse(IEntity seller, LF_ETraderSale why, string detail)
	{
		LF_Debug.Warn("Trader", string.Format("sale refused (%1): %2", typename.EnumToString(LF_ETraderSale, why), detail));
		if (seller)
			Tell(seller, "#LF-Trader_SaleRefused", "#LF-Trader_SaleRefused_Title", 3.0);
		return why;
	}

	//------------------------------------------------------------------------------------------------
	//! Stack size of the sold entity; 1 for items without a quantity component.
	protected static int GetItemQuantity(IEntity item)
	{
		LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(item);
		if (!quantity)
			return 1;
		return quantity.GetQuantity();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PlayerAccount GetPlayerAccount(IEntity user)
	{
		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return null;

		return accountManager.GetAccount(LF_Utils.GetPlayerUid(user));
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_TraderSale's statics (LF_StaticReset finds it by reflection).
class LF_TraderSale_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_TraderSale";
	}

	override void Apply()
	{
		LF_TraderSale.FailRemoveForTest(false);
		LF_TraderQuoteLive.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The request path. A client owns its player controller for the whole session, so the sale
//! request rides it; the server answers by moving money and the item (replicated state) and by a
//! toast to this player only.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to sell `item` to the trader that owns the target storage.
	//! The listen host resolves it directly.
	void LF_RequestTraderSale(IEntity item, IEntity traderOwner)
	{
		RplId itemId = LF_NetworkUtils.GetRplId(item);
		RplId traderId = LF_NetworkUtils.GetRplId(traderOwner);
		if (!itemId.IsValid() || !traderId.IsValid())
		{
			LF_Debug.Warn("Trader", "sale request dropped: the item or the trader has no replication id");
			return;
		}

		if (Replication.IsServer())
		{
			RpcAsk_LF_TraderSale(itemId, traderId);
			return;
		}

		Rpc(RpcAsk_LF_TraderSale, itemId, traderId);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: asks the server for the prices of the trader's sell rows (LF_TraderQuoteLive keeps them).
	void LF_RequestTraderQuote(IEntity traderOwner)
	{
		RplId traderId = LF_NetworkUtils.GetRplId(traderOwner);
		if (!traderId.IsValid())
		{
			LF_Debug.Warn("Trader", "quote request dropped: the trader has no replication id");
			return;
		}

		Rpc(RpcAsk_LF_TraderQuote, traderId);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a "Sell 1" row: one unit of the carried item (a stack shrinks by one).
	void LF_RequestTraderSellOne(IEntity item, IEntity traderOwner)
	{
		RplId itemId = LF_NetworkUtils.GetRplId(item);
		RplId traderId = LF_NetworkUtils.GetRplId(traderOwner);
		if (!itemId.IsValid() || !traderId.IsValid())
		{
			LF_Debug.Warn("Trader", "sell-one request dropped: the item or the trader has no replication id");
			return;
		}

		Rpc(RpcAsk_LF_TraderSellOne, itemId, traderId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one unit, priced and checked like any client sale.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_TraderSellOne(RplId itemId, RplId traderId)
	{
		if (!LF_RateLimit.Allow(GetPlayerId(), LF_ERequestKind.PURCHASE, "trader sell one"))
			return;

		LF_TraderSale.Sell(GetControlledEntity(), LF_NetworkUtils.FindEntityByRplId(itemId), LF_NetworkUtils.FindEntityByRplId(traderId), true, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the "Sell all" row. The server finds the goods itself; the request names only the trader.
	void LF_RequestTraderSellAll(IEntity traderOwner)
	{
		RplId traderId = LF_NetworkUtils.GetRplId(traderOwner);
		if (!traderId.IsValid())
		{
			LF_Debug.Warn("Trader", "sell-all request dropped: the trader has no replication id");
			return;
		}

		Rpc(RpcAsk_LF_TraderSellAll, traderId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: answers a quote for this controller's own character only.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_TraderQuote(RplId traderId)
	{
		if (!LF_RateLimit.Allow(GetPlayerId(), LF_ERequestKind.INVENTORY, "trader quote"))
			return;

		SendTraderQuote(traderId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: builds and sends the sell-row prices to this controller's owner.
	protected void SendTraderQuote(RplId traderId)
	{
		array<string> goods = {};
		array<int> onePrices = {};
		array<int> allPrices = {};
		LF_TraderSale.Quote(GetControlledEntity(), LF_NetworkUtils.FindEntityByRplId(traderId), goods, onePrices, allPrices);
		Rpc(RpcDo_LF_TraderQuote, traderId, goods, onePrices, allPrices);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_TraderQuote(RplId traderId, array<string> goods, array<int> onePrices, array<int> allPrices)
	{
		LF_TraderQuoteLive.OnReply(traderId, goods, onePrices, allPrices);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the "Sell all" row. The client asks for fresh prices once its inventory changes.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_TraderSellAll(RplId traderId)
	{
		if (!LF_RateLimit.Allow(GetPlayerId(), LF_ERequestKind.PURCHASE, "trader sell all"))
			return;

		LF_TraderSale.SellAll(GetControlledEntity(), LF_NetworkUtils.FindEntityByRplId(traderId), true);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the seller is always this controller's own character, never a payload field.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_TraderSale(RplId itemId, RplId traderId)
	{
		if (!LF_RateLimit.Allow(GetPlayerId(), LF_ERequestKind.PURCHASE, "trader sale"))
			return;

		LF_TraderSale.Sell(GetControlledEntity(), LF_NetworkUtils.FindEntityByRplId(itemId), LF_NetworkUtils.FindEntityByRplId(traderId), true);
	}
}
