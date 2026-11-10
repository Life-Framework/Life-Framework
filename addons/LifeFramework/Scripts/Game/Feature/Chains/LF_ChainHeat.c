//------------------------------------------------------------------------------------------------
//! Heat arithmetic for a chain site. Pure: the site (LF_ChainSite, #417) owns the number, the
//! trace (#418) reads it. No heat setting on the chain means no trace at all.
class LF_ChainHeat
{
	//------------------------------------------------------------------------------------------------
	//! \return Heat after workUnits of hands-on work, capped at the chain's maximum.
	static float AfterWork(LF_ChainHeatDef def, float heat, int workUnits)
	{
		if (!def || workUnits < 1)
			return heat;

		return Math.Min(heat + def.m_fPerWork * workUnits, def.m_fMax);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Heat after seconds of nobody working, never below 0.
	static float AfterDecay(LF_ChainHeatDef def, float heat, float seconds)
	{
		if (!def || seconds <= 0)
			return heat;

		return Math.Max(heat - def.m_fDecayPerSecond * seconds, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Metres the site can be smelled from at this heat: the chain range scaled by heat / max.
	static float RangeAt(LF_ChainHeatDef def, float heat)
	{
		if (!def || def.m_fMax <= 0 || heat <= 0)
			return 0;

		return def.m_fRangeM * Math.Min(heat / def.m_fMax, 1);
	}
}
