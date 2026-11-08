//------------------------------------------------------------------------------------------------
//! Client-to-server bridge for shop transactions, on the owning player's character (the entity a
//! client is allowed to RPC on). The price list calls RequestBuy/RequestSell; the server resolves
//! the shop from its RplId, re-derives every price from the price manager, and does the money and
//! inventory work. Nothing the client sends is trusted beyond naming the shop and the item. Every
//! request gets a reply with the reason when refused, so the order slip never hangs.
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
	//! The order slip clamps its quantity to it, so it is public.
	static const int MAX_TRANSACTION_QUANTITY = 99;

	//! World cue at the till: the character handling the goods and notes, heard by everyone nearby.
	static const string TILL_SOUND = "SOUND_PICK_UP";

	//------------------------------------------------------------------------------------------------
	//! Buy items from a shop for the owning player.
	//! \param[in] shopId RplId of the shop entity.
	//! \param[in] prefab Prefab of the item to buy.
	//! \param[in] quantity How many to buy.
	void RequestBuy(RplId shopId, ResourceName prefab, int quantity)
	{
		if (!shopId.IsValid() || prefab.IsEmpty() || quantity < 1)
		{
			EL_Debug.Warn("Shop", "buy not sent: no shop, item or quantity");
			return;
		}

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
		{
			EL_Debug.Warn("Shop", "sale not sent: no shop, item or quantity");
			return;
		}

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
		ServeTrade(true, shopId, prefab, quantity);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take the items from the seller and pay out the re-derived sell price.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Sell(RplId shopId, string prefab, int quantity)
	{
		ServeTrade(false, shopId, prefab, quantity);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one buy or sell, from the request checks to the reply.
	protected void ServeTrade(bool buy, RplId shopId, string prefab, int quantity)
	{
		if (!Replication.IsServer())
			return;

		IEntity player = GetOwner();
		if (!player)
		{
			EL_Debug.Error("Shop", "trade rejected: no owning character");
			return;
		}

		ResourceName prefabName = prefab;
		int money = 0;
		LF_EShopRefusal reason = CheckRequest(player, shopId, prefabName, quantity);
		if (reason == LF_EShopRefusal.NONE)
		{
			EL_ShopComponent shop = ResolveShop(shopId);
			if (buy)
				reason = shop.Buy(prefabName, quantity, player, money);
			else
				reason = shop.Sell(prefabName, quantity, player, money);
		}

		if (reason == LF_EShopRefusal.NONE)
			PlayTillSound(player);

		RespondResult(buy, reason, prefabName, quantity, money, EL_MoneyUtils.GetCash(player));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: quantity, shop and range checks shared by buy and sell.
	protected LF_EShopRefusal CheckRequest(IEntity player, RplId shopId, ResourceName prefab, int quantity)
	{
		if (quantity < 1 || quantity > MAX_TRANSACTION_QUANTITY)
		{
			EL_Debug.Warn("Shop", string.Format("trade rejected (quantity %1 outside 1..%2) item=%3", quantity, MAX_TRANSACTION_QUANTITY, prefab));
			return LF_EShopRefusal.BAD_QUANTITY;
		}

		if (prefab.IsEmpty())
		{
			EL_Debug.Warn("Shop", "trade rejected (no item)");
			return LF_EShopRefusal.NOT_SOLD_HERE;
		}

		EL_ShopComponent shop = ResolveShop(shopId);
		if (!shop)
		{
			EL_Debug.Warn("Shop", "trade rejected (shop does not resolve)");
			return LF_EShopRefusal.NO_SHOP;
		}

		if (vector.Distance(player.GetOrigin(), shop.GetOwner().GetOrigin()) > MAX_SHOP_DISTANCE)
		{
			EL_Debug.Warn("Shop", "trade rejected (out of range)");
			return LF_EShopRefusal.TOO_FAR;
		}

		return LF_EShopRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected static void PlayTillSound(IEntity player)
	{
		SCR_InventoryStorageManagerComponent inventory = EL_Component<SCR_InventoryStorageManagerComponent>.Find(player);
		if (!inventory)
		{
			EL_Debug.Warn("Shop", "no inventory to play the till sound on");
			return;
		}

		inventory.RpcAsk_PlaySound(EL_NetworkUtils.GetRplId(player), TILL_SOUND);
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers the transaction outcome to the owning client. A listen-server host has no wire hop
	//! for an Owner-targeted RPC to itself, so the handler runs directly there.
	protected void RespondResult(bool buy, LF_EShopRefusal reason, ResourceName prefab, int quantity, int money, int cash)
	{
		string prefabString = prefab;
		if (IsLocalPlayer())
		{
			RpcDo_TransactionResult(buy, reason, prefabString, quantity, money, cash);
			return;
		}

		Rpc(RpcDo_TransactionResult, buy, reason, prefabString, quantity, money, cash);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: outcome of a buy or sell request, printed on the order slip.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_TransactionResult(bool buy, int reason, string prefab, int quantity, int money, int cash)
	{
		LF_ShopPriceList.OnResult(buy, LF_ShopRefusal.FromWire(reason), prefab, quantity, money, cash);
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
