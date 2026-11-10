//------------------------------------------------------------------------------------------------
//! One site: the stations that share a site id, and the heat their work gives off.
class LF_ChainSiteState : Managed
{
	string m_sSiteId;
	string m_sChainId;
	vector m_vOrigin;
	//! Heat as of m_fAtSeconds, before the decay since then.
	float m_fHeat;
	float m_fAtSeconds;
	int m_iStations;
}

//------------------------------------------------------------------------------------------------
//! Per-site heat (#417), read by the smell and smoke trace (#418). Server only. Heat rises with
//! accepted hands-on work and fades with time; it is computed when read, so a quiet site costs
//! nothing. Heat is not saved: a restart starts every site cold, which is the right answer for
//! a trace that fades in minutes.
class LF_ChainSites
{
	static const string FEATURE = "Chains";

	protected static ref map<string, ref LF_ChainSiteState> s_mSites;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_ChainSiteState> Table()
	{
		if (!s_mSites)
			s_mSites = new map<string, ref LF_ChainSiteState>();

		return s_mSites;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mSites = null;
	}

	//------------------------------------------------------------------------------------------------
	//! A station announces itself so the site knows its chain and where it is. The first station
	//! to register sets the site origin.
	static void Register(string siteId, string chainId, vector origin)
	{
		if (siteId.IsEmpty())
			return;

		LF_ChainSiteState site = Table().Get(siteId);
		if (!site)
		{
			site = new LF_ChainSiteState();
			site.m_sSiteId = siteId;
			site.m_sChainId = chainId;
			site.m_vOrigin = origin;
			Table().Set(siteId, site);
		}
		else if (site.m_sChainId != chainId)
		{
			LF_Debug.Warn(FEATURE, string.Format("site %1 mixes chains %2 and %3; heat follows %2", siteId, site.m_sChainId, chainId));
		}

		site.m_iStations = site.m_iStations + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets a site (a test that registered its own).
	static void Remove(string siteId)
	{
		if (s_mSites)
			s_mSites.Remove(siteId);
	}

	//------------------------------------------------------------------------------------------------
	static LF_ChainSiteState Find(string siteId)
	{
		if (siteId.IsEmpty())
			return null;

		return Table().Get(siteId);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ChainHeatDef HeatDef(LF_ChainSiteState site)
	{
		LF_ChainDef chain = LF_ChainCatalog.FindChain(site.m_sChainId);
		if (!chain)
			return null;

		return chain.m_Heat;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: workUnits of accepted work at the site. \return The heat after the work.
	static float AddWork(string siteId, int workUnits, float nowSeconds)
	{
		LF_ChainSiteState site = Find(siteId);
		if (!site)
			return 0;

		LF_ChainHeatDef def = HeatDef(site);
		if (!def)
			return 0;

		float heat = LF_ChainHeat.AfterDecay(def, site.m_fHeat, nowSeconds - site.m_fAtSeconds);
		site.m_fHeat = LF_ChainHeat.AfterWork(def, heat, workUnits);
		site.m_fAtSeconds = nowSeconds;
		return site.m_fHeat;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a site goes cold at once (an admin reset, or a test cleaning up after itself).
	static void ClearHeat(string siteId)
	{
		LF_ChainSiteState site = Find(siteId);
		if (!site)
			return;

		site.m_fHeat = 0;
		site.m_fAtSeconds = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The site's heat now, after the fade since the last work. 0 for an unknown site or
	//! a chain with no heat settings.
	static float GetHeat(string siteId, float nowSeconds)
	{
		LF_ChainSiteState site = Find(siteId);
		if (!site)
			return 0;

		return LF_ChainHeat.AfterDecay(HeatDef(site), site.m_fHeat, nowSeconds - site.m_fAtSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Metres the site can be smelled from right now.
	static float GetRange(string siteId, float nowSeconds)
	{
		LF_ChainSiteState site = Find(siteId);
		if (!site)
			return 0;

		return LF_ChainHeat.RangeAt(HeatDef(site), GetHeat(siteId, nowSeconds));
	}

	//------------------------------------------------------------------------------------------------
	//! Every site with any heat left, for the trace (#418) to walk.
	static void GetWarmSites(float nowSeconds, notnull array<LF_ChainSiteState> outSites)
	{
		outSites.Clear();
		foreach (string siteId, LF_ChainSiteState site : Table())
		{
			if (site && GetHeat(siteId, nowSeconds) > 0)
				outSites.Insert(site);
		}
	}
}
