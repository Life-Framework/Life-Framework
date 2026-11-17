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

		// The sale resolves on the server (LF_TraderSale). A client's drag only forwards the request:
		// money and items are server state, so a client-side payout always fails.
		IEntity seller = GetOwner();
		if (!seller)
		{
			super.InsertItem(pItem, pStorageTo, pStorageFrom, cb);
			return;
		}

		if (!Replication.IsServer())
		{
			SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (controller)
				controller.LF_RequestTraderSale(pItem, shopOwner);
			else
				FailSell(cb);
			return;
		}

		if (LF_TraderSale.Sell(seller, pItem, shopOwner, false) != LF_ETraderSale.SOLD)
		{
			FailSell(cb);
			return;
		}

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