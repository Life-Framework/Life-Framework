modded class SCR_InventoryStorageManagerComponent
{
	override void InsertItem( IEntity pItem, BaseInventoryStorageComponent pStorageTo = null, BaseInventoryStorageComponent pStorageFrom = null, SCR_InvCallBack cb = null  )
	{
		if(pStorageTo) {
			IEntity owner = pStorageTo.GetOwner();
			auto trader = EL_TraderManagerComponent.Cast(owner.FindComponent(EL_TraderManagerComponent));
			if (trader) {
				ResourceName itemPrefab = pItem.GetPrefabData().GetPrefabName();
				int value = GetItemValue(trader, itemPrefab);
				
				if (value <= 0)
				{
					this.SetReturnCode(EInventoryRetCode.RETCODE_ITEM_TOO_BIG); // Item not tradable
					cb.InvokeOnFailed();
					return;
				}
				
				// Check black market access
				if (trader.m_bBlackMarket)
				{
					IEntity playerEntity = pStorageFrom.GetOwner();
					EL_PlayerAccount account = GetPlayerAccount(playerEntity);
					if (account && account.GetFaction() != EL_Faction.CIVILIAN)
					{
						this.SetReturnCode(EInventoryRetCode.RETCODE_ITEM_TOO_BIG); // Police can't access black market
						cb.InvokeOnFailed();
						return;
					}
				}
				
				// Delete the item
				bool deleteSuccess = this.TryDeleteItem(pItem);
				if (!deleteSuccess){
					cb.InvokeOnFailed();
					return;
				}
				
				// Deposit money to player's account
				EL_ATMManager atmManager = EL_ATMManager.GetInstance();
				if (atmManager)
				{
					IEntity playerEntity = pStorageFrom.GetOwner(); // Assuming pStorageFrom is player's inventory
					string playerUid = EL_Utils.GetPlayerUid(playerEntity);
					atmManager.Deposit(playerUid, value);
					
					// Show notification
					SCR_HintManagerComponent.ShowCustomHint(string.Format("#EL-Item_Sold", value), "Trade Successful", 3.0);
				}
				
				this.SetReturnCode(EInventoryRetCode.RETCODE_OK);
				cb.InvokeOnComplete();
				return;
			}
		}
		super.InsertItem(pItem, pStorageTo, pStorageFrom, cb);
	}
	
	protected int GetItemValue(EL_TraderManagerComponent trader, ResourceName itemPrefab)
	{
		foreach (EL_TraderItem tradableItem : trader.m_aTradableItems)
		{
			if (tradableItem.m_ItemPrefab == itemPrefab)
			{
				return tradableItem.m_ValuePerItem;
			}
		}
		return 0;
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