//------------------------------------------------------------------------------------------------
//! Why a stall trade step refused, or OK. `RULE` means `LF_StallRules` said no: the rule's own answer is the
//! `out LF_EStallResult`. A refused step moves no cash and no item (or puts back what it moved).
enum LF_EStallTrade
{
	OK,
	RULE,			// the stall rules refused; see the out rule
	BAD_INPUT,		// not on the server, no stall, no character or no key
	OUT_OF_REACH,	// the character is further than LF_StallComponent.REACH from the stall
	NOT_HOLDER,		// only the stall's holder lists or takes down listings
	NO_STOCK,		// the holder does not carry the item (any more)
	HOLDER_AWAY,	// the holder's character is not in the world, so no item can change hands
	CASH_SHORT,		// the buyer's purse came up short when the cash was taken
	BUYER_FULL		// the item did not fit the buyer's inventory
}

//------------------------------------------------------------------------------------------------
//! Listing goods on a stall and buying them (#2926, framework #786). Server logic over the rules of #2924
//! (`LF_StallRules`) and the stall prop of #2925 (`LF_StallComponent`); no state of its own.
//!
//! A LISTING IS A CLAIM, NOT CUSTODY. The listed item stays in the holder's inventory; the listing is its
//! price tag. The item moves only inside `Buy`, in one server call, so nothing is held in a stall between
//! frames: a restart, a crash or a holder logging out cannot lose an item, and unsold goods at closing time
//! are simply still the holder's. The cost: the holder can carry the item away, so `Buy` checks the stock
//! again after the buyer's cash is reserved and, if it is gone, refunds and drops the stale listing.
//!
//! BUY, in this order, each step undone by the one before when it fails:
//!   1 rules.Reserve  - the listing is held, no cash moved yet
//!   2 holder stock   - the holder carries the item, else refund and drop the listing (NO_STOCK)
//!   3 take the cash  - the buyer's purse (a short purse refunds, CASH_SHORT)
//!   4 take the item  - from the holder (a miss gives the cash back, NO_STOCK)
//!   5 give the item  - to the buyer (it did not fit: the item goes back to the holder, the cash to the
//!                      buyer, BUYER_FULL)
//!   6 rules.Settle   - the seller is paid the price minus the cut; the market keeps the cut
//! A listing cannot be taken down (`LF_StallRules.Delist`) while its cash is reserved, and one item is
//! never listed twice over the holder's stock (`List` counts the stock against listings of the same prefab).
class LF_StallTrade
{
	//! Ledger reasons of the cash moves.
	static const string REASON_BUY = "stall_buy";
	static const string REASON_SALE = "stall_sale";

