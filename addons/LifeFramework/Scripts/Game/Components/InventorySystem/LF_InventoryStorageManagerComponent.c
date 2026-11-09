modded class SCR_InventoryStorageManagerComponent
{
	override void InsertItem( IEntity pItem, BaseInventoryStorageComponent pStorageTo = null, BaseInventoryStorageComponent pStorageFrom = null, SCR_InvCallBack cb = null  )
	{
		if (!pItem || !pStorageTo || !pStorageFrom)
		{
			super.InsertItem(pItem, pStorageTo, pStorageFrom, cb);
			return;
		}

		IEntity shopOwner = pStorageTo.GetOwner();
		if (!shopOwner)
		{
			super.InsertItem(pItem, pStorageTo, pStorageFrom, cb);
			return;
		}

		auto trader = LF_TraderManagerComponent.Cast(shopOwner.FindComponent(LF_TraderManagerComponent));
		if (!trader)
		{
			super.InsertItem(pItem, pStorageTo, pStorageFrom, cb);
			return;
		}

		IEntity seller = pStorageFrom.GetOwner();
		if (!seller)
		{
			super.InsertItem(pItem, pStorageTo, pStorageFrom, cb);
			return;
		}

		ResourceName itemPrefab = pItem.GetPrefabData().GetPrefabName();
		int unitValue = LF_TraderManagerComponent.GetUnitValue(trader.m_aTradableItems, itemPrefab);
		if (unitValue <= 0)
		{
			FailSell(cb);
			return;
		}

		if (trader.m_bBlackMarket && !LF_TraderManagerComponent.CanAccessBlackMarket(GetPlayerAccount(seller)))
		{
			FailSell(cb);
			return;
		}

		// One output unit per LF_QuantityComponent quantity, then one cash unit per
		// output. Ported from rekterakathom/EveronLife (MIT), commit
		// 482b74a5a7e39e999d129b0657253696b9b8be15. This trader pays cash rather
		// than spawning a barter prefab; the count is the same rule.
		int outputUnits = LF_ShopRules.StackTradeOutputCount(GetItemQuantity(pItem));
		int totalValue = LF_ShopRules.StackTradeTotal(unitValue, outputUnits);

		// Pay before deleting: a sale that cannot pay out never consumes the item.
		int paid = LF_MoneyUtils.AddCash(seller, totalValue, "trader-sell");
		if (paid != totalValue)
		{
			if (paid > 0)
				LF_MoneyUtils.RemoveCash(seller, paid, "trader-sell-rollback");
			FailSell(cb);
			return;
		}

		if (!this.TryDeleteItem(pItem))
		{
			LF_MoneyUtils.RemoveCash(seller, paid, "trader-sell-rollback");
			FailSell(cb);
			return;
		}

		SCR_HintManagerComponent.ShowCustomHint(WidgetManager.Translate("#EL-Item_Sold", totalValue), "#EL-Trade_Successful", 3.0);
		this.SetReturnCode(EInventoryRetCode.RETCODE_OK);
		if (cb)
			cb.InvokeOnComplete();
	}

	//------------------------------------------------------------------------------------------------
	//! Rejects a sale without claiming a misleading inventory retcode. The
	//! failed callback is what bounces the item back to the seller.
	protected void FailSell(SCR_InvCallBack cb)
	{
		if (cb)
			cb.InvokeOnFailed();
	}

	//------------------------------------------------------------------------------------------------
	//! Stack size of the sold entity; 1 for items without a quantity component.
	protected int GetItemQuantity(IEntity item)
	{
		LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(item);
		if (!quantity)
			return 1;
		return quantity.GetQuantity();
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlayerAccount GetPlayerAccount(IEntity user)
	{
		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (accountManager)
		{
			string playerUid = LF_Utils.GetPlayerUid(user);
			return accountManager.GetAccount(playerUid);
		}
		return null;
	}
}

modded class SCR_InvCallBack
{
	void InvokeOnComplete()
	{
		this.OnComplete();
	}

	void InvokeOnFailed()
	{
		this.OnFailed();
	}
};