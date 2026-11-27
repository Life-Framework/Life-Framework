// red-proof: with `RedProof()` on the garage config is never changed, so the three ladders all read the shipped answer
// and the gap, floor and equal-cap checks fail.

//! garage/max-buyable: LF_GarageSlotStore.MaxBuyable, the most slots a character can buy (max slots minus the free base
//! slots). It is the gap between the two config numbers, and never negative: a pack that sets the base above the max must
//! leave nothing for sale, not a negative count that a slot-buy check could read as "always room".
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_GarageMaxBuyable : LF_Test
{
	override string GetName()
	{
		return "garage/max-buyable";
	}

	//------------------------------------------------------------------------------------------------
	protected int Buyable(int baseSlots, int maxSlots)
	{
		LF_GarageConfig config = LF_GarageConfig.Get();
		if (!config)
			return -99;

		config.m_iBaseSlots = baseSlots;
		config.m_iMaxSlots = maxSlots;
		return LF_GarageSlotStore.MaxBuyable();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int gap = -50;
		int equal = -50;
		int inverted = -50;
		int none = -50;
		int capInverted = -50;
		int priceInverted = -50;
		if (!RedProof())
		{
			gap = Buyable(2, 6);
			equal = Buyable(3, 3);
			none = Buyable(0, 0);
			inverted = Buyable(5, 2);
			capInverted = LF_GarageSlots.CapOf("garage-max-buyable-nobody");
			priceInverted = LF_GarageSlots.NextPriceFor("garage-max-buyable-nobody");
			LF_GarageConfig.Reload();
		}

		ctx.Equal(4, gap, "base 2, max 6: four slots for sale");
		ctx.Equal(0, equal, "base equal to max: nothing for sale");
		ctx.Equal(0, inverted, "base above max: nothing for sale, never a negative count");
		ctx.Equal(0, none, "no slots at all: nothing for sale");
		ctx.Equal(2, capInverted, "base above max: the cap is the max");
		ctx.Equal(-1, priceInverted, "base above max: there is no next slot price");
	}
}
