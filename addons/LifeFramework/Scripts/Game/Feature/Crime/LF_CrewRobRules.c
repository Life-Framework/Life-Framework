//------------------------------------------------------------------------------------------------
//! The numbers of a crew robbery (Configs/Crime/LF_CrewRob.conf, #3401). Content, not a server knob:
//! core ships neutral values.
[BaseContainerProps(configRoot: true)]
class LF_CrewRobConfig
{
	static const string FEATURE = "CrewRob";
	static const ResourceName CONFIG_PATH = "{7A41C9E02B58D63F}Configs/Crime/LF_CrewRob.conf";

	[Attribute("2", UIWidgets.Auto, "Fewest crew members (holder included) a crew robbery needs to start", params: "1 8")]
	int m_iMinCrew;

	[Attribute("5", UIWidgets.Auto, "Cash added to the take per second while the holder stands on the till spot", params: "0 100000")]
	int m_iTakePerSecond;

	[Attribute("1500", UIWidgets.Auto, "The take never grows past this", params: "1 10000000")]
	int m_iTakeCap;

	[Attribute("90", UIWidgets.Auto, "Seconds from the alarm until the police response window closes", params: "1 3600")]
	float m_fResponseWindowSeconds;

	[Attribute("120", UIWidgets.Auto, "Seconds the take may stay the same (holder off the spot, not holding, or at the cap) before the job ends unpaid and the till is free; 0 turns the time-out off", params: "0 3600")]
	float m_fIdleTimeoutSeconds;

	protected static ref LF_CrewRobConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded table, or a default instance (logged once) when the core file does not resolve.
	static LF_CrewRobConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (container && container.GetResource())
			s_Config = LF_CrewRobConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, string.Format("crew robbery table does not load, built-in defaults apply: %1", CONFIG_PATH));
			s_Config = new LF_CrewRobConfig();
		}

		return s_Config;
	}
}

//------------------------------------------------------------------------------------------------
//! The rules of a crew robbery as pure functions: no world, no clock. The caller ticks the
//! spot-seconds with the time that passed and whether the holder stands on the till spot.
//! LOGIC-tested (LF_Test_CrewRobRules).
class LF_CrewRobRules
{
	//------------------------------------------------------------------------------------------------
	//! \return True when \p crewCount (holder and lookouts together) reaches \p minCrew.
	static bool CrewMeets(int crewCount, int minCrew)
	{
		return crewCount >= minCrew;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds that count toward the take after one tick: they grow only while the holder is on the
	//! spot and stay put off it. A zero or negative step counts as none.
	static float AdvanceSpotSeconds(float spotSeconds, float dtSeconds, bool holderOnSpot)
	{
		if (!holderOnSpot || dtSeconds <= 0)
			return spotSeconds;

		return spotSeconds + dtSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! The take after \p spotSeconds on the spot at \p perSecond, whole cash, held at \p cap.
	static int Take(float spotSeconds, int perSecond, int cap)
	{
		if (spotSeconds <= 0 || perSecond <= 0 || cap <= 0)
			return 0;

		float raw = spotSeconds * perSecond; // lf-float: whole cash of a bounded product, compared to the cap below
		if (raw >= cap)
			return cap;

		return raw;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the police response window is open: from the alarm for \p windowSeconds.
	//! No alarm yet (\p alarmMs < 0) is not open; the window starts with the alarm, not the robbery.
	static bool ResponseWindowOpen(int nowMs, int alarmMs, float windowSeconds)
	{
		if (alarmMs < 0 || nowMs < alarmMs)
			return false;

		float elapsedMs = nowMs - alarmMs; // lf-float: a short span in ms, compared to the window
		return elapsedMs < windowSeconds * 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Splits \p total evenly across \p crewPresent members in int math: the remainder goes one
	//! cash each to the first members, so 301 among 3 is 101, 100, 100. Empty for no crew or no pay.
	static void Split(int total, int crewPresent, notnull array<int> shares)
	{
		shares.Clear();
		if (crewPresent <= 0 || total <= 0)
			return;

		int each = total / crewPresent;
		int extra = total - each * crewPresent;
		for (int i = 0; i < crewPresent; i++)
		{
			if (i < extra)
				shares.Insert(each + 1);
			else
				shares.Insert(each);
		}
	}
}
