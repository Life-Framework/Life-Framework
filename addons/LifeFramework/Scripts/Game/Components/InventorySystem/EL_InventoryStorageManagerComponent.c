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
		int unitValue = GetItemValue(trader, itemPrefab);
		if (unitValue <= 0)
		{
			FailSell(cb);
			return;
		}

		if (trader.m_bBlackMarket && !CanAccessBlackMarket(seller))
		{
			FailSell(cb);
			return;
		}

		int totalValue = unitValue * GetItemQuantity(pItem);

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

		SCR_HintManagerComponent.ShowCustomHint(string.Format("#EL-Item_Sold", totalValue), "#EL-Trade_Successful", 3.0);
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
	protected int GetItemValue(EL_TraderManagerComponent trader, ResourceName itemPrefab)
	{
		if (!trader.m_aTradableItems)
			return 0;

		foreach (EL_TraderItem tradableItem : trader.m_aTradableItems)
		{
			if (tradableItem.m_ItemPrefab == itemPrefab)
			{
				return tradableItem.m_ValuePerItem;
			}
		}
		return 0;
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
	//! Black market access. Fails closed: an account that cannot be verified
	//! as civilian is not admitted.
	protected bool CanAccessBlackMarket(IEntity playerEntity)
	{
		EL_PlayerAccount account = GetPlayerAccount(playerEntity);
		if (!account)
			return false;
		return account.GetFaction() == EL_Faction.CIVILIAN;
	}

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