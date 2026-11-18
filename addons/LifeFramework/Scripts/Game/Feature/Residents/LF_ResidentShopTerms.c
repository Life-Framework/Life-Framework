// LF_ResidentShopTerms.c - a resident behind a counter sets the terms of the sale (#2911, epic #1012 beat 4).
// Server side. When the clerk of a shop is a resident (their work line stands on the counter anchor of
// that shop), the memory word the clerk holds for the buyer (LF_ResidentMemory.Tier, #2859) changes the
// deal: `regular` pays a config share less, `refuses` is turned away in words ("Not to you.") and no
// money or item moves. The word is read twice on the server: when the till prices the order and again
// when it charges. The client never sees the word or the score (M6 decision 7).
//
// The seam is one `modded class LF_ShopComponent` below: Buy asks first, then prices every line through
// GetBuyPrice while the buyer's word is set. LF_ShopPriceResolver is untouched. A discount never goes
// under what the same shop pays for the item, so buying at the regular's price and selling back is
// never a profit (the no-profit-loop rule, #686).

//------------------------------------------------------------------------------------------------
//! The shop-terms tuning. No .conf row yet: the field default is the shipped value and a pack or an
//! owner can ship a conf for it later; tests swap it with UseForTest.
[BaseContainerProps(configRoot: true)]
class LF_ResidentShopTermsConfig
{
	[Attribute("10", UIWidgets.EditBox, "Percent a regular pays less at the shop their friend keeps (0 turns the discount off)")]
	int m_iRegularDiscountPercent;

	protected static ref LF_ResidentShopTermsConfig s_Config;

	//------------------------------------------------------------------------------------------------
	static LF_ResidentShopTermsConfig Get()
	{
		if (!s_Config)
		{
			s_Config = new LF_ResidentShopTermsConfig();
			s_Config.m_iRegularDiscountPercent = LF_ResidentShopTerms.DEFAULT_REGULAR_PERCENT;
		}

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static void UseForTest(LF_ResidentShopTermsConfig config)
	{
		s_Config = config;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
	}
}

//------------------------------------------------------------------------------------------------
//! Who keeps a shop. The default asks the live residents; a test swaps in a fixed answer.
class LF_ResidentClerkFinder
{
	//! A resident this near (horizontal metres) the shop, standing on a `work`/`counter` line, keeps it.
	static const float CLERK_RADIUS_M = 5;	// lf-fixed: how near the counter a clerk stands