	//! Tests: the character standing in for the holder, whose key is `s_sHolderUidForTest`.
	protected static IEntity s_HolderForTest;
	protected static string s_sHolderUidForTest;
	//! Tests: the buyer's inventory takes nothing (a test character's pockets never fill up).
	protected static bool s_bNoRoomForTest;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_HolderForTest = null;
		s_sHolderUidForTest = "";
		s_bNoRoomForTest = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: the next hand-overs find no room in the buyer's inventory.
	static void SetNoRoomForTest(bool noRoom)
	{
		s_bNoRoomForTest = noRoom;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: `holder` is the character of the player whose key is `uid`. A null body clears it.
	static void SetHolderForTest(IEntity holder, string uid)
	{
		s_HolderForTest = holder;
		s_sHolderUidForTest = uid;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The live character of the player with this key, or null when they are not in the world.
	static IEntity FindHolder(string uid)
	{
		if (uid.IsEmpty())
			return null;

		if (s_HolderForTest && s_sHolderUidForTest == uid)
			return s_HolderForTest;

		LF_PlayerControllerManagerCompat players = LF_PlayerControllerManagerCompat.GetInstance();
		if (!players)
			return null;

		array<int> playerIds = {};
		players.GetAllPlayerIds(playerIds);
		foreach (int playerId : playerIds)
		{
			PlayerController controller = players.GetPlayerController(playerId);
			if (!controller)
				continue;

			IEntity body = controller.GetControlledEntity();
			if (body && LF_Utils.GetPlayerUid(body) == uid)
				return body;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EStallTrade Refuse(LF_EStallTrade result, string why)
	{
		LF_Debug.Warn(LF_StallConfig.FEATURE, string.Format("stall trade refused: %1", why));
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when `character` is within reach of the stall prop.
	protected static bool InReach(LF_StallComponent stall, IEntity character)
	{
		IEntity prop = stall.GetOwner();
		return prop && character && vector.Distance(character.GetOrigin(), prop.GetOrigin()) <= LF_StallComponent.REACH;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many open or reserved listings on the stall are of this prefab.
	protected static int ListedOf(LF_Stall stall, ResourceName item)
	{
		int count;
		foreach (LF_StallListing listing : stall.m_aListings)
		{
			if (listing && LF_Utils.SamePrefab(listing.m_sItem, item))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The id of the cheapest listing nobody has reserved (the lowest id on a tie), 0 for none.
	static int CheapestOpen(LF_Stall stall)
	{
		if (!stall)
			return 0;

		LF_StallListing best;
		foreach (LF_StallListing listing : stall.m_aListings)
		{
			if (!listing || listing.m_bReserved)
				continue;

			if (!best || listing.m_iPrice < best.m_iPrice)
				best = listing;
		}

		if (!best)
			return 0;

		return best.m_iId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the holder puts one `item` from their inventory on the stall at `price`. The item stays with
	//! the holder; the stock must cover every listing of that prefab, so one item is never listed twice.
	//! \param holder The holder's character, within reach of the stall.
	//! \param holderUid The holder's key; it must be the stall's holder.
	//! \param listingId The new listing, 0 on a refusal.
	//! \param rule What `LF_StallRules` answered when this returns RULE.
	static LF_EStallTrade List(LF_StallComponent stall, IEntity holder, string holderUid, ResourceName item, int price, out int listingId, out LF_EStallResult rule)
	{
		listingId = 0;
		rule = LF_EStallResult.OK;
		if (!Replication.IsServer() || !stall || !holder || holderUid.IsEmpty() || item.IsEmpty())
			return Refuse(LF_EStallTrade.BAD_INPUT, "a listing needs the server, a stall, its holder and an item");

		LF_Stall state = stall.GetStall();
		if (!state || stall.GetHolderKey() != holderUid)
			return Refuse(LF_EStallTrade.NOT_HOLDER, string.Format("%1 is not the holder of this stall", holderUid));
		if (!InReach(stall, holder))
			return Refuse(LF_EStallTrade.OUT_OF_REACH, string.Format("%1 is out of reach of the stall", holderUid));

		int day;
		int hour;
		LF_StallConfig config = LF_StallConfig.Get();
		if (!config || !LF_StallClock.Now(day, hour))
			return Refuse(LF_EStallTrade.RULE, "no stall config or clock");

		stall.Lapse();
		if (LF_InventoryUtils.GetAmount(holder, item) < ListedOf(state, item) + 1)
			return Refuse(LF_EStallTrade.NO_STOCK, string.Format("%1 does not carry another %2 to list", holderUid, item));

		rule = LF_StallRules.List(config, state, day, item, price, listingId);
		if (rule != LF_EStallResult.OK)
			return LF_EStallTrade.RULE;

		LF_Debug.Info(LF_StallConfig.FEATURE, string.Format("%1 listed %2 at %3 (listing %4)", holderUid, item, price, listingId));
		return LF_EStallTrade.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the holder takes a listing off the stall. Refused while a buyer's cash is reserved for it.
	static LF_EStallTrade Delist(LF_StallComponent stall, string holderUid, int listingId, out LF_EStallResult rule)
	{
		rule = LF_EStallResult.OK;
		if (!Replication.IsServer() || !stall || holderUid.IsEmpty())
			return Refuse(LF_EStallTrade.BAD_INPUT, "delist needs the server, a stall and its holder");

		LF_Stall state = stall.GetStall();
		if (!state || stall.GetHolderKey() != holderUid)
			return Refuse(LF_EStallTrade.NOT_HOLDER, string.Format("%1 is not the holder of this stall", holderUid));

		rule = LF_StallRules.Delist(state, listingId);
		if (rule != LF_EStallResult.OK)
			return LF_EStallTrade.RULE;

		return LF_EStallTrade.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: `buyer` buys listing `listingId`: the price leaves their purse, one item moves from the holder
	//! to them, the seller is paid the price minus the cut. Any failure leaves cash and items where they were.
	//! \param buyerUid The buyer's key (LF_Utils.GetPlayerUid).
	//! \param paid What left the buyer's purse (0 unless OK).
	//! \param sellerPay What the holder was paid (0 unless OK).
	//! \param rule What `LF_StallRules` answered when this returns RULE.
	static LF_EStallTrade Buy(LF_StallComponent stall, IEntity buyer, string buyerUid, int listingId, out int paid, out int sellerPay, out LF_EStallResult rule)
	{
		paid = 0;
		sellerPay = 0;
		rule = LF_EStallResult.OK;
		if (!Replication.IsServer() || !stall || !buyer || buyerUid.IsEmpty())
			return Refuse(LF_EStallTrade.BAD_INPUT, "a purchase needs the server, a stall and a buyer");
		if (!InReach(stall, buyer))
			return Refuse(LF_EStallTrade.OUT_OF_REACH, string.Format("%1 is out of reach of the stall", buyerUid));

		int day;
		int hour;
		LF_StallConfig config = LF_StallConfig.Get();
		if (!config || !LF_StallClock.Now(day, hour))
			return Refuse(LF_EStallTrade.RULE, "no stall config or clock");

		stall.Lapse();
		LF_Stall state = stall.GetStall();
		if (!state)
		{
			rule = LF_EStallResult.NOT_RENTED;
			return Refuse(LF_EStallTrade.RULE, "the stall is not rented");
		}

		// 1: hold the listing. No cash has moved.
		LF_StallEscrow escrow;
		LF_StallListing listing = state.Find(listingId);
		ResourceName item;
		if (listing)
			item = listing.m_sItem;

		rule = LF_StallRules.Reserve(config, state, listingId, buyerUid, LF_MoneyUtils.GetCash(buyer), day, hour, escrow);
		if (rule != LF_EStallResult.OK)
			return LF_EStallTrade.RULE;

		// 2: the holder must still carry the item.
		string holderUid = state.m_sHolder;
		IEntity holder = FindHolder(holderUid);
		if (!holder)
			return Unwind(state, escrow, LF_EStallTrade.HOLDER_AWAY, false, string.Format("holder %1 is not in the world", holderUid));
		if (LF_InventoryUtils.GetAmount(holder, item) < 1)
			return Unwind(state, escrow, LF_EStallTrade.NO_STOCK, true, string.Format("holder %1 no longer carries %2", holderUid, item));

		// 3: the buyer's cash.
		int taken = LF_MoneyUtils.RemoveAmount(buyer, escrow.m_iAmount, REASON_BUY, holderUid);
		if (taken != escrow.m_iAmount)
		{
			if (taken > 0)
				LF_MoneyUtils.AddCash(buyer, taken, REASON_BUY, holderUid, true);

			return Unwind(state, escrow, LF_EStallTrade.CASH_SHORT, false, string.Format("%1 had %2 of %3 when the cash was taken, refunded", buyerUid, taken, escrow.m_iAmount));
		}

		// 4: the item leaves the holder.
		if (LF_InventoryUtils.RemoveAmount(holder, item, 1) != 1)
		{
			LF_MoneyUtils.AddCash(buyer, taken, REASON_BUY, holderUid, true);
			return Unwind(state, escrow, LF_EStallTrade.NO_STOCK, true, string.Format("holder %1 lost %2 mid-sale, buyer refunded", holderUid, item));
		}

		// 5: the item reaches the buyer, or goes back.
		int handed;
		if (!s_bNoRoomForTest)
			handed = LF_InventoryUtils.AddAmount(buyer, item, 1, false);

		if (handed != 1)
		{
			LF_InventoryUtils.AddAmount(holder, item, 1, true);
			LF_MoneyUtils.AddCash(buyer, taken, REASON_BUY, holderUid, true);
			return Unwind(state, escrow, LF_EStallTrade.BUYER_FULL, false, string.Format("%1 had no room for %2, item and cash returned", buyerUid, item));
		}

		// 6: settle. The item is the buyer's; the seller is paid.
		LF_EStallResult settled = LF_StallRules.Settle(config, state, escrow, sellerPay);
		if (settled != LF_EStallResult.OK)
		{
			// Cannot happen on a held escrow; undo the whole sale rather than leave a free item.
			LF_InventoryUtils.RemoveAmount(buyer, item, 1);
			LF_InventoryUtils.AddAmount(holder, item, 1, true);
			LF_MoneyUtils.AddCash(buyer, taken, REASON_BUY, holderUid, true);
			sellerPay = 0;
			rule = settled;
			LF_Debug.Error(LF_StallConfig.FEATURE, string.Format("stall sale of %1 did not settle (%2): undone", item, settled));
			return LF_EStallTrade.RULE;
		}

		if (sellerPay > 0)
			LF_MoneyUtils.AddCash(holder, sellerPay, REASON_SALE, buyerUid, true);

		paid = taken;
		LF_Debug.Info(LF_StallConfig.FEATURE, string.Format("%1 bought %2 from %3 for %4; seller paid %5", buyerUid, item, holderUid, paid, sellerPay));
		return LF_EStallTrade.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Ends a reserved purchase that fell through: the escrow refunds (its listing opens again) and, when
	//! the holder cannot back the listing any more, the listing is dropped. The caller has already put
	//! cash and item back.
	protected static LF_EStallTrade Unwind(LF_Stall state, LF_StallEscrow escrow, LF_EStallTrade result, bool dropListing, string why)
	{
		int refund;
		LF_EStallResult refunded = LF_StallRules.Refund(state, escrow, refund);
		if (refunded != LF_EStallResult.OK)
			LF_Debug.Error(LF_StallConfig.FEATURE, string.Format("stall escrow did not refund (%1) after: %2", refunded, why));

		if (dropListing)
			LF_StallRules.Delist(state, escrow.m_iListingId);

		return Refuse(result, why);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_StallTrade's statics.
class LF_StallTrade_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_StallTrade";
	}

	override void Apply()
	{
		LF_StallTrade.ResetStatics();
	}
}
