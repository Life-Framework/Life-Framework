//------------------------------------------------------------------------------------------------
//! What one ore rock has taken since it last grew back. The server keeps one per rock.
class LF_OreYieldState
{
	//! Pickaxe damage toward the next piece.
	float m_fDamage;
	//! Pieces broken off since the rock last grew back.
	int m_iBreaks;
	bool m_bDepleted;
}

//------------------------------------------------------------------------------------------------
//! Ore rock arithmetic, kept apart from the entity so a LOGIC test drives it.
//! Pickaxe damage piles up; every `damagePerBreak` breaks one piece off. After
//! `maxBreaks` pieces the rock is spent until it grows back. 0 means it never runs out.
class LF_OreYield
{
	static const float DEFAULT_RESPAWN_MS = 300000;

	//------------------------------------------------------------------------------------------------
	//! One accepted hit. Returns how many pieces it broke off (usually 0 or 1).
	static int Strike(LF_OreYieldState state, float damage, float damagePerBreak, int maxBreaks)
	{
		if (!state || state.m_bDepleted)
			return 0;

		if (damage <= 0 || damagePerBreak <= 0)
			return 0;

		state.m_fDamage = state.m_fDamage + damage;
		int breaks = 0;
		while (state.m_fDamage >= damagePerBreak)
		{
			state.m_fDamage = state.m_fDamage - damagePerBreak;
			breaks++;
			state.m_iBreaks++;
			if (maxBreaks > 0 && state.m_iBreaks >= maxBreaks)
			{
				state.m_bDepleted = true;
				state.m_fDamage = 0;
				break;
			}
		}

		return breaks;
	}

	//------------------------------------------------------------------------------------------------
	//! Items that drop for `breaks` pieces when each piece yields `yieldPerBreak`.
	static int ItemsFor(int breaks, int yieldPerBreak)
	{
		if (breaks <= 0 || yieldPerBreak <= 0)
			return 0;

		return breaks * yieldPerBreak;
	}

	//------------------------------------------------------------------------------------------------
	//! Grow-back delay. The node row's cooldown wins; a row with none uses the
	//! rock's own seconds, and a rock with none uses five minutes.
	static float RespawnDelayMs(float rowCooldownMs, float fallbackSeconds)
	{
		if (rowCooldownMs > 0)
			return rowCooldownMs;

		if (fallbackSeconds > 0)
			return fallbackSeconds * 1000;

		return DEFAULT_RESPAWN_MS;
	}

	//------------------------------------------------------------------------------------------------
	//! The rock grows back whole.
	static void Refill(LF_OreYieldState state)
	{
		if (!state)
			return;

		state.m_fDamage = 0;
		state.m_iBreaks = 0;
		state.m_bDepleted = false;
	}
}
