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

	//! A back room (#600): its stock is contraband. An on-duty officer may search it and seize the
	//! stock as evidence (LF_BackRoomSearchAction); the room then stays shut for a while.
	[Attribute("0", UIWidgets.CheckBox, desc: "Back room: an on-duty officer can search it and seize its stock as evidence")]
	protected bool m_bBackRoom;

	//! A line the shopkeeper adds to this shop's sheet: a localization key, or empty. The general
	//! store uses it to point a buyer at the back room (#933).
	[Attribute("", UIWidgets.Auto, desc: "Shopkeeper's word on the sheet: a localization key (such as a pointer to the back room); empty shows nothing")]
	protected string m_sShopHint;

	//! An outlet of the item catalogue (#988). When set, the shop sells exactly that outlet's offers at
	//! the catalogue's prices behind each offer's gate, and the curated list and price rules are not read.
	[Attribute("", UIWidgets.Auto, desc: "Item catalogue outlet id (gun_counter, clothing_rack, ...). Set, the shop sells that outlet's offers at catalogue prices behind their gates")]
	protected string m_sOutletId;

	//! A stock of LF_ShopStockConfig (core mechanism, pack data). When set, the shop sells that stock
	//! at its prices and buys back what its buyback takes; the curated list, outlet and price rules
	//! are not read for what it sells.
	[Attribute("", UIWidgets.Auto, desc: "Stock id in LF_ShopStockConfig (fence, ...). Set, the shop sells that stock at its prices and buys back per its rules")]
	protected string m_sStockId;

	//! Server only, world time in ms. Not saved: a raid closes the room for the rest of the session at most.
	protected int m_iClosedUntilMs;

	//! The shutter as clients see it, so the door prompt can say "shut" before anyone walks up and
	//! knocks. The server's m_iClosedUntilMs stays the authority; this follows it.
	[RplProp()]
	protected bool m_bShutShown;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!m_aShopItems)
			m_aShopItems = new array<ref LF_ShopItem>();
	}

	//------------------------------------------------------------------------------------------------
	bool IsBackRoom()
	{
		return m_bBackRoom;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether a raid has shut this room and the shutter is still down.
	bool IsClosed(int nowMs)
	{
		return LF_BackRoomRules.IsClosed(m_iClosedUntilMs, nowMs);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an officer's search. Logs each curated stock line in the evidence locker (existing
	//! locker payload, kind "backroom") and shuts the room.
	//! Returns NONE, or why it was refused.
	LF_EBackRoomRefusal Raid(IEntity officer, bool onDuty, int nowMs, out int seized)
	{
		seized = 0;
		if (!Replication.IsServer() || !officer)
			return LF_EBackRoomRefusal.NOT_ALLOWED;

		float distance = vector.Distance(officer.GetOrigin(), GetOwner().GetOrigin());
		LF_EBackRoomRefusal refusal = LF_BackRoomRules.Check(onDuty, m_bBackRoom, distance, m_iClosedUntilMs, nowMs);
		if (refusal != LF_EBackRoomRefusal.NONE)
		{
			LF_Debug.Warn("Shop", string.Format("back room search refused: %1", typename.EnumToString(LF_EBackRoomRefusal, refusal)));
			return refusal;
		}

		string officerUid = LF_Utils.GetPlayerUID(officer);
		foreach (LF_ShopItem curated : m_aShopItems)
		{
			if (!curated)
				continue;

			LF_EvidenceLocker.LogSeizure(curated.GetItemPrefab(), m_sShopName, officerUid);
			seized++;
		}

		LF_ShopStock raided = GetStock();
		if (raided && raided.m_aItems)
		{
			foreach (LF_ShopStockItem line : raided.m_aItems)
			{
				if (!line)
					continue;

				LF_EvidenceLocker.LogSeizure(line.m_Prefab, m_sShopName, officerUid);
				seized++;
			}
		}

		m_iClosedUntilMs = LF_BackRoomRules.ClosedUntil(nowMs);
		SetShutShown(true);
		GetGame().GetCallqueue().CallLater(OpenShutter, LF_ShopLimitsConfig.BackRoomShutMs(), false);
		LF_Debug.Info("Shop", string.Format("back room '%1' searched by %2: %3 stock line(s) seized, shut until %4", m_sShopName, officerUid, seized, m_iClosedUntilMs));
		return LF_EBackRoomRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Client and server: whether the room's shutter is down (a raid shut it and the time has not passed).
	bool IsShutShown()
	{
		return m_bShutShown;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetShutShown(bool shut)
	{
		if (m_bShutShown == shut)
			return;

		m_bShutShown = shut;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the shut time has passed.
	protected void OpenShutter()
	{
		SetShutShown(false);
		LF_Debug.Info("Shop", string.Format("back room '%1' open again", m_sShopName));
	}

	//------------------------------------------------------------------------------------------------
	string GetShopHint()
	{
		return m_sShopHint;
	}

	//------------------------------------------------------------------------------------------------
	string GetShopName()
	{
		return m_sShopName;
	}

	//------------------------------------------------------------------------------------------------
	//! The item catalogue outlet this shop is, or empty for a price-rule shop.
	string GetOutletId()
	{
		return m_sOutletId;
	}

	//------------------------------------------------------------------------------------------------
	string GetStockId()
	{
		return m_sStockId;
	}

	//------------------------------------------------------------------------------------------------
	//! The stock this shop sells, or null for a shop with none (or one the config lacks, logged).
	LF_ShopStock GetStock()
	{
		if (m_sStockId.IsEmpty())
			return null;

		LF_ShopStock stock = LF_ShopStockConfig.Get().Find(m_sStockId);
		if (!stock)
			LF_Debug.Warn(LF_ShopStockRules.FEATURE, string.Format("shop '%1' names stock '%2', which no stock config has; it sells nothing", m_sShopName, m_sStockId));

		return stock;
	}

	//------------------------------------------------------------------------------------------------
	//! Price-table buy price of a prefab (what it costs new): the base a buyback pays a share of.
	static int StreetValue(ResourceName prefab)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		if (!manager)
			return 0;

		int buyPrice;
		int sellPrice;
		bool hidden;
		if (!manager.GetPrices(prefab, buyPrice, sellPrice, hidden) || hidden)
			return 0;

		return buyPrice;
	}

	//------------------------------------------------------------------------------------------------
	//! The stocked shop's rows: what it sells at stock prices, then what its buyback list takes
	//! (no buy price, the buyback price as the sell price).
	protected void AddStockRows(LF_ShopStock stock, notnull array<ref LF_ShopCatalogItem> result)
	{
		if (stock.m_aItems)
		{
			foreach (LF_ShopStockItem line : stock.m_aItems)
			{
				if (!line || line.m_Prefab.IsEmpty())
					continue;

				LF_ShopCatalogItem row = new LF_ShopCatalogItem();
				row.m_ItemPrefab = line.m_Prefab;
				row.m_sDisplayName = line.m_sDisplayName;
				if (row.m_sDisplayName.IsEmpty())
					row.m_sDisplayName = LF_ShopManagerComponent.ResolveDisplayName(line.m_Prefab);
				row.m_iBuyPrice = line.m_iPrice;
				result.Insert(row);
			}
		}

		if (!stock.m_Buyback || !stock.m_Buyback.m_aPrefabs)
			return;

		foreach (ResourceName wanted : stock.m_Buyback.m_aPrefabs)
		{
			LF_ShopCatalogItem buyback = new LF_ShopCatalogItem();
			buyback.m_ItemPrefab = wanted;
			buyback.m_sDisplayName = LF_ShopManagerComponent.ResolveDisplayName(wanted);
			buyback.m_iSellPrice = LF_ShopStockRules.BuybackPrice(StreetValue(wanted), stock.m_Buyback.m_fFraction);
			result.Insert(buyback);
		}
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

		if (!m_sStockId.IsEmpty())
		{
			LF_ShopStock stock = GetStock();
			if (stock)
				AddStockRows(stock, result);

			return result;
		}

		if (!m_sOutletId.IsEmpty())
		{
			// An outlet is the catalogue's counter: with Catalogue off it shows nothing (#1074).
			if (!LF_Systems.IsEnabled(LF_ItemCatalogue.SYSTEM))
				return result;

			LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
			array<LF_OutletOffer> offers = {};
			LF_ItemCatalogue.Offers(catalogue, m_sOutletId, offers);
			foreach (LF_OutletOffer offer : offers)
			{
				LF_CatalogueItem entry = LF_ItemCatalogue.Item(catalogue, offer.m_sItem);
				LF_ShopCatalogItem row = new LF_ShopCatalogItem();
				row.m_ItemPrefab = entry.m_Prefab;
				row.m_sDisplayName = LF_ShopManagerComponent.ResolveDisplayName(entry.m_Prefab);
				row.m_iBuyPrice = entry.m_iPrice;
				row.m_iSellPrice = entry.m_iSellPrice;
				result.Insert(row);
			}

			return result;
		}

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

		if (!m_sStockId.IsEmpty())
		{
			LF_ShopStock stock = GetStock();
			if (!stock)
				return false;

			return stock.FindItem(prefab) != null || stock.ListsBuyback(prefab);
		}

		if (!m_sOutletId.IsEmpty())
		{
			LF_CatalogueItem entry;
			return LF_ItemCatalogue.OfferFor(LF_ItemCatalogueConfig.Get(), m_sOutletId, prefab, entry) != null;
		}

		if (m_aShopItems && !m_aShopItems.IsEmpty())
		{
			foreach (LF_ShopItem curated : m_aShopItems)
			{
				if (curated && LF_Utils.SamePrefab(curated.GetItemPrefab(), prefab))
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
		if (!m_sStockId.IsEmpty())
		{
			LF_ShopStock stock = GetStock();
			if (!stock)
				return 0;

			LF_ShopStockItem line = stock.FindItem(prefab);
			if (!line)
				return 0;

			return line.m_iPrice;
		}

		if (!m_sOutletId.IsEmpty())
		{
			LF_CatalogueItem entry;
			if (LF_ItemCatalogue.OfferFor(LF_ItemCatalogueConfig.Get(), m_sOutletId, prefab, entry))
				return entry.m_iPrice;

			return 0;
		}

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
		// A stocked shop pays only through its buyback: a share of the street value, for listed goods
		// and (when it takes them) flagged ones. Its own stock is not bought back.
		if (!m_sStockId.IsEmpty())
		{
			LF_ShopStock stock = GetStock();
			if (!stock || !stock.m_Buyback)
				return 0;

			if (!stock.ListsBuyback(prefab) && !stock.m_Buyback.m_bStolen)
				return 0;

			return LF_ShopStockRules.BuybackPrice(StreetValue(prefab), stock.m_Buyback.m_fFraction);
		}

		if (!m_sOutletId.IsEmpty())
		{
			LF_CatalogueItem entry;
			if (LF_ItemCatalogue.OfferFor(LF_ItemCatalogueConfig.Get(), m_sOutletId, prefab, entry))
				return entry.m_iSellPrice;

			return 0;
		}

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

		LF_ShopStock stock = GetStock();
		if (stock)
		{
			LF_ShopStockItem line = stock.FindItem(prefab);
			if (line)
				return line.m_iMinCrimes;

			return 0;
		}

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
		string missing;
		return Buy(prefab, quantity, buyer, charged, missing) == LF_EShopRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: charge the buyer the config buy price and deliver the items. Only what was delivered
	//! is charged; a partial delivery refunds the difference. The till slip shows the outcome, so
	//! this raises no toast.
	//! \param[out] charged Cash taken for the order (0 when refused).
	//! \param[out] missing What an outlet's gate found the buyer lacks, a localization key; else empty.
	//! \return NONE when the whole order was paid for and delivered, else why not.
	LF_EShopRefusal Buy(ResourceName prefab, int quantity, IEntity buyer, out int charged, out string missing)
	{
		charged = 0;
		missing = string.Empty;
		if (!Replication.IsServer() || !buyer)
			return LF_EShopRefusal.NO_SHOP;

		if (LF_Systems.Refuses(LF_ShopRefusal.SYSTEM, "buy at " + m_sOutletId))
			return LF_EShopRefusal.SYSTEM_OFF;

		if (quantity <= 0)
			return LF_EShopRefusal.BAD_QUANTITY;

		if (IsClosed(GetGame().GetWorld().GetWorldTime()))
		{
			LF_Debug.Info("Shop", string.Format("buy rejected (raided, shut) item=%1", prefab));
			return LF_EShopRefusal.CLOSED;
		}

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

		LF_EShopRefusal gate = CheckOutletGate(prefab, buyer, missing);
		if (gate != LF_EShopRefusal.NONE)
			return gate;

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

		// A physical store (#202) holds a limited stock. A shelf copy taken by hand is counted before
		// the check, and the units are set aside only once the cash is in, so a refused sale never
		// takes a copy off the shelf and a pickup and a sale never both count one unit.
		LF_StoreComponent store = LF_Component<LF_StoreComponent>.Find(GetOwner());
		if (store && !store.CanSupply(prefab, units))
			return LF_EShopRefusal.OUT_OF_STOCK;

		int removed = LF_MoneyUtils.RemoveAmount(buyer, totalPrice, "shop-buy");
		if (removed != totalPrice)
		{
			if (removed > 0)
				LF_MoneyUtils.AddAmount(buyer, removed, "shop-buy-rollback");
			LF_Debug.Info("Shop", string.Format("buy failed (insufficient funds) item=%1 qty=%2 price=%3 removed=%4", prefab, quantity, totalPrice, removed));
			return LF_EShopRefusal.NOT_ENOUGH_CASH;
		}

		if (store && !store.Reserve(prefab, units))
		{
			LF_MoneyUtils.AddAmount(buyer, removed, "shop-buy-rollback");
			return LF_EShopRefusal.OUT_OF_STOCK;
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
		if (store && notDelivered > 0)
			store.Unreserve(prefab, notDelivered);
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
		string missing;
		return Sell(prefab, quantity, seller, earned, missing) == LF_EShopRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take the items from the seller and pay the config sell price. Cash is paid before the
	//! items are consumed, so a sale that cannot pay out never loses the goods. No toast; the slip
	//! shows the outcome.
	//! \param[out] earned Cash paid for what was taken (0 when refused).
	//! \return NONE when at least one unit was sold, else why not.
	LF_EShopRefusal Sell(ResourceName prefab, int quantity, IEntity seller, out int earned, out string missing)
	{
		earned = 0;
		missing = string.Empty;
		if (!Replication.IsServer() || !seller)
			return LF_EShopRefusal.NO_SHOP;

		if (LF_Systems.Refuses(LF_ShopRefusal.SYSTEM, "sell at " + m_sOutletId))
			return LF_EShopRefusal.SYSTEM_OFF;

		if (quantity <= 0)
			return LF_EShopRefusal.BAD_QUANTITY;

		if (IsClosed(GetGame().GetWorld().GetWorldTime()))
		{
			LF_Debug.Info("Shop", string.Format("sell rejected (raided, shut) item=%1", prefab));
			return LF_EShopRefusal.CLOSED;
		}

		// A stocked shop's buyback: listed goods go the usual way; anything else it takes only as
		// flagged stolen copies, and only those copies are taken off the seller.
		LF_ShopStock stock = GetStock();
		array<IEntity> flagged = {};
		bool flaggedOnly = false;
		if (stock && !stock.ListsBuyback(prefab))
		{
			LF_StolenGoods.Carried(seller, prefab, flagged);
			if (!LF_ShopStockRules.Buys(stock, prefab, !flagged.IsEmpty()))
			{
				LF_Debug.Info(LF_ShopStockRules.FEATURE, string.Format("sell rejected (stock '%1' does not buy it, flagged copies %2) item=%3", m_sStockId, flagged.Count(), prefab));
				return LF_EShopRefusal.NOT_SOLD_HERE;
			}

			flaggedOnly = true;
		}
		else if (!IsItemSoldHere(prefab))
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

		// An outlet trades only with whoever its gate lets through, both ways (#988).
		LF_EShopRefusal gate = CheckOutletGate(prefab, seller, missing);
		if (gate != LF_EShopRefusal.NONE)
			return gate;

		// Goods taken off a store's shelf and not paid for are not the seller's to sell: selling them
		// back would pay out for free stock and delete the evidence of the theft (#1143 review).
		if (LF_Stores.CarriesHeldApart(seller, prefab))
		{
			LF_Debug.Info("Shop", string.Format("sell rejected (carries unpaid store goods) item=%1", prefab));
			return LF_EShopRefusal.NOT_SOLD_HERE;
		}

		int units = LF_ShopRules.StackTradeOutputCount(quantity);
		if (flaggedOnly && units > flagged.Count())
			units = flagged.Count();

		int totalPrice = LF_ShopRules.StackTradeTotal(unitPrice, units);

		int paid = LF_MoneyUtils.AddCash(seller, totalPrice, "shop-sell");
		if (paid != totalPrice)
		{
			if (paid > 0)
				LF_MoneyUtils.RemoveCash(seller, paid, "shop-sell-rollback");
			LF_Debug.Warn("Shop", string.Format("sell failed (cannot pay out) item=%1 qty=%2 price=%3 paid=%4", prefab, quantity, totalPrice, paid));
			return LF_EShopRefusal.POCKETS_FULL;
		}

		int removed;
		if (flaggedOnly)
			removed = LF_StolenGoods.Take(seller, prefab, units);
		else
			removed = LF_InventoryUtils.RemoveAmount(seller, prefab, units);

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
		LF_Debug.Info("Shop", string.Format("sold item=%1 qty=%2 price=%3 stock=%4 flagged=%5", prefab, removed, earned, m_sStockId, flaggedOnly));
		return LF_EShopRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an outlet's gate for one trader and prefab (#988). NONE for a shop that is no outlet.
	//! \param[out] missing What the trader lacks, a localization key; empty when let through.
	protected LF_EShopRefusal CheckOutletGate(ResourceName prefab, IEntity trader, out string missing)
	{
		missing = string.Empty;
		if (m_sOutletId.IsEmpty())
			return LF_EShopRefusal.NONE;

		if (LF_Systems.Refuses(LF_ItemCatalogue.SYSTEM, "outlet trade at " + m_sOutletId))
			return LF_EShopRefusal.SYSTEM_OFF;

		LF_CatalogueItem entry;
		LF_OutletOffer offer = LF_ItemCatalogue.OfferFor(LF_ItemCatalogueConfig.Get(), m_sOutletId, prefab, entry);
		LF_EOutletGate gate = LF_ItemCatalogue.Check(offer, LF_EntityBuyerFacts.Of(trader), missing);
		if (gate == LF_EOutletGate.OPEN)
			return LF_EShopRefusal.NONE;

		LF_Debug.Info(LF_ItemCatalogue.FEATURE, string.Format("trade refused at outlet %1: %2 lacks %3 (%4) for item %5", m_sOutletId, LF_Utils.GetPlayerUID(trader), typename.EnumToString(LF_EOutletGate, gate), missing, prefab));
		return LF_ItemCatalogue.RefusalOf(gate);
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
			if (curated && LF_Utils.SamePrefab(curated.GetItemPrefab(), prefab))
				return curated;
		}

		return null;
	}
};
