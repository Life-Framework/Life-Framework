//------------------------------------------------------------------------------------------------
enum EL_EFactionType
{
	CIVILIAN,
	POLICE,
	MILITARY,
	MAFIA
}

//------------------------------------------------------------------------------------------------
//! Manages whitelist for restricted factions and jobs
class EL_WhitelistManager
{
	protected static ref map<string, ref array<EL_EFactionType>> s_mFactionWhitelist;
	protected static ref map<string, ref array<EL_EJobType>> s_mJobWhitelist;
	
	//------------------------------------------------------------------------------------------------
	//! Initialize whitelist system
	static void Initialize()
	{
		if (!s_mFactionWhitelist)
			s_mFactionWhitelist = new map<string, ref array<EL_EFactionType>>();
		
		if (!s_mJobWhitelist)
			s_mJobWhitelist = new map<string, ref array<EL_EJobType>>();
		
		// TODO: Load from config file or database
		Print("[EL_WhitelistManager] Initialized", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if faction type requires whitelist
	static bool IsFactionRestricted(EL_EFactionType factionType)
	{
		switch (factionType)
		{
			case EL_EFactionType.POLICE:
			case EL_EFactionType.MILITARY:
			case EL_EFactionType.MAFIA:
				return true;
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if job type requires whitelist
	static bool IsJobRestricted(EL_EJobType jobType)
	{
		// Solo POLICE requiere whitelist
		return jobType == EL_EJobType.POLICE;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if player is whitelisted for faction
	static bool IsWhitelistedForFaction(string playerUID, EL_EFactionType factionType)
	{
		if (!IsFactionRestricted(factionType))
			return true; // Not restricted, always allowed
		
		if (!s_mFactionWhitelist)
			Initialize();
		
		array<EL_EFactionType> playerFactions = s_mFactionWhitelist.Get(playerUID);
		if (!playerFactions)
			return false;
		
		return playerFactions.Contains(factionType);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if player is whitelisted for job
	static bool IsWhitelistedForJob(string playerUID, EL_EJobType jobType)
	{
		if (!IsJobRestricted(jobType))
			return true; // Not restricted, always allowed
		
		if (!s_mJobWhitelist)
			Initialize();
		
		array<EL_EJobType> playerJobs = s_mJobWhitelist.Get(playerUID);
		if (!playerJobs)
			return false;
		
		return playerJobs.Contains(jobType);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Add player to faction whitelist (admin command)
	static bool AddFactionWhitelist(string playerUID, EL_EFactionType factionType)
	{
		if (!s_mFactionWhitelist)
			Initialize();
		
		array<EL_EFactionType> playerFactions = s_mFactionWhitelist.Get(playerUID);
		if (!playerFactions)
		{
			playerFactions = new array<EL_EFactionType>();
			s_mFactionWhitelist.Set(playerUID, playerFactions);
		}
		
		if (playerFactions.Contains(factionType))
			return false; // Already whitelisted
		
		playerFactions.Insert(factionType);
		Print(string.Format("[EL_WhitelistManager] Added faction whitelist: %1 -> %2", playerUID, factionType), LogLevel.NORMAL);
		
		// TODO: Save to database
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Add player to job whitelist (admin command)
	static bool AddJobWhitelist(string playerUID, EL_EJobType jobType)
	{
		if (!s_mJobWhitelist)
			Initialize();
		
		array<EL_EJobType> playerJobs = s_mJobWhitelist.Get(playerUID);
		if (!playerJobs)
		{
			playerJobs = new array<EL_EJobType>();
			s_mJobWhitelist.Set(playerUID, playerJobs);
		}
		
		if (playerJobs.Contains(jobType))
			return false; // Already whitelisted
		
		playerJobs.Insert(jobType);
		Print(string.Format("[EL_WhitelistManager] Added job whitelist: %1 -> %2", playerUID, jobType), LogLevel.NORMAL);
		
		// TODO: Save to database
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Remove player from faction whitelist (admin command)
	static bool RemoveFactionWhitelist(string playerUID, EL_EFactionType factionType)
	{
		if (!s_mFactionWhitelist)
			return false;
		
		array<EL_EFactionType> playerFactions = s_mFactionWhitelist.Get(playerUID);
		if (!playerFactions)
			return false;
		
		int index = playerFactions.Find(factionType);
		if (index == -1)
			return false;
		
		playerFactions.Remove(index);
		Print(string.Format("[EL_WhitelistManager] Removed faction whitelist: %1 -> %2", playerUID, factionType), LogLevel.NORMAL);
		
		// TODO: Save to database
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Remove player from job whitelist (admin command)
	static bool RemoveJobWhitelist(string playerUID, EL_EJobType jobType)
	{
		if (!s_mJobWhitelist)
			return false;
		
		array<EL_EJobType> playerJobs = s_mJobWhitelist.Get(playerUID);
		if (!playerJobs)
			return false;
		
		int index = playerJobs.Find(jobType);
		if (index == -1)
			return false;
		
		playerJobs.Remove(index);
		Print(string.Format("[EL_WhitelistManager] Removed job whitelist: %1 -> %2", playerUID, jobType), LogLevel.NORMAL);
		
		// TODO: Save to database
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get all whitelisted factions for player
	static array<EL_EFactionType> GetWhitelistedFactions(string playerUID)
	{
		if (!s_mFactionWhitelist)
			Initialize();
		
		array<EL_EFactionType> playerFactions = s_mFactionWhitelist.Get(playerUID);
		if (!playerFactions)
			return new array<EL_EFactionType>();
		
		return playerFactions;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get all whitelisted jobs for player
	static array<EL_EJobType> GetWhitelistedJobs(string playerUID)
	{
		if (!s_mJobWhitelist)
			Initialize();
		
		array<EL_EJobType> playerJobs = s_mJobWhitelist.Get(playerUID);
		if (!playerJobs)
			return new array<EL_EJobType>();
		
		return playerJobs;
	}
}
