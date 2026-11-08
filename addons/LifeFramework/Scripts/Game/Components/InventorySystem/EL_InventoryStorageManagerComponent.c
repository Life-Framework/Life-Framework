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

		auto trader = EL_TraderManagerComponent.Cast(shopOwner.FindComponent(EL_TraderManagerComponent));
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
		int unitValue = EL_TraderManagerComponent.GetUnitValue(trader.m_aTradableItems, itemPrefab);
		if (unitValue <= 0)
		{
			FailSell(cb);
			return;
		}

		if (trader.m_bBlackMarket && !EL_TraderManagerComponent.CanAccessBlackMarket(GetPlayerAccount(seller)))
		{
			FailSell(cb);
			return;
		}

		// One output unit per EL_QuantityComponent quantity, then one cash unit per
		// output. Ported from rekterakathom/EveronLife (MIT), commit
		// 482b74a5a7e39e999d129b0657253696b9b8be15. This trader pays cash rather
		// than spawning a barter prefab; the count is the same rule.
		int outputUnits = EL_ShopRules.StackTradeOutputCount(GetItemQuantity(pItem));
		int totalValue = EL_ShopRules.StackTradeTotal(unitValue, outputUnits);

		// Pay before deleting: a sale that cannot pay out never consumes the item.
		int paid = EL_MoneyUtils.AddCash(seller, totalValue);
		if (paid != totalValue)
		{
			if (paid > 0)
				EL_MoneyUtils.RemoveCash(seller, paid);
			FailSell(cb);
			return;
		}

		if (!this.TryDeleteItem(pItem))
		{
			EL_MoneyUtils.RemoveCash(seller, paid);
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
		EL_QuantityComponent quantity = EL_Component<EL_QuantityComponent>.Find(item);
		if (!quantity)
			return 1;
		return quantity.GetQuantity();
	}

	//------------------------------------------------------------------------------------------------
	protected EL_PlayerAccount GetPlayerAccount(IEntity user)
	{
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (accountManager)
		{
			string playerUid = EL_Utils.GetPlayerUid(user);
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