// red-proof: with `RedProof()` on the wire helpers are not asked, so no faction order, slot or packed value is read and the
// order, range, clamp and refusal checks fail.

//! reputation/wire-range: the packed reputation a client receives (LF_ReputationSync). Town, police and underworld sit in slots
//! 0, 1 and 2 in that order and the order list agrees with the slots, an unknown or empty faction has no slot, the smallest and
//! largest tier the wire holds (-16 and +15) round trip, a tier outside that range is held at the nearest end instead of
//! spilling into the next faction's digit, and a value below the flag or at twice the flag is not a packed value at all.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ReputationWireRange : LF_Test
{
	override string GetName()
	{
		return "reputation/wire-range";
	}

	protected static int TierOf(int packed, string faction)
	{
		int tier = -999;
		if (!LF_ReputationSync.Unpack(packed, faction, tier))
			return -999;

		return tier;
	}

	override void Run(LF_TestContext ctx)
	{
		int count = -1;
		string first = "-";
		string second = "-";
		string third = "-";
		int slotTown = -9;
		int slotPolice = -9;
		int slotUnderworld = -9;
		int slotEmpty = 9;
		int slotUnknown = 9;
		int lowTown = 0;
		int highPolice = 0;
		int lowUnderworld = 0;
		int clampedHigh = 0;
		int clampedLow = 0;
		int neighbourAfterClamp = 0;
		bool belowFlag = true;
		bool atTwiceFlag = true;
		bool atFlag = false;
		if (!RedProof())
		{
			array<string> order = {};
			LF_ReputationSync.FactionOrder(order);
			count = order.Count();
			if (count == 3)
			{
				first = order[0];
				second = order[1];
				third = order[2];
			}

			slotTown = LF_ReputationSync.Slot(LF_Reputation.FACTION_TOWN);
			slotPolice = LF_ReputationSync.Slot(LF_Reputation.FACTION_POLICE);
			slotUnderworld = LF_ReputationSync.Slot(LF_Reputation.FACTION_UNDERWORLD);
			slotEmpty = LF_ReputationSync.Slot("");
			slotUnknown = LF_ReputationSync.Slot("NO_SUCH_FACTION");

			int edges = LF_ReputationSync.Pack(-LF_ReputationSync.TIER_OFFSET, LF_ReputationSync.TIER_BASE - 1 - LF_ReputationSync.TIER_OFFSET, -LF_ReputationSync.TIER_OFFSET);
			lowTown = TierOf(edges, LF_Reputation.FACTION_TOWN);
			highPolice = TierOf(edges, LF_Reputation.FACTION_POLICE);
			lowUnderworld = TierOf(edges, LF_Reputation.FACTION_UNDERWORLD);

			int high = LF_ReputationSync.Pack(500, 0, 0);
			clampedHigh = TierOf(high, LF_Reputation.FACTION_TOWN);
			neighbourAfterClamp = TierOf(high, LF_Reputation.FACTION_POLICE);
			int low = LF_ReputationSync.Pack(-500, 0, 0);
			clampedLow = TierOf(low, LF_Reputation.FACTION_TOWN);

			int none = -1;
			belowFlag = LF_ReputationSync.Unpack(LF_ReputationSync.PACKED_FLAG - 1, LF_Reputation.FACTION_TOWN, none);
			atTwiceFlag = LF_ReputationSync.Unpack(LF_ReputationSync.PACKED_FLAG * 2, LF_Reputation.FACTION_TOWN, none);
			atFlag = LF_ReputationSync.Unpack(LF_ReputationSync.PACKED_FLAG, LF_Reputation.FACTION_TOWN, none);
		}

		int highest = LF_ReputationSync.TIER_BASE - 1 - LF_ReputationSync.TIER_OFFSET;
		ctx.Equal(3, count, "three factions travel on the wire");
		ctx.EqualStr(LF_Reputation.FACTION_TOWN, first, "town is first");
		ctx.EqualStr(LF_Reputation.FACTION_POLICE, second, "police is second");
		ctx.EqualStr(LF_Reputation.FACTION_UNDERWORLD, third, "underworld is third");
		ctx.Equal(0, slotTown, "town is slot 0");
		ctx.Equal(1, slotPolice, "police is slot 1");
		ctx.Equal(2, slotUnderworld, "underworld is slot 2");
		ctx.Equal(-1, slotEmpty, "an empty faction has no slot");
		ctx.Equal(-1, slotUnknown, "an unknown faction has no slot");
		ctx.Equal(-LF_ReputationSync.TIER_OFFSET, lowTown, "the lowest tier on the wire round trips");
		ctx.Equal(highest, highPolice, "the highest tier on the wire round trips");
		ctx.Equal(-LF_ReputationSync.TIER_OFFSET, lowUnderworld, "the lowest tier round trips in the last slot too");
		ctx.Equal(highest, clampedHigh, "a tier above the range is held at the highest");
		ctx.Equal(0, neighbourAfterClamp, "an out-of-range town tier does not spill into the police digit");
		ctx.Equal(-LF_ReputationSync.TIER_OFFSET, clampedLow, "a tier below the range is held at the lowest");
		ctx.False(belowFlag, "a value below the flag was never packed");
		ctx.False(atTwiceFlag, "twice the flag is outside the packed range");
		ctx.True(atFlag, "the flag itself is the smallest packed value (the lowest tier in every slot)");
	}
}
