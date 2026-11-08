//! Named spawn location for the second stage of the faction selection menu.
//! A location is a display name plus a position; at spawn time the faction's
//! spawn point closest to the position wins.
class EL_SpawnLocation
{
	string m_sKey;
	string m_sNameKey;
	EL_Faction m_eFaction;
	vector m_vPosition;

	//------------------------------------------------------------------------------------------------
	void EL_SpawnLocation(string key, string nameKey, EL_Faction faction, string position)
	{
		m_sKey = key;
		m_sNameKey = nameKey;
		m_eFaction = faction;
		m_vPosition = position.ToVector();
	}
}

//------------------------------------------------------------------------------------------------
//! Static registry of the locations offered per faction. Session data only; the
//! DebugWorld spawn points sit next to these positions.
class EL_SpawnLocations
{
	protected static ref array<ref EL_SpawnLocation> s_aLocations;

	//------------------------------------------------------------------------------------------------
	static array<ref EL_SpawnLocation> GetAll()
	{
		if (!s_aLocations)
		{
			s_aLocations = new array<ref EL_SpawnLocation>();
			s_aLocations.Insert(new EL_SpawnLocation("town", "EL-SpawnLoc_Town", EL_Faction.CIVILIAN, "126 0 246"));
			s_aLocations.Insert(new EL_SpawnLocation("mine", "EL-SpawnLoc_Mine", EL_Faction.CIVILIAN, "170 0 199"));
			s_aLocations.Insert(new EL_SpawnLocation("station", "EL-SpawnLoc_Station", EL_Faction.POLICE, "232.4 0 245.5"));
			s_aLocations.Insert(new EL_SpawnLocation("outpost", "EL-SpawnLoc_Outpost", EL_Faction.POLICE, "234 0 245.5"));
		}
		return s_aLocations;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref EL_SpawnLocation> GetForFaction(EL_Faction faction)
	{
		array<ref EL_SpawnLocation> result = new array<ref EL_SpawnLocation>();
		foreach (EL_SpawnLocation location : GetAll())
		{
			if (location.m_eFaction == faction)
				result.Insert(location);
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	static EL_SpawnLocation GetByKey(string key)
	{
		foreach (EL_SpawnLocation location : GetAll())
		{
			if (location.m_sKey == key)
				return location;
		}
		return null;
	}
}
