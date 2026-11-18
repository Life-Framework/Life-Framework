//------------------------------------------------------------------------------------------------
//! Result of one work tick on a step that can go wrong (#2854).
enum LF_EChainMistake
{
	NONE,
	STRIKE,
	RUINED
}

//------------------------------------------------------------------------------------------------
//! The hidden strike count one batch carries. Session-only: a restart forgives it, like the batch
//! progress itself. The station (#2855) holds one per batch.
class LF_ChainStrikes
{
	int m_iStrikes;
	int m_iTicks;
}

//------------------------------------------------------------------------------------------------
//! Pure rules for cook mistakes on the approved chains (moonshine, tobacco, counterfeit papers).
//! A step's mistakeChancePct is the chance per work tick of a strike; maxMistakes strikes ruin the
//! batch, and 0 means the step can never fail. The chance is seeded, so a seed gives a repeatable
//! sequence. No statics, so there is nothing to reset.
class LF_ChainMistakes
{
	//------------------------------------------------------------------------------------------------
	//! \return A repeatable roll 0..99 for this seed and tick. Every product stays under 2^31.
	static int Roll(int seed, int tick)
	{
		int s = Math.AbsInt(seed) % 10007;
		int t = Math.AbsInt(tick) % 10007;
		int x = (s * 7919 + t * 104729 + 13) % 1000003;
		x = (x * 997 + 31) % 1000003;
		x = (x * 991 + t) % 1000003;
		x = (x * 983 + s) % 1000003;
		return (x / 7) % 100;
	}

	//------------------------------------------------------------------------------------------------
	//! One hands-on work tick. Adds a strike when the roll lands under chancePct; reports RUINED
	//! when strikes reach maxMistakes. The caller drops the batch then and calls Reset.
	static LF_EChainMistake Tick(LF_ChainStrikes state, int chancePct, int maxMistakes, int seed)
	{
		if (!state)
			return LF_EChainMistake.NONE;

		int tick = state.m_iTicks;
		state.m_iTicks = tick + 1;

		if (maxMistakes < 1 || chancePct < 1)
			return LF_EChainMistake.NONE;

		if (Roll(seed, tick) >= chancePct)
			return LF_EChainMistake.NONE;

		state.m_iStrikes = state.m_iStrikes + 1;
		if (state.m_iStrikes >= maxMistakes)
			return LF_EChainMistake.RUINED;

		return LF_EChainMistake.STRIKE;
	}

	//------------------------------------------------------------------------------------------------
	//! A completed or ruined batch starts clean.
	static void Reset(LF_ChainStrikes state)
	{
		if (!state)
			return;

		state.m_iStrikes = 0;
		state.m_iTicks = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return 0 clean, 1 off-colour smoke, 2 about to blow (one strike from ruin).
	static int CueLevel(LF_ChainStrikes state, int maxMistakes)
	{
		if (!state || maxMistakes < 1 || state.m_iStrikes < 1)
			return 0;

		if (state.m_iStrikes >= maxMistakes - 1)
			return 2;

		return 1;
	}
}
