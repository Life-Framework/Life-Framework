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
	NOT_REMOVED
}

//------------------------------------------------------------------------------------------------
//! The one place a trader sale resolves, and it resolves on the server. The client's inventory
//! drag only forwards a request (SCR_PlayerController.LF_RequestTraderSale); money and items
//! change here. Pay before deleting: a sale that cannot pay out never consumes the item.
class LF_TraderSale
{
	//! How far the seller may stand from the trader's storage for a client request.
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
	//! \return SOLD, or why the sale was refused; a refusal changes nothing.
	static LF_ETraderSale Sell(IEntity seller, IEntity item, IEntity traderOwner, bool untrusted)
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
		int outputUnits = LF_ShopRules.StackTradeOutputCount(GetItemQuantity(item));
		int totalValue = LF_ShopRules.StackTradeTotal(unitValue, outputUnits);

		int paid = LF_MoneyUtils.AddCash(seller, totalValue, "trader-sell");
		if (paid != totalValue)
		{
			if (paid > 0)
				LF_MoneyUtils.RemoveCash(seller, paid, "trader-sell-rollback");
			return Refuse(seller, LF_ETraderSale.UNPAYABLE, string.Format("cash payout failed (%1 of %2)", paid, totalValue));
		}

		if (!inventory.TryDeleteItem(item))
		{
			LF_MoneyUtils.RemoveCash(seller, paid, "trader-sell-rollback");
			return Refuse(seller, LF_ETraderSale.NOT_REMOVED, "the item could not be removed");
		}

		LF_Debug.Info("Trader", string.Format("sold %1 for %2 (untrusted=%3)", itemPrefab, totalValue, untrusted));
		LF_Utils.NotifyEntity(seller, "#EL-Item_Sold", "#EL-Trade_Successful", 3.0, totalValue.ToString());
		return LF_ETraderSale.SOLD;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ETraderSale Refuse(IEntity seller, LF_ETraderSale why, string detail)
	{
		LF_Debug.Warn("Trader", string.Format("sale refused (%1): %2", typename.EnumToString(LF_ETraderSale, why), detail));
		if (seller)
			LF_Utils.NotifyEntity(seller, "#LF-Trader_SaleRefused", "#LF-Trader_SaleRefused_Title", 3.0);
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
	//! Server: the seller is always this controller's own character, never a payload field.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_TraderSale(RplId itemId, RplId traderId)
	{
		LF_TraderSale.Sell(GetControlledEntity(), LF_NetworkUtils.FindEntityByRplId(itemId), LF_NetworkUtils.FindEntityByRplId(traderId), true);
	}
}
