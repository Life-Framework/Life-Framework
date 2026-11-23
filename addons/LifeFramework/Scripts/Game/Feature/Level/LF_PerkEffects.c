//------------------------------------------------------------------------------------------------
//! What the passive trade perks do to the work (WK-W11, #4408). A perk a character picked in a trade
//! (LF_TradePerks.Has, #4186) changes only that trade's work:
//!  - `double_yield`: a chance that a gather pays double.
//!  - `swift_hands`:  a shorter gather hold.
//!  - `light_touch`:  less wear on the tool per use (LF_ToolWear).
//! The shares are one pick each, inside the caps of the work plan (double yield 20%, speed 20%, wear
//! -40%), and the double-yield chance is the income lever, so it sits at 10% and keeps the duty-pay
//! ladder (civilian trades above police). The buyer-price perk and the full-perks economy line are
//! WK-W11_2. Yield and wear are server state; the roll runs on the server.
class LF_PerkEffects
{
	static const string FEATURE = "PerkEffects";

	static const string PERK_DOUBLE_YIELD = "double_yield";
	static const string PERK_SWIFT_HANDS = "swift_hands";
	static const string PERK_LIGHT_TOUCH = "light_touch";

	static const float DOUBLE_YIELD_CHANCE = 0.10; // lf-fixed: share of gathers that pay double with the perk (cap 0.20)
	static const float SWIFT_SHARE = 0.20; // lf-fixed: share of the hold swift hands removes (cap 0.20)
	static const float LIGHT_SHARE = 0.40; // lf-fixed: share of the wear light touch removes (cap 0.40)

	//------------------------------------------------------------------------------------------------
	//! True when the character holds this perk in this trade. A missing character or trade holds none.
	static bool Holds(string playerUid, string trade, string perk)
	{
		if (playerUid.IsEmpty() || trade.IsEmpty())
			return false;

		return LF_TradePerks.Has(playerUid, trade, perk);
	}

	//------------------------------------------------------------------------------------------------
	//! Chance (0 to 1) that one gather pays double.
	static float DoubleChance(string playerUid, string trade)
	{
		if (Holds(playerUid, trade, PERK_DOUBLE_YIELD))
			return DOUBLE_YIELD_CHANCE;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Units one gather pays given a roll from 0 to 1: double when the roll is under the chance. Pure
	//! in the roll, so a test forces a win or a loss.
	static int Yield(string playerUid, string trade, int baseAmount, float roll)
	{
		if (baseAmount <= 0)
			return baseAmount;

		if (roll < DoubleChance(playerUid, trade))
		{
			LF_Debug.Info(FEATURE, string.Format("%1 double yield in %2: %3 -> %4", playerUid, trade, baseAmount, baseAmount * 2));
			return baseAmount * 2;
		}

		return baseAmount;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the units one gather pays, with a fresh roll.
	static int RollYield(string playerUid, string trade, int baseAmount)
	{
		return Yield(playerUid, trade, baseAmount, Math.RandomFloat01());
	}

	//------------------------------------------------------------------------------------------------
	//! Share (0 to 1) of the hold the character's perks remove.
	static float HoldShare(string playerUid, string trade)
	{
		if (Holds(playerUid, trade, PERK_SWIFT_HANDS))
			return SWIFT_SHARE;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds one gather hold takes with the perks: the base, shorter by the share.
	static float HoldSeconds(string playerUid, string trade, float baseSeconds)
	{
		return baseSeconds * (1.0 - HoldShare(playerUid, trade));
	}

	//------------------------------------------------------------------------------------------------
	//! The hold as a speed factor (1 with no perk): the engine adds the time slice times this to the
	//! progress, so 1 / (1 - share) finishes the hold in `HoldSeconds`.
	static float SpeedFactor(string playerUid, string trade)
	{
		return 1.0 / (1.0 - HoldShare(playerUid, trade));
	}

	//------------------------------------------------------------------------------------------------
	//! Share of the tool's wear per use that is left after the perks: 1 with no perk.
	static float WearScale(string playerUid, string trade)
	{
		if (Holds(playerUid, trade, PERK_LIGHT_TOUCH))
			return 1.0 - LIGHT_SHARE;

		return 1.0;
	}
}
