//------------------------------------------------------------------------------------------------
//! The shopkeeper pattern: an NPC with an LF_ShopComponent offers both "Talk" and "Browse goods"
//! from one interaction context, and its conversation carries a "Show me what you've got" reply
//! that closes the talk and opens the same price list. Any NPC gets it by carrying a shop
//! component and putting LF_ShopAction in its talk context (LF_Fence.et puts it in "RoleTalk").
//! An authored dialogue adds the reply with LF_ShopkeeperTalk.BrowseOption's fields:
//! text #LF-Shopkeeper_ShowGoods, condition and action "OpenShop", no next node.
class LF_ShopkeeperTalk
{
	static const string HOOK = "OpenShop";
	static const string REPLY_KEY = "#LF-Shopkeeper_ShowGoods";

	//------------------------------------------------------------------------------------------------
	//! The reply, offered only when the NPC keeps a shop that is open.
	static LF_DialogueOption BrowseOption()
	{
		LF_DialogueOption option = new LF_DialogueOption();
		option.m_sTextKey = REPLY_KEY;
		option.m_sCondition = HOOK;
		option.m_sAction = HOOK;
		return option;
	}

	//------------------------------------------------------------------------------------------------
	//! The NPC's shop, when it keeps one that trades now (Shop system on, not shut by a raid).
	static LF_ShopComponent OpenShopOf(IEntity npc)
	{
		if (!npc || !LF_Systems.IsEnabled(LF_ShopRefusal.SYSTEM))
			return null;

		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(npc);
		if (!shop || shop.IsShutShown())
			return null;

		return shop;
	}

	//------------------------------------------------------------------------------------------------
	//! The NPC's own browse action, for its sounds; null is fine (the list opens silent).
	static LF_ShopAction ActionOf(IEntity npc)
	{
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(npc);
		if (!actions)
			return null;

		array<BaseUserAction> all = {};
		actions.GetActionsList(all);
		foreach (BaseUserAction action : all)
		{
			LF_ShopAction shopAction = LF_ShopAction.Cast(action);
			if (shopAction)
				return shopAction;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! "Show me what you've got": offered by an NPC that keeps an open shop; after the talk closes the
//! player's own screen opens the price list. Buying and selling still go through the till RPCs.
class LF_DialogueHook_OpenShop : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(IEntity player, IEntity npc, string param)
	{
		return LF_ShopkeeperTalk.OpenShopOf(npc) != null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		spoken.Insert("#LF-Shopkeeper_ShowGoodsReply");
		LF_Debug.Info("Shop", string.Format("%1 asked %2 to show the goods", LF_Utils.GetPlayerUID(player), LF_Utils.GetPrefabName(npc)));
	}

	//------------------------------------------------------------------------------------------------
	override void AfterScene(IEntity player, IEntity npc, string param)
	{
		LF_ShopComponent shop = LF_ShopkeeperTalk.OpenShopOf(npc);
		if (!shop)
		{
			LF_Debug.Warn("Shop", "show-goods reply picked, but the NPC keeps no open shop");
			return;
		}

		LF_ShopPriceList.Open(shop, LF_ShopkeeperTalk.ActionOf(npc));
	}
}
