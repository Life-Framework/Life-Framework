//------------------------------------------------------------------------------------------------
//! The buyer-price perk (WK-W11_2, #4409). A character with `good_word` in a work trade is paid
//! GOOD_WORD_PERCENT more by a trader for that trade's goods: a miner for ore and bars, a lumberjack
//! for planks and logs, a fisher for fish, a farmer for sacks. The plan caps the share at +15% at
//! premium buyers; 10% keeps a perked loop under the crime band (`tools\cli economy`, economy-perks).
//!
//! The server prices the sale (LF_TraderSale.Sell and SellToBuyer); the sell rows quote the same
//! number (LF_TraderSale.Quote), so the displayed price is the paid price. The market curve moves by
//! units sold, never by the perk, so a perk holder does not push the price any harder.
class LF_PerkPrice
{
	static const string FEATURE = "PerkPrice";

	static const string PERK_GOOD_WORD = "good_word";

	static const int GOOD_WORD_PERCENT = 10; // lf-fixed: share more a trader pays a perk holder for the trade's goods (cap 15)

	//------------------------------------------------------------------------------------------------
	//! The work trade a good belongs to, by its prefab folder, or "" when it belongs to none.
	static string TradeOfGood(ResourceName prefab)
	{
		string path = prefab;
		if (path.IsEmpty())
			return string.Empty;

		if (path.Contains("Prefabs/Resources/Mining/"))
			return "MINER";

		if (path.Contains("Prefabs/Resources/WoodCutting/"))
			return "LUMBERJACK";

		if (path.Contains("Prefabs/Items/Fishing/"))
			return "FISHER";

		if (path.Contains("Prefabs/Items/Harvest/"))
			return "FARMER";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The share of a good's price the character's perks add, in percent: 0 with no perk, a good that
	//! belongs to no trade, or a perk held in another trade.
	static int Percent(string playerUid, ResourceName prefab)
	{
		string trade = TradeOfGood(prefab);
		if (trade.IsEmpty() || playerUid.IsEmpty())
			return 0;

		if (LF_PerkEffects.Holds(playerUid, trade, PERK_GOOD_WORD))
			return GOOD_WORD_PERCENT;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! What a trader pays for a stack once the perk is counted: the plain total plus the share, rounded
	//! to the nearest cash, never under the plain total. Hundreds and the remainder are split so a total
	//! near the 2^31 limit does not overflow.
	static int Adjust(string playerUid, ResourceName prefab, int total)
	{
		if (total <= 0)
			return total;

		int percent = Percent(playerUid, prefab);
		if (percent <= 0)
			return total;

		int extra = (total / 100) * percent + ((total % 100) * percent + 50) / 100;
		return total + extra;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the same adjustment for a seller entity (LF_TraderSale).
	static int AdjustFor(IEntity seller, ResourceName prefab, int total)
	{
		if (!seller)
			return total;

		return Adjust(LF_Utils.GetPlayerUid(seller), prefab, total);
	}
}
