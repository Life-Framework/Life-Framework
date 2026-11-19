//------------------------------------------------------------------------------------------------
//! Why a hive refused a taking. OK gives the combs.
enum LF_EHiveTake
{
	OK,
	EMPTY,
	NOT_OWNER
}

//------------------------------------------------------------------------------------------------
//! A beehive's tuning (#3446): game hours per comb, the cap, and the night window. Core ships the
//! neutral defaults in Configs/Farming/LF_Hives.conf.
[BaseContainerProps(configRoot: true)]
class LF_HiveConfig
{
	static const ResourceName CONFIG_PATH = "{C5E1A07B3D294F18}Configs/Farming/LF_Hives.conf";
	static const string FEATURE = "Farming";

	[Attribute("6", UIWidgets.Auto, "Game hours of fair day weather for one comb", params: "0.1 240")]
	float m_fHoursPerComb;

	[Attribute("4", UIWidgets.Auto, "Combs a hive holds. A full hive gains nothing", params: "1 100")]
	int m_iMaxCombs;

	[Attribute("20", UIWidgets.Auto, "Game hour of the day the bees stop working", params: "0 24")]
	float m_fNightStartHour;

	[Attribute("6", UIWidgets.Auto, "Game hour of the day the bees start again", params: "0 24")]
	float m_fNightEndHour;

	protected static ref LF_HiveConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_HiveConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("hive config does not resolve, hives will not grow: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_HiveConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(FEATURE, string.Format("hive config has an unexpected root class: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached config; the next Get loads the file again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}
}

//------------------------------------------------------------------------------------------------
//! One hive's state: combs inside and game hours of work toward the next. Pure data; no entity.
class LF_HiveState
{
	int m_iCombs;
	float m_fProgressHours;
}

//------------------------------------------------------------------------------------------------
//! The beehive rules (#3446): pure logic with no entity, world or clock. The caller feeds game hours
//! (the game clock, never real time) and the facts of the moment.
class LF_HiveRules
{
	//------------------------------------------------------------------------------------------------
	//! Is this game hour of the day inside the night window. The window may wrap midnight (20 to 6).
	static bool IsNight(float hourOfDay, float nightStart, float nightEnd)
	{
		if (nightStart == nightEnd)
			return false;

		if (nightStart < nightEnd)
			return hourOfDay >= nightStart && hourOfDay < nightEnd;

		return hourOfDay >= nightStart || hourOfDay < nightEnd;
	}

	//------------------------------------------------------------------------------------------------
	//! Works the hive for this many game hours. Rain or night gains nothing; a full hive gains nothing
	//! and keeps no progress. \return Combs gained.
	static int Advance(LF_HiveState state, float hoursPerComb, int maxCombs, float hours, bool raining, bool night)
	{
		if (!state || hours <= 0 || hoursPerComb <= 0)
			return 0;

		if (raining || night)
			return 0;

		if (state.m_iCombs >= maxCombs)
		{
			state.m_fProgressHours = 0;
			return 0;
		}

		int gained = 0;
		state.m_fProgressHours += hours;
		while (state.m_fProgressHours >= hoursPerComb && state.m_iCombs < maxCombs)
		{
			state.m_fProgressHours -= hoursPerComb;
			state.m_iCombs++;
			gained++;
		}

		if (state.m_iCombs >= maxCombs)
			state.m_fProgressHours = 0;

		return gained;
	}

	//------------------------------------------------------------------------------------------------
	//! May this person use the hive. An open hive (ownerOnly false) takes anyone; an owner-only hive
	//! takes only its owner key.
	static bool MayUse(bool ownerOnly, string ownerKey, string actorKey)
	{
		if (!ownerOnly)
			return true;

		return !ownerKey.IsEmpty() && ownerKey == actorKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes every comb out. A stranger at an owner-only hive is refused and the hive keeps its combs;
	//! an empty hive refuses with EMPTY. Taking leaves the work toward the next comb as it was.
	//! \param[out] taken Combs handed out, 0 on a refusal.
	static LF_EHiveTake Take(LF_HiveState state, bool ownerOnly, string ownerKey, string actorKey, out int taken)
	{
		taken = 0;
		if (!state)
			return LF_EHiveTake.EMPTY;

		if (!MayUse(ownerOnly, ownerKey, actorKey))
			return LF_EHiveTake.NOT_OWNER;

		if (state.m_iCombs <= 0)
			return LF_EHiveTake.EMPTY;

		taken = state.m_iCombs;
		state.m_iCombs = 0;
		return LF_EHiveTake.OK;
	}
}
