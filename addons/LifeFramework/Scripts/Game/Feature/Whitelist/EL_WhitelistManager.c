//------------------------------------------------------------------------------------------------
enum EL_EFactionType
{
	CIVILIAN,
	POLICE,
	MILITARY,
	MAFIA
}

//------------------------------------------------------------------------------------------------
//! Manages whitelist for restricted factions and jobs.
//! The list is this manager's own file. Version is the first line. Load replaces the
//! cache, so a second load does not duplicate entries. A missing file logs an error and
//! leaves every restricted faction closed. This is not the game-mode serializer.
class EL_WhitelistManager
{
	static const int VERSION = 1;

	protected static ref map<string, ref array<EL_EFactionType>> s_mFactionWhitelist;
	protected static ref map<string, ref array<EL_EJobType>> s_mJobWhitelist;
	protected static string s_sSavePath = "";
	protected static int s_iMissingFileReports;
	protected static int s_iFactionRefusals;

	//------------------------------------------------------------------------------------------------
	//! Load the save the first time the cache is touched. A later call keeps the
	//! cache that Load already built.
	static void Initialize()
	{
		if (s_mFactionWhitelist && s_mJobWhitelist)
			return;

		Load();
	}

	//------------------------------------------------------------------------------------------------
	static int GetMissingFileCount()
	{
		return s_iMissingFileReports;
	}

	//------------------------------------------------------------------------------------------------
	static int GetFactionRefusalCount()
	{
		return s_iFactionRefusals;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests point this at a private file. An empty path uses the profile save.
	static void SetSavePath(string path)
	{
		s_sSavePath = path;
	}

	//------------------------------------------------------------------------------------------------
	//! Drop the in-memory lists. The file stays. The next Load is a restart.
	static void Reset()
	{
		s_mFactionWhitelist = null;
		s_mJobWhitelist = null;
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
		Save();
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
		Save();
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
		Save();
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
		Save();
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

	//------------------------------------------------------------------------------------------------
	//! Civilian is open. A restricted faction whose uid is absent is refused and logged.
	static bool MayJoinFaction(string playerUID, EL_EFactionType factionType)
	{
		if (!IsFactionRestricted(factionType))
			return true;

		if (IsWhitelistedForFaction(playerUID, factionType))
			return true;

		s_iFactionRefusals++;
		EL_Debug.Log("Whitelist", string.Format("faction refused: uid=%1 faction=%2", playerUID, typename.EnumToString(EL_EFactionType, factionType)));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Replace the cache from the versioned file. A second call ends on the same list.
	//! A missing file logs EL_Debug.Error and leaves restricted factions closed.
	static void Load()
	{
		CloseLists();

		string path = GetSavePath();
		if (!FileIO.FileExists(path))
		{
			s_iMissingFileReports++;
			EL_Debug.Error("Whitelist", string.Format("whitelist file missing, police faction closed: %1", path));
			return;
		}

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			EL_Debug.Error("Whitelist", string.Format("cannot open whitelist file, police faction closed: %1", path));
			return;
		}

		string versionLine;
		if (handle.ReadLine(versionLine) <= 0)
		{
			handle.Close();
			EL_Debug.Error("Whitelist", string.Format("whitelist file has no version, police faction closed: %1", path));
			return;
		}

		versionLine.TrimInPlace();
		if (versionLine.ToInt() != VERSION)
		{
			handle.Close();
			EL_Debug.Error("Whitelist", string.Format("whitelist file version %1 rejected, police faction closed: %2", versionLine, path));
			return;
		}

		ref map<string, ref array<EL_EFactionType>> loadedFactions = new map<string, ref array<EL_EFactionType>>();
		ref map<string, ref array<EL_EJobType>> loadedJobs = new map<string, ref array<EL_EJobType>>();
		if (!ReadFactionBlock(handle, loadedFactions) || !ReadJobBlock(handle, loadedJobs))
		{
			handle.Close();
			EL_Debug.Error("Whitelist", string.Format("whitelist file is unreadable, police faction closed: %1", path));
			return;
		}

		handle.Close();
		s_mFactionWhitelist = loadedFactions;
		s_mJobWhitelist = loadedJobs;
		EL_Debug.Log("Whitelist", string.Format("loaded whitelist version=%1 path=%2", VERSION, path));
	}

	//------------------------------------------------------------------------------------------------
	//! Write version, then the faction entries, then the job entries.
	static void Save()
	{
		if (!s_mFactionWhitelist)
			s_mFactionWhitelist = new map<string, ref array<EL_EFactionType>>();

		if (!s_mJobWhitelist)
			s_mJobWhitelist = new map<string, ref array<EL_EJobType>>();

		string path = GetSavePath();
		string parent = FilePath.StripFileName(path);
		if (!parent.IsEmpty())
			FileIO.MakeDirectory(parent);

		FileHandle handle = FileIO.OpenFile(path, FileMode.WRITE);
		if (!handle)
		{
			EL_Debug.Error("Whitelist", string.Format("cannot open whitelist file for save: %1", path));
			return;
		}

		handle.WriteLine(VERSION.ToString());
		WriteFactionBlock(handle);
		WriteJobBlock(handle);
		handle.Close();
		EL_Debug.Log("Whitelist", string.Format("saved whitelist version=%1 path=%2", VERSION, path));
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetSavePath()
	{
		if (s_sSavePath.IsEmpty())
			return "$profile:LifeFramework/whitelist.txt";

		return s_sSavePath;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CloseLists()
	{
		if (!s_mFactionWhitelist)
			s_mFactionWhitelist = new map<string, ref array<EL_EFactionType>>();
		else
			s_mFactionWhitelist.Clear();

		if (!s_mJobWhitelist)
			s_mJobWhitelist = new map<string, ref array<EL_EJobType>>();
		else
			s_mJobWhitelist.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static void WriteFactionBlock(FileHandle handle)
	{
		handle.WriteLine(CountFactionEntries().ToString());
		if (!s_mFactionWhitelist)
			return;

		foreach (string playerUID, array<EL_EFactionType> factions : s_mFactionWhitelist)
		{
			if (playerUID.IsEmpty() || !factions)
				continue;

			foreach (EL_EFactionType factionType : factions)
			{
				handle.WriteLine(playerUID);
				handle.WriteLine(typename.EnumToString(EL_EFactionType, factionType));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void WriteJobBlock(FileHandle handle)
	{
		handle.WriteLine(CountJobEntries().ToString());
		if (!s_mJobWhitelist)
			return;

		foreach (string playerUID, array<EL_EJobType> jobs : s_mJobWhitelist)
		{
			if (playerUID.IsEmpty() || !jobs)
				continue;

			foreach (EL_EJobType jobType : jobs)
			{
				handle.WriteLine(playerUID);
				handle.WriteLine(typename.EnumToString(EL_EJobType, jobType));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static int CountFactionEntries()
	{
		int count = 0;
		if (!s_mFactionWhitelist)
			return 0;

		foreach (string playerUID, array<EL_EFactionType> factions : s_mFactionWhitelist)
		{
			if (playerUID.IsEmpty() || !factions)
				continue;

			count += factions.Count();
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static int CountJobEntries()
	{
		int count = 0;
		if (!s_mJobWhitelist)
			return 0;

		foreach (string playerUID, array<EL_EJobType> jobs : s_mJobWhitelist)
		{
			if (playerUID.IsEmpty() || !jobs)
				continue;

			count += jobs.Count();
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ReadFactionBlock(FileHandle handle, map<string, ref array<EL_EFactionType>> target)
	{
		string countLine;
		if (handle.ReadLine(countLine) <= 0)
			return false;

		countLine.TrimInPlace();
		int count = countLine.ToInt();
		if (count < 0)
			return false;

		for (int i = 0; i < count; i++)
		{
			string playerUID;
			string factionName;
			if (handle.ReadLine(playerUID) <= 0)
				return false;
			if (handle.ReadLine(factionName) <= 0)
				return false;

			playerUID.TrimInPlace();
			factionName.TrimInPlace();
			if (playerUID.IsEmpty())
				return false;

			EL_EFactionType factionType = typename.StringToEnum(EL_EFactionType, factionName);
			if (typename.EnumToString(EL_EFactionType, factionType) != factionName)
				return false;

			array<EL_EFactionType> playerFactions = target.Get(playerUID);
			if (!playerFactions)
			{
				playerFactions = new array<EL_EFactionType>();
				target.Set(playerUID, playerFactions);
			}

			if (!playerFactions.Contains(factionType))
				playerFactions.Insert(factionType);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool ReadJobBlock(FileHandle handle, map<string, ref array<EL_EJobType>> target)
	{
		string countLine;
		if (handle.ReadLine(countLine) <= 0)
			return false;

		countLine.TrimInPlace();
		int count = countLine.ToInt();
		if (count < 0)
			return false;

		for (int i = 0; i < count; i++)
		{
			string playerUID;
			string jobName;
			if (handle.ReadLine(playerUID) <= 0)
				return false;
			if (handle.ReadLine(jobName) <= 0)
				return false;

			playerUID.TrimInPlace();
			jobName.TrimInPlace();
			if (playerUID.IsEmpty())
				return false;

			EL_EJobType jobType = typename.StringToEnum(EL_EJobType, jobName);
			if (typename.EnumToString(EL_EJobType, jobType) != jobName)
				return false;

			array<EL_EJobType> playerJobs = target.Get(playerUID);
			if (!playerJobs)
			{
				playerJobs = new array<EL_EJobType>();
				target.Set(playerUID, playerJobs);
			}

			if (!playerJobs.Contains(jobType))
				playerJobs.Insert(jobType);
		}

		return true;
	}
}
