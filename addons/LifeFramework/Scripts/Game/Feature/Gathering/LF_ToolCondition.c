//------------------------------------------------------------------------------------------------
//! The state of one gathering tool (WK-W7, #4182): a tier (worn, standard, fine) and a condition
//! from 0 to 100 that drops a little with every use. A worn-out tool still works, only slower, and
//! the action text says how it is doing in words. Pure rules plus one small state object; the gather
//! action, the repair bench and the save section read it later.
class LF_ToolCondition
{
	static const int TIER_WORN = 0;
	static const int TIER_STANDARD = 1;
	static const int TIER_FINE = 2; // lf-fixed: an enum value, not a fine amount

	static const int STATE_GOOD = 0;
	static const int STATE_WORN = 1;
	static const int STATE_POOR = 2;
	static const int STATE_WORN_OUT = 3;

	static const float FULL = 100.0; // lf-fixed: the condition scale; 100 is a tool as sold
	static const float GOOD_FROM = 60.0; // lf-fixed: at or above this the tool works at full speed and reads "good"
	static const float WORN_FROM = 25.0; // lf-fixed: below GOOD_FROM down to here reads "getting worn"
	static const float SLOWEST = 0.5; // lf-fixed: speed factor of a worn-out tool, which still works

	int m_iTier = TIER_STANDARD;
	float m_fCondition = FULL;

	//------------------------------------------------------------------------------------------------
	//! A tool as sold: full condition at the given tier.
	static LF_ToolCondition Create(int tier)
	{
		LF_ToolCondition tool = new LF_ToolCondition();
		tool.m_iTier = ClampTier(tier);
		tool.m_fCondition = FULL;
		return tool;
	}

	//------------------------------------------------------------------------------------------------
	static int ClampTier(int tier)
	{
		if (tier < TIER_WORN)
			return TIER_WORN;
		if (tier > TIER_FINE)
			return TIER_FINE;
		return tier;
	}

	//------------------------------------------------------------------------------------------------
	//! Condition lost per use at a tier: a worn tool wears fastest, a fine one slowest.
	static float WearPerUse(int tier)
	{
		int clamped = ClampTier(tier);
		if (clamped == TIER_WORN)
			return 2.0; // lf-fixed: wear table, tuned in the W36 economy pass
		if (clamped == TIER_FINE)
			return 0.5; // lf-fixed: wear table
		return 1.0; // lf-fixed: wear table
	}

	//------------------------------------------------------------------------------------------------
	//! One use: the condition after a use at this tier, never below 0.
	static float AfterUse(float condition, int tier)
	{
		float left = condition - WearPerUse(tier);
		if (left < 0)
			return 0;
		return left;
	}

	//------------------------------------------------------------------------------------------------
	//! Speed factor of a tool in this condition: 1 while it is good, falling to SLOWEST at 0. Never 0,
	//! so a worn-out tool still works.
	static float SpeedFactor(float condition)
	{
		if (condition >= GOOD_FROM)
			return 1.0;
		float clamped = condition;
		if (clamped < 0)
			clamped = 0;
		return SLOWEST + (1.0 - SLOWEST) * clamped / GOOD_FROM;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds one hold takes with a tool of this speed factor in this condition.
	static float Duration(float baseSeconds, float toolSpeed, float condition)
	{
		return LF_GatherTools.Duration(baseSeconds, toolSpeed * SpeedFactor(condition));
	}

	//------------------------------------------------------------------------------------------------
	//! The wear state a condition reads as.
	static int StateOf(float condition)
	{
		if (condition >= GOOD_FROM)
			return STATE_GOOD;
		if (condition >= WORN_FROM)
			return STATE_WORN;
		if (condition > 0)
			return STATE_POOR;
		return STATE_WORN_OUT;
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key for the action text at a condition. %1 is the tool name; words, no numbers.
	static string StateKey(float condition)
	{
		int state = StateOf(condition);
		if (state == STATE_GOOD)
			return "#LF-ToolCondition_Good";
		if (state == STATE_WORN)
			return "#LF-ToolCondition_Worn";
		if (state == STATE_POOR)
			return "#LF-ToolCondition_Poor";
		return "#LF-ToolCondition_WornOut";
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key naming a tier: words, for the shop, the bench and the tool's name line.
	static string TierKey(int tier)
	{
		int clamped = ClampTier(tier);
		if (clamped == TIER_WORN)
			return "#LF-ToolTier_Worn";
		if (clamped == TIER_FINE)
			return "#LF-ToolTier_Fine";
		return "#LF-ToolTier_Standard";
	}

	//------------------------------------------------------------------------------------------------
	//! One use of this tool. Returns the condition after it.
	float Use()
	{
		m_fCondition = AfterUse(m_fCondition, m_iTier);
		return m_fCondition;
	}

	//------------------------------------------------------------------------------------------------
	float Speed()
	{
		return SpeedFactor(m_fCondition);
	}

	//------------------------------------------------------------------------------------------------
	string Key()
	{
		return StateKey(m_fCondition);
	}
}
