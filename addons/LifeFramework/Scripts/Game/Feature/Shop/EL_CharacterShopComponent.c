//------------------------------------------------------------------------------------------------
//! Client-to-server bridge for shop transactions, on the owning player's character (the entity a
//! client is allowed to RPC on). The menu calls RequestBuy/RequestSell; the server resolves the
//! shop from its RplId, re-derives every price from the price manager, and does the money and
//! inventory work. Nothing the client sends is trusted beyond naming the shop and the item.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Shop", description: "Shop transaction bridge for the owning player")]
class EL_CharacterShopComponentClass : ScriptComponentClass
{
}

class EL_CharacterShopComponent : ScriptComponent
{
	//! How far the player may be from the shop before the server rejects a transaction.
	protected const float MAX_SHOP_DISTANCE = 30;

	//! Upper bound on one buy or sell request. Every bought unit is spawned and every sold unit is
	//! deleted, so an unbounded quantity is an unbounded entity loop on the server.
	protected const int MAX_TRANSACTION_QUANTITY = 99;

	//------------------------------------------------------------------------------------------------
	//! Buy items from a shop for the owning player.
	//! \param[in] shopId RplId of the shop entity.
	//! \param[in] prefab Prefab of the item to buy.
	//! \param[in] quantity How many to buy.
	void RequestBuy(RplId shopId, ResourceName prefab, int quantity)
	{
		if (!shopId.IsValid() || prefab.IsEmpty() || quantity < 1)
			return;

		string prefabString = prefab;
		if (Replication.IsServer())
			RpcAsk_Buy(shopId, prefabString, quantity);
		else
			Rpc(RpcAsk_Buy, shopId, prefabString, quantity);
	}

	//------------------------------------------------------------------------------------------------
	//! Sell items to a shop for the owning player.
	//! \param[in] shopId RplId of the shop entity.
	//! \param[in] prefab Prefab of the item to sell.
	//! \param[in] quantity How many to sell.
	void RequestSell(RplId shopId, ResourceName prefab, int quantity)
	{
		if (!shopId.IsValid() || prefab.IsEmpty() || quantity < 1)
			return;

		string prefabString = prefab;
		if (Replication.IsServer())
			RpcAsk_Sell(shopId, prefabString, quantity);
		else
			Rpc(RpcAsk_Sell, shopId, prefabString, quantity);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: charge the buyer and deliver the items. Price and availability are re-derived here,
	//! never taken from the client.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Buy(RplId shopId, string prefab, int quantity)
	{
		if (!Replication.IsServer())
			return;
		if (quantity < 1 || quantity > MAX_TRANSACTION_QUANTITY)
			return;

		IEntity player = GetOwner();
		if (!player)
			return;

		EL_ShopComponent shop = ResolveShop(shopId);
		if (!shop)
		{
			EL_Debug.Log("Shop", "buy rejected (shop does not resolve)");
			return;
		}

		if (vector.Distance(player.GetOrigin(), shop.GetOwner().GetOrigin()) > MAX_SHOP_DISTANCE)
		{
			EL_Debug.Log("Shop", "buy rejected (out of range)");
			return;
		}

		ResourceName prefabName = prefab;
		if (prefabName.IsEmpty())
			return;

		bool success = shop.BuyItem(prefabName, quantity, player);
		RespondResult(success, prefabName, quantity);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take the items from the seller and pay out the re-derived sell price.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Sell(RplId shopId, string prefab, int quantity)
	{
		if (!Replication.IsServer())
			return;
		if (quantity < 1 || quantity > MAX_TRANSACTION_QUANTITY)
			return;

		IEntity player = GetOwner();
		if (!player)
			return;

		EL_ShopComponent shop = ResolveShop(shopId);
		if (!shop)
		{
			EL_Debug.Log("Shop", "sell rejected (shop does not resolve)");
			return;
		}

		if (vector.Distance(player.GetOrigin(), shop.GetOwner().GetOrigin()) > MAX_SHOP_DISTANCE)
		{
			EL_Debug.Log("Shop", "sell rejected (out of range)");
			return;
		}

		ResourceName prefabName = prefab;
		if (prefabName.IsEmpty())
			return;

		bool success = shop.SellItem(prefabName, quantity, player);
		RespondResult(success, prefabName, quantity);
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers the transaction outcome to the owning client. A listen-server host has no wire hop
	//! for an Owner-targeted RPC to itself, so the handler runs directly there.
	protected void RespondResult(bool success, ResourceName prefab, int quantity)
	{
		string prefabString = prefab;
		if (IsLocalPlayer())
		{
			RpcDo_TransactionResult(success, prefabString, quantity);
			return;
		}

		Rpc(RpcDo_TransactionResult, success, prefabString, quantity);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: outcome of a buy or sell request, for the menu to refresh its rows and balances.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_TransactionResult(bool success, string prefab, int quantity)
	{
		EL_ShopMenu menu = EL_ShopMenu.Cast(GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.Shop));
		if (menu)
			menu.OnTransactionResult(success, prefab, quantity);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLocalPlayer()
	{
		IEntity player = GetOwner();
		if (!player)
			return false;

		RplComponent rpl = RplComponent.Cast(player.FindComponent(RplComponent));
		if (!rpl)
			return false;

		return rpl.IsOwner();
	}

	//------------------------------------------------------------------------------------------------
	protected EL_ShopComponent ResolveShop(RplId shopId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
		if (!rpl)
			return null;

		IEntity entity = rpl.GetEntity();
		if (!entity)
			return null;

		return EL_ShopComponent.Cast(entity.FindComponent(EL_ShopComponent));
	}
};