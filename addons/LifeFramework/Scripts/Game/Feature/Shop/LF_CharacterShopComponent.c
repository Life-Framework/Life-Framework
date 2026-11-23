//------------------------------------------------------------------------------------------------
//! Client-to-server bridge for shop transactions, on the owning player's character (the entity a
//! client is allowed to RPC on). The price list calls RequestBuy/RequestSell; the server resolves
//! the shop from its RplId, re-derives every price from the price manager, and does the money and
//! inventory work. Nothing the client sends is trusted beyond naming the shop and the item. Every
//! request gets a reply with the reason when refused, so the order slip never hangs.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Shop", description: "Shop transaction bridge for the owning player")]
class LF_CharacterShopComponentClass : ScriptComponentClass
{
}

class LF_CharacterShopComponent : ScriptComponent
{
	//! The shop distance and the per-order unit cap are LF_ShopLimitsConfig fields. Every bought unit is
	//! spawned and every sold unit deleted, so an unbounded quantity would be an unbounded entity loop.

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
			LF_Debug.Warn("Shop", "buy not sent: no shop, item or quantity");
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
			LF_Debug.Warn("Shop", "sale not sent: no shop, item or quantity");
			return;
		}

		string prefabString = prefab;
		if (Replication.IsServer())
			RpcAsk_Sell(shopId, prefabString, quantity);
		else
			Rpc(RpcAsk_Sell, shopId, prefabString, quantity);
	}

	//------------------------------------------------------------------------------------------------
	//! Apply for, ask after, or collect a licence at a shop counter's licence row for the owning player.
	//! \param[in] shopId RplId of the shop entity.
	//! \param[in] licence The licence the row grants (LF_ELicenseType name).
	void RequestLicence(RplId shopId, string licence)
	{
		if (!shopId.IsValid() || licence.IsEmpty())
		{
			LF_Debug.Warn("Shop", "licence request not sent: no shop or licence");
			return;
		}

		if (Replication.IsServer())
			RpcAsk_Licence(shopId, licence);
		else
			Rpc(RpcAsk_Licence, shopId, licence);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one licence-row visit. The fee, record and wait are the licence office's
	//! (LF_ShopComponent.ApplyForLicence); the client only names the shop and the licence.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Licence(RplId shopId, string licence)
	{
		if (!Replication.IsServer())
			return;

		IEntity player = GetOwner();
		if (!player)
		{
			LF_Debug.Error("Shop", "licence rejected: no owning character");
			return;
		}

		if (!LF_RateLimit.AllowCharacter(player, LF_ERequestKind.PURCHASE, "shop licence", false))
		{
			RespondLicence(LF_EShopRefusal.TOO_FAST, LF_ELicenceDesk.FAILED, string.Empty, LF_MoneyUtils.GetCash(player));
			return;
		}

		LF_ELicenseType type;
		if (!LF_ItemCatalogue.ParseLicence(licence, type))
		{
			LF_Debug.Warn("Shop", "licence rejected: '" + licence + "' is not a licence");
			RespondLicence(LF_EShopRefusal.NOT_SOLD_HERE, LF_ELicenceDesk.FAILED, string.Empty, LF_MoneyUtils.GetCash(player));
			return;
		}

		LF_ShopComponent shop;
		LF_EShopRefusal reason = CheckShop(player, shopId, shop);
		LF_ELicenceDesk outcome = LF_ELicenceDesk.FAILED;
		int minutesLeft;
		if (reason == LF_EShopRefusal.NONE)
			reason = shop.ApplyForLicence(player, type, outcome, minutesLeft);

		if (reason == LF_EShopRefusal.NONE && (outcome == LF_ELicenceDesk.APPLIED || outcome == LF_ELicenceDesk.ISSUED))
			PlayTillSound(player);

		RespondLicence(reason, outcome, LF_LicenceDesk.AnswerArg(outcome, minutesLeft), LF_MoneyUtils.GetCash(player));
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers a licence-row answer to the owning client (directly on a listen-server host).
	protected void RespondLicence(LF_EShopRefusal reason, LF_ELicenceDesk outcome, string arg, int cash)
	{
		if (IsLocalPlayer())
		{
			RpcDo_LicenceResult(reason, outcome, arg, cash);
			return;
		}

		Rpc(RpcDo_LicenceResult, reason, outcome, arg, cash);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the counter's answer to a licence request, printed on the order slip.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LicenceResult(int reason, int outcome, string arg, int cash)
	{
		LF_ShopPriceList.OnLicenceResult(LF_ShopRefusal.FromWire(reason), outcome, arg, cash);
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
			LF_Debug.Error("Shop", "trade rejected: no owning character");
			return;
		}

		ResourceName prefabName = prefab;
		int money = 0;
		// #693b: the slip prints the refusal, so the limiter does not toast as well.
		if (!LF_RateLimit.AllowCharacter(player, LF_ERequestKind.PURCHASE, "shop trade", false))
		{
			RespondResult(buy, LF_EShopRefusal.TOO_FAST, prefabName, quantity, 0, LF_MoneyUtils.GetCash(player), string.Empty);
			return;
		}

		string missing;
		LF_EShopRefusal reason = CheckRequest(player, shopId, prefabName, quantity);
		LF_ShopComponent shop;
		if (reason == LF_EShopRefusal.NONE)
		{
			shop = ResolveShop(shopId);
			// Outposts (#4217, #4224): the clerk does not serve an on-duty officer, and says so.
			if (LF_Outpost.RefusesService(player, shop.GetOwner().GetOrigin()))
			{
				LF_Outpost.NotifyRefusal(player, shop.GetOwner().GetOrigin());
				reason = LF_EShopRefusal.RESIDENT_REFUSES;
			}
			else if (buy)
				reason = shop.Buy(prefabName, quantity, player, money, missing);
			else
				reason = shop.Sell(prefabName, quantity, player, money, missing);
		}

		if (reason == LF_EShopRefusal.NONE)
		{
			PlayTillSound(player);
			LF_OutpostStanding.OnSale(player, shop.GetOwner().GetOrigin());
		}

		RespondResult(buy, reason, prefabName, quantity, money, LF_MoneyUtils.GetCash(player), missing);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: quantity, shop and range checks shared by buy and sell.
	protected LF_EShopRefusal CheckRequest(IEntity player, RplId shopId, ResourceName prefab, int quantity)
	{
		if (quantity < 1 || quantity > LF_ShopLimitsConfig.OrderMaxQuantity())
		{
			LF_Debug.Warn("Shop", string.Format("trade rejected (quantity %1 outside 1..%2) item=%3", quantity, LF_ShopLimitsConfig.OrderMaxQuantity(), prefab));
			return LF_EShopRefusal.BAD_QUANTITY;
		}

		if (prefab.IsEmpty())
		{
			LF_Debug.Warn("Shop", "trade rejected (no item)");
			return LF_EShopRefusal.NOT_SOLD_HERE;
		}

		LF_ShopComponent shop;
		return CheckShop(player, shopId, shop);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the shop resolves and the player stands within reach of it.
	protected LF_EShopRefusal CheckShop(IEntity player, RplId shopId, out LF_ShopComponent shop)
	{
		shop = ResolveShop(shopId);
		if (!shop)
		{
			LF_Debug.Warn("Shop", "trade rejected (shop does not resolve)");
			return LF_EShopRefusal.NO_SHOP;
		}

		if (vector.Distance(player.GetOrigin(), shop.GetOwner().GetOrigin()) > LF_ShopLimitsConfig.ShopDistance())
		{
			LF_Debug.Warn("Shop", "trade rejected (out of range)");
			return LF_EShopRefusal.TOO_FAR;
		}

		return LF_EShopRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected static void PlayTillSound(IEntity player)
	{
		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(player);
		if (!inventory)
		{
			LF_Debug.Warn("Shop", "no inventory to play the till sound on");
			return;
		}

		inventory.RpcAsk_PlaySound(LF_NetworkUtils.GetRplId(player), TILL_SOUND);
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers the transaction outcome to the owning client. A listen-server host has no wire hop
	//! for an Owner-targeted RPC to itself, so the handler runs directly there.
	//! missing is what an outlet's gate found the buyer lacks (a localization key, #988), else empty.
	protected void RespondResult(bool buy, LF_EShopRefusal reason, ResourceName prefab, int quantity, int money, int cash, string missing)
	{
		string prefabString = prefab;
		if (IsLocalPlayer())
		{
			RpcDo_TransactionResult(buy, reason, prefabString, quantity, money, cash, missing);
			return;
		}

		Rpc(RpcDo_TransactionResult, buy, reason, prefabString, quantity, money, cash, missing);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: outcome of a buy or sell request, printed on the order slip.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_TransactionResult(bool buy, int reason, string prefab, int quantity, int money, int cash, string missing)
	{
		LF_ShopPriceList.OnResult(buy, LF_ShopRefusal.FromWire(reason), prefab, quantity, money, cash, missing);
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
	protected LF_ShopComponent ResolveShop(RplId shopId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shopId));
		if (!rpl)
			return null;

		IEntity entity = rpl.GetEntity();
		if (!entity)
			return null;

		return LF_ShopComponent.Cast(entity.FindComponent(LF_ShopComponent));
	}
};