	//------------------------------------------------------------------------------------------------
	//! The id of the live resident whose work anchor is this shop's counter, or empty.
	string ClerkAt(notnull IEntity shop)
	{
		array<string> ids = {};
		LF_ResidentTimetable.GetIds(ids);
		ids.Sort();
		int minute = LF_ResidentTimetable.NowMinute();
		vector at = shop.GetOrigin();
		foreach (string id : ids)
		{
			IEntity live = LF_ResidentSpawner.GetLive(id);
			if (!live)
				continue;

			LF_ResidentSlot slot = LF_ResidentTimetable.WhereIs(id, minute);
			if (!slot || slot.m_sRole != LF_ResidentTimetable.ROLE_WORK || slot.m_sAnchor != LF_ResidentShopTerms.COUNTER_ANCHOR)
				continue;

			vector there = live.GetOrigin();
			float dx = there[0] - at[0];
			float dz = there[2] - at[2];
			if (dx * dx + dz * dz <= CLERK_RADIUS_M * CLERK_RADIUS_M)
				return id;
		}

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
class LF_ResidentShopTerms
{
	static const string FEATURE = "Residents";
	static const string COUNTER_ANCHOR = "counter";
	static const int DEFAULT_REGULAR_PERCENT = 10;	// lf-fixed: the shipped default of LF_ResidentShopTermsConfig

	protected static ref LF_ResidentClerkFinder s_Finder;

	//------------------------------------------------------------------------------------------------
	//! Tests: the finder ClerkOf asks until ResetStatics.
	static void UseFinderForTest(LF_ResidentClerkFinder finder)
	{
		s_Finder = finder;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Finder = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The resident who keeps the shop at `shop`, or empty when nobody does.
	static string ClerkOf(IEntity shop)
	{
		if (!shop)
			return string.Empty;

		if (!s_Finder)
			s_Finder = new LF_ResidentClerkFinder();

		return s_Finder.ClerkAt(shop);
	}

	//------------------------------------------------------------------------------------------------
	//! The memory word the shop's resident clerk holds for the buyer, or empty when the shop has no
	//! resident clerk (or no buyer uid): then the sale is as it always was.
	static string WordAt(IEntity shop, string buyerUid)
	{
		if (buyerUid.IsEmpty())
			return string.Empty;

		string clerk = ClerkOf(shop);
		if (clerk.IsEmpty())
			return string.Empty;

		return LF_ResidentMemory.Tier(clerk, buyerUid);
	}

	//------------------------------------------------------------------------------------------------
	static bool Refuses(string word)
	{
		return word == LF_ResidentMemory.TIER_REFUSES;
	}

	//------------------------------------------------------------------------------------------------
	//! The unit price a buyer with this word pays. Only `regular` pays less, by `percent` (rounded
	//! down to whole money), and never below `floor` (what the shop pays for the item) nor below 1.
	//! A price that is 0 or less (unpriced) is returned as it came.
	static int UnitPrice(int price, string word, int percent, int floor)
	{
		if (price <= 0 || word != LF_ResidentMemory.TIER_REGULAR || percent <= 0)
			return price;

		int discounted = price * (100 - LF_Utils.ClampInt(percent, 0, 100)) / 100;
		discounted = LF_Utils.MaxInt(discounted, floor);
		discounted = LF_Utils.MaxInt(discounted, 1);
		return LF_Utils.MinInt(discounted, price);
	}
}

//------------------------------------------------------------------------------------------------
//! The one seam: the till asks the resident clerk before it takes an order and prices the order at
//! the buyer's terms. Server only; a client never calls Buy.
modded class LF_ShopComponent
{
	//! The buyer's word while one Buy runs, else empty.
	protected string m_sResidentWord;

	//------------------------------------------------------------------------------------------------
	override LF_EShopRefusal Buy(ResourceName prefab, int quantity, IEntity buyer, out int charged, out string missing)
	{
		m_sResidentWord = string.Empty;
		if (Replication.IsServer() && buyer)
		{
			string uid = LF_Utils.GetPlayerUid(buyer);
			string word = LF_ResidentShopTerms.WordAt(GetOwner(), uid);
			if (LF_ResidentShopTerms.Refuses(word))
			{
				charged = 0;
				missing = string.Empty;
				LF_Debug.Info(LF_ResidentShopTerms.FEATURE, string.Format("shop: the clerk refuses %1 (%2)", uid, word));
				return LF_EShopRefusal.RESIDENT_REFUSES;
			}

			m_sResidentWord = word;
		}

		LF_EShopRefusal result = super.Buy(prefab, quantity, buyer, charged, missing);
		m_sResidentWord = string.Empty;
		return result;
	}

	//------------------------------------------------------------------------------------------------
	override int GetBuyPrice(ResourceName prefab)
	{
		int price = super.GetBuyPrice(prefab);
		if (m_sResidentWord.IsEmpty())
			return price;

		LF_ResidentShopTermsConfig config = LF_ResidentShopTermsConfig.Get();
		int paid = LF_ResidentShopTerms.UnitPrice(price, m_sResidentWord, config.m_iRegularDiscountPercent, GetSellPrice(prefab));
		if (paid != price)
			LF_Debug.Info(LF_ResidentShopTerms.FEATURE, string.Format("shop: regular's price for %1 is %2 (was %3)", prefab, paid, price));

		return paid;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the shop-terms statics (LF_StaticReset finds it by reflection).
class LF_ResidentShopTerms_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ResidentShopTerms";
	}

	override void Apply()
	{
		LF_ResidentShopTermsConfig.ResetStatics();
		LF_ResidentShopTerms.ResetStatics();
	}
}
