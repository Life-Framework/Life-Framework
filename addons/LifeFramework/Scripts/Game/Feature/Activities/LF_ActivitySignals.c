// LF_ActivitySignals.c - the quest SIGNAL a starter step needs beyond the existing ones: a trader paid
// the player for a good, "trader.sold:<GUID>" (the prefab's GUID in upper case, LF_Utils.PrefabGuid),
// so the mining starter's last step ("sell your bars") finishes at the bar buyer. A modded override, so
// the trader file keeps its own path; a duplicate report of the same key is harmless (a starter step
// waits for one sale).

//------------------------------------------------------------------------------------------------
class LF_ActivitySignals
{
	static const string TRADER_SOLD = "trader.sold:";

	//------------------------------------------------------------------------------------------------
	static string TraderSold(ResourceName prefab)
	{
		return TRADER_SOLD + LF_Utils.PrefabGuid(prefab);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_TraderSale
{
	//------------------------------------------------------------------------------------------------
	override static LF_ETraderSale Sell(IEntity seller, IEntity item, IEntity traderOwner, bool untrusted)
	{
		// Read before the sale: a sold item is deleted.
		ResourceName prefab;
		if (item && item.GetPrefabData())
			prefab = item.GetPrefabData().GetPrefabName();

		LF_ETraderSale result = super.Sell(seller, item, traderOwner, untrusted);
		if (result != LF_ETraderSale.SOLD || prefab.IsEmpty() || !seller || !Replication.IsServer())
			return result;

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		if (manager)
			manager.ReportSignal(seller, LF_ActivitySignals.TraderSold(prefab));

		return result;
	}
}
