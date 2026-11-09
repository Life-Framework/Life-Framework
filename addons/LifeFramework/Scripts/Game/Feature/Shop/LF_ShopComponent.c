[ComponentEditorProps(category: "EveronLife/Shop", description: "Component for shop entities that buy and sell items")]
class LF_ShopComponentClass : ScriptComponentClass
{
}

class LF_ShopComponent : ScriptComponent
{
	[Attribute("Shop", UIWidgets.Auto, desc: "Name of the shop")]
	protected string m_sShopName;

	[Attribute("", UIWidgets.Auto, desc: "Description of the shop")]
	protected string m_sShopDescription;

	//! Optional curated allowlist. Empty means the shop sells every priced item in the catalog.
	[Attribute("", UIWidgets.Object, desc: "Optional curated item allowlist; empty sells every priced item")]
	protected ref array<ref LF_ShopItem> m_aShopItems;

	//! A fence: sells only to proven criminals. Each item's threshold is its LF_ShopItem.m_iMinCrimes,
	//! read against the buyer's hidden crime record on the server. Every sale publishes LF_OnFenceSale.
	[Attribute("0", UIWidgets.CheckBox, desc: "Fence: refuse buyers whose crime record is below the item's threshold; publish LF_OnFenceSale")]
	protected bool m_bFence;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!m_aShopItems)
			m_aShopItems = new array<ref LF_ShopItem>();
	}

	//------------------------------------------------------------------------------------------------
	string GetShopName()
	{
		return m_sShopName;
	}

	//------------------------------------------------------------------------------------------------
	string GetShopDescription()
	{
		return m_sShopDescription;
	}

	//------------------------------------------------------------------------------------------------
	//! The items this shop shows: its curated list when one is set, else every priced catalog item.
	//! Prices always come from the price config via the manager, never from the curated list.
	array<ref LF_ShopCatalogItem> GetShopItems()
	{
		array<ref LF_ShopCatalogItem> result = new array<ref LF_ShopCatalogItem>();

		if (m_aShopItems && !m_aShopItems.IsEmpty())
		{
			foreach (LF_ShopItem curated : m_aShopItems)
			{
				if (!curated)
					continue;
				LF_ShopCatalogItem item = BuildCatalogItem(curated.GetItemPrefab());
				if (item)
				{
					// An unset curated name keeps the catalog's UIInfo name instead of a blank row.
					if (!curated.GetDisplayName().IsEmpty())
						item.m_sDisplayName = curated.GetDisplayName();
					result.Insert(item);
				}
			}

			return result;
		}

		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		if (!manager)
			return result;

		foreach (LF_ShopCatalogItem item : manager.GetCatalogItems())
		{
			if (!item || item.m_bHidden)
				continue;
			result.Insert(item);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether one prefab is offered by this shop at all (the server-side gate for buy and sell).
	bool IsItemSoldHere(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return false;

		if (m_aShopItems && !m_aShopItems.IsEmpty())
		{
			foreach (LF_ShopItem curated : m_aShopItems)
			{
				if (curated && curated.GetItemPrefab() == prefab)
					return true;
			}

			return false;
		}

		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		if (!manager)
			return false;

		return !manager.IsHidden(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Buy price this shop charges for one prefab. Zero when the item is not priced.
	int GetBuyPrice(ResourceName prefab)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		if (manager)
		{
			int buyPrice;
			int sellPrice;
			bool hidden;
			if (manager.GetPrices(prefab, buyPrice, sellPrice, hidden))
				return buyPrice;
		}

		LF_ShopItem curated = FindCuratedItem(prefab);
		if (curated)
			return curated.GetPrice();

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Sell price this shop pays for one prefab. Zero when the item is not priced.
	int GetSellPrice(ResourceName prefab)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		if (manager)
		{
			int buyPrice;
			int sellPrice;
			bool hidden;
			if (manager.GetPrices(prefab, buyPrice, sellPrice, hidden))
				return sellPrice;
		}

		LF_ShopItem curated = FindCuratedItem(prefab);
		if (curated)
			return curated.GetPrice() / 2;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	bool IsFence()
	{
		return m_bFence;
	}

	//------------------------------------------------------------------------------------------------
	//! Completed crimes a buyer needs before this shop sells the prefab. 0 for a plain shop.
	int GetMinCrimes(ResourceName prefab)
	{
		if (!m_bFence)
			return 0;

		LF_ShopItem curated = FindCuratedItem(prefab);
		if (!curated)
			return 0;

		return curated.GetMinCrimes();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a requested quantity is within the item's buy limits.
	static bool IsQuantityAllowed(int maxQuantity, int quantity)
	{
		if (quantity <= 0)
			return false;
		return quantity <= maxQuantity;
	}

	//------------------------------------------------------------------------------------------------
	//! Total price of a purchase. A stack pays or costs one unit per quantity.
	static int ComputeTotalPrice(int unitPrice, int quantity)
	{
		return LF_ShopRules.StackTradeTotal(unitPrice, quantity);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: \see Buy. \return True when the whole order was paid for and delivered.
	bool BuyItem(ResourceName prefab, int quantity, IEntity buyer)
	{
		int charged;
		return Buy(prefab, quantity, buyer, charged) == LF_EShopRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: charge the buyer the config buy price and deliver the items. Only what was delivered
	//! is charged; a partial delivery refunds the difference. The till slip shows the outcome, so
	//! this raises no toast.
	//! \param[out] charged Cash taken for the order (0 when refused).
	//! \return NONE when the whole order was paid for and delivered, else why not.
	LF_EShopRefusal Buy(ResourceName prefab, int quantity, IEntity buyer, out int charged)
	{
		charged = 0;
		if (!Replication.IsServer() || !buyer)
			return LF_EShopRefusal.NO_SHOP;

		if (quantity <= 0)
			return LF_EShopRefusal.BAD_QUANTITY;

		if (!IsItemSoldHere(prefab))
		{
			LF_Debug.Info("Shop", string.Format("buy rejected (not sold here) item=%1 qty=%2", prefab, quantity));
			return LF_EShopRefusal.NOT_SOLD_HERE;
		}

		int unitPrice = GetBuyPrice(prefab);
		if (unitPrice <= 0)
		{
			LF_Debug.Info("Shop", string.Format("buy rejected (unpriced) item=%1", prefab));
			return LF_EShopRefusal.UNPRICED;
		}

		string buyerUid;
		if (m_bFence)
		{
			buyerUid = LF_Utils.GetPlayerUid(buyer);
			LF_EShopRefusal trust = LF_FenceRules.Check(buyerUid, GetMinCrimes(prefab), prefab);
			if (trust != LF_EShopRefusal.NONE)
				return trust;
		}

		int units = LF_ShopRules.StackTradeOutputCount(quantity);
		int totalPrice = LF_ShopRules.StackTradeTotal(unitPrice, units);

		int removed = LF_MoneyUtils.RemoveAmount(buyer, totalPrice, "shop-buy");
		if (removed != totalPrice)
		{
			if (removed > 0)
				LF_MoneyUtils.AddAmount(buyer, removed, "shop-buy-rollback");
			LF_Debug.Info("Shop", string.Format("buy failed (insufficient funds) item=%1 qty=%2 price=%3 removed=%4", prefab, quantity, totalPrice, removed));
			return LF_EShopRefusal.NOT_ENOUGH_CASH;
		}

		int added = LF_InventoryUtils.AddAmount(buyer, prefab, units);
		if (added == units)
		{
			charged = totalPrice;
			LF_Debug.Info("Shop", string.Format("bought item=%1 qty=%2 price=%3", prefab, units, totalPrice));
			if (m_bFence)
				LF_EventBus.Publish(LF_OnFenceSale.Create(buyerUid, prefab, units, totalPrice));
			return LF_EShopRefusal.NONE;
		}

		int notDelivered = units - added;
		if (notDelivered > 0)
			LF_MoneyUtils.AddAmount(buyer, LF_ShopRules.StackTradeTotal(unitPrice, notDelivered), "shop-buy-rollback");
		LF_Debug.Warn("Shop", string.Format("buy failed (inventory full) item=%1 qty=%2 delivered=%3 - refunded %4", prefab, quantity, added, notDelivered));
		return LF_EShopRefusal.POCKETS_FULL;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: \see Sell. \return True when the sale went through.
	bool SellItem(ResourceName prefab, int quantity, IEntity seller)
	{
		int earned;
		return Sell(prefab, quantity, seller, earned) == LF_EShopRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take the items from the seller and pay the config sell price. Cash is paid before the
	//! items are consumed, so a sale that cannot pay out never loses the goods. No toast; the slip
	//! shows the outcome.
	//! \param[out] earned Cash paid for what was taken (0 when refused).
	//! \return NONE when at least one unit was sold, else why not.
	LF_EShopRefusal Sell(ResourceName prefab, int quantity, IEntity seller, out int earned)
	{
		earned = 0;
		if (!Replication.IsServer() || !seller)
			return LF_EShopRefusal.NO_SHOP;

		if (quantity <= 0)
			return LF_EShopRefusal.BAD_QUANTITY;

		if (!IsItemSoldHere(prefab))
		{
			LF_Debug.Info("Shop", string.Format("sell rejected (not bought here) item=%1 qty=%2", prefab, quantity));
			return LF_EShopRefusal.NOT_SOLD_HERE;
		}

		int unitPrice = GetSellPrice(prefab);
		if (unitPrice <= 0)
		{
			LF_Debug.Info("Shop", string.Format("sell rejected (unpriced) item=%1", prefab));
			return LF_EShopRefusal.UNPRICED;
		}

		int units = LF_ShopRules.StackTradeOutputCount(quantity);
		int totalPrice = LF_ShopRules.StackTradeTotal(unitPrice, units);

		int paid = LF_MoneyUtils.AddCash(seller, totalPrice, "shop-sell");
		if (paid != totalPrice)
		{
			if (paid > 0)
				LF_MoneyUtils.RemoveCash(seller, paid, "shop-sell-rollback");
			LF_Debug.Warn("Shop", string.Format("sell failed (cannot pay out) item=%1 qty=%2 price=%3 paid=%4", prefab, quantity, totalPrice, paid));
			return LF_EShopRefusal.POCKETS_FULL;
		}

		int removed = LF_InventoryUtils.RemoveAmount(seller, prefab, units);
		if (removed <= 0)
		{
			LF_MoneyUtils.RemoveCash(seller, paid, "shop-sell-rollback");
			LF_Debug.Info("Shop", string.Format("sell failed (nothing to sell) item=%1 qty=%2", prefab, units));
			return LF_EShopRefusal.NOTHING_TO_SELL;
		}

		int overpaid = LF_ShopRules.StackTradeTotal(unitPrice, units - removed);
		if (overpaid > 0)
			LF_MoneyUtils.RemoveCash(seller, overpaid, "shop-sell-rollback");

		earned = LF_ShopRules.StackTradeTotal(unitPrice, removed);
		LF_Debug.Info("Shop", string.Format("sold item=%1 qty=%2 price=%3", prefab, removed, earned));
		return LF_EShopRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Best available display name for an item (a #key or a plain name).
	string GetItemDisplayName(ResourceName prefab)
	{
		LF_ShopCatalogItem item = BuildCatalogItem(prefab);
		if (item && !item.m_sDisplayName.IsEmpty())
			return item.m_sDisplayName;

		LF_ShopItem curated = FindCuratedItem(prefab);
		if (curated && !curated.GetDisplayName().IsEmpty())
			return curated.GetDisplayName();

		return LF_ShopManagerComponent.ResolveDisplayName(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! The catalog entry for a prefab, carrying the config-resolved prices.
	protected LF_ShopCatalogItem BuildCatalogItem(ResourceName prefab)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		if (!manager)
			return null;

		LF_ShopCatalogItem source = manager.GetItem(prefab);
		if (source)
			return source;

		LF_ShopCatalogItem item = new LF_ShopCatalogItem();
		item.m_ItemPrefab = prefab;
		item.m_sDisplayName = LF_ShopManagerComponent.ResolveDisplayName(prefab);

		int buyPrice;
		int sellPrice;
		bool hidden;
		if (manager.GetPrices(prefab, buyPrice, sellPrice, hidden))
		{
			item.m_iBuyPrice = buyPrice;
			item.m_iSellPrice = sellPrice;
			item.m_bHidden = hidden;
		}

		return item;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ShopItem FindCuratedItem(ResourceName prefab)
	{
		if (!m_aShopItems)
			return null;

		foreach (LF_ShopItem curated : m_aShopItems)
		{
			if (curated && curated.GetItemPrefab() == prefab)
				return curated;
		}

		return null;
	}
};
