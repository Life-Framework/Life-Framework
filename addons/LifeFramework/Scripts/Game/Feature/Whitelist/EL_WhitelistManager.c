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

	//! True after Load found a whitelist file it could not read. Save refuses while set, so
	//! the next admin edit cannot overwrite the unread file with only the new entry.
	protected static bool s_bLoadFailed;

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
	//! \return True when the last Load found a file it could not read. Saving is refused until a
	//!         Load succeeds (fix or remove the file, then restart).
	static bool IsLoadFailed()
	{
		return s_bLoadFailed;
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
	//! How many players the roster holds for a faction.
	static int CountFaction(EL_EFactionType factionType)
	{
		if (!s_mFactionWhitelist)
			Initialize();

		int count = 0;
		foreach (string uid, array<EL_EFactionType> factions : s_mFactionWhitelist)
		{
			if (factions && factions.Contains(factionType))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the first player to join a playtest host whose police roster is empty goes on it.
	static bool ShouldAddFirstPolice(bool missionAllows, int policeCount, string uid)
	{
		return missionAllows && policeCount == 0 && !uid.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Server, on join: LF_MissionHeader.m_bLF_FirstPlayerPolice puts the first player on an empty
	//! police roster (the maintainer on his own playtest host). Off on real servers.
	static bool AddFirstPlayerToPolice(string uid)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		bool allows = header && header.m_bLF_FirstPlayerPolice;
		if (!ShouldAddFirstPolice(allows, CountFaction(EL_EFactionType.POLICE), uid))
			return false;

		AddFactionWhitelist(uid, EL_EFactionType.POLICE);
		EL_Debug.Info("Whitelist", string.Format("first player %1 put on the empty police roster (playtest mission)", uid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
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
		EL_Debug.Log("Whitelist", string.Format("Added faction whitelist: %1 -> %2", playerUID, factionType));
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
		EL_Debug.Log("Whitelist", string.Format("Added job whitelist: %1 -> %2", playerUID, jobType));
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
		EL_Debug.Log("Whitelist", string.Format("Removed faction whitelist: %1 -> %2", playerUID, factionType));
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
		EL_Debug.Log("Whitelist", string.Format("Removed job whitelist: %1 -> %2", playerUID, jobType));
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
	//! A present file that cannot be read sets the load-failed flag, which blocks Save().
	static void Load()
	{
		CloseLists();
		s_bLoadFailed = false;

		string path = GetSavePath();
		if (!FileIO.FileExists(path))
		{
			// No file is a first run, not a failure: there is nothing on disk to protect.
			s_iMissingFileReports++;
			EL_Debug.Error("Whitelist", string.Format("whitelist file missing, police faction closed: %1", path));
			return;
		}

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			s_bLoadFailed = true;
			EL_Debug.Error("Whitelist", string.Format("cannot open whitelist file, police faction closed, saving disabled: %1", path));
			return;
		}

		string versionLine;
		if (handle.ReadLine(versionLine) <= 0)
		{
			handle.Close();
			s_bLoadFailed = true;
			EL_Debug.Error("Whitelist", string.Format("whitelist file has no version, police faction closed, saving disabled: %1", path));
			return;
		}

		versionLine.TrimInPlace();
		if (versionLine.ToInt() != VERSION)
		{
			handle.Close();
			s_bLoadFailed = true;
			EL_Debug.Error("Whitelist", string.Format("whitelist file version %1 rejected, police faction closed, saving disabled: %2", versionLine, path));
			return;
		}

		ref map<string, ref array<EL_EFactionType>> loadedFactions = new map<string, ref array<EL_EFactionType>>();
		ref map<string, ref array<EL_EJobType>> loadedJobs = new map<string, ref array<EL_EJobType>>();
		if (!ReadFactionBlock(handle, loadedFactions) || !ReadJobBlock(handle, loadedJobs))
		{
			handle.Close();
			s_bLoadFailed = true;
			EL_Debug.Error("Whitelist", string.Format("whitelist file is unreadable, police faction closed, saving disabled: %1", path));
			return;
		}

		handle.Close();
		s_mFactionWhitelist = loadedFactions;
		s_mJobWhitelist = loadedJobs;
		EL_Debug.Log("Whitelist", string.Format("loaded whitelist version=%1 path=%2", VERSION, path));
	}

	//------------------------------------------------------------------------------------------------
	//! Write version, then the faction entries, then the job entries.
	//! Refused while the last Load could not read the file: the in-memory list is then only
	//! this session's edits, and writing it would replace every entry the file still holds.
	//! \return True when the file was written.
	static bool Save()
	{
		if (s_bLoadFailed)
		{
			EL_Debug.Error("Whitelist", string.Format("save refused: the whitelist file could not be read at load, it is kept as is: %1", GetSavePath()));
			return false;
		}

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
			return false;
		}

		handle.WriteLine(VERSION.ToString());
		WriteFactionBlock(handle);
		WriteJobBlock(handle);
		handle.Close();
		EL_Debug.Log("Whitelist", string.Format("saved whitelist version=%1 path=%2", VERSION, path));
		return true;
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
