//------------------------------------------------------------------------------------------------
//! Bit flags a player can hold. CONNECT gates joining, POLICE/MEDIC gate actions.
enum LF_WhitelistType
{
	NONE,
	CONNECT = 1 << 0,
	POLICE = 1 << 1,
	MEDIC = 1 << 2
}

//------------------------------------------------------------------------------------------------
//! One configurable whitelist: a type, a UUID file, and the in-memory copy of that file.
//! Server only: the client never reads the file, it receives flags via LF_PlayerWhitelistComponent.
[BaseContainerProps()]
class LF_Whitelist
{
	[Attribute("", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(LF_WhitelistType))]
	LF_WhitelistType m_eType;

	[Attribute("", UIWidgets.FileNamePicker)]
	string m_sWhitelistFilePath;

	ref array<string> m_aWhitelistUuids = new array<string>();

	//! True once the file load ran and found the file. A whitelist whose file is missing
	//! must not gate joins/actions (fail-safe: degrade, never lock everyone out).
	protected bool m_bLoaded;

	//------------------------------------------------------------------------------------------------
	//! \return True when the UUID file was present and read.
	bool WasLoaded()
	{
		return m_bLoaded;
	}

	//------------------------------------------------------------------------------------------------
	bool VerifyPlayer(string uuid)
	{
		bool matches = m_aWhitelistUuids.Contains(uuid);
		LF_Debug.Log("Whitelist", string.Format("verify uuid=%1 type=%2 match=%3", uuid, SCR_Enum.GetEnumName(LF_WhitelistType, m_eType), matches));
		return matches;
	}

	//------------------------------------------------------------------------------------------------
	//! Read the legacy file into memory. The file is read only; nothing writes it any more (#2575).
	void LoadUuidsFromFile()
	{
		if (!Replication.IsServer())
			return;

		m_bLoaded = ReadUuids(m_sWhitelistFilePath, m_aWhitelistUuids);
		if (!m_bLoaded)
		{
			LF_Debug.Log("Whitelist", string.Format("no whitelist file yet: %1", m_sWhitelistFilePath));
			return;
		}

		LF_Debug.Log("Whitelist", string.Format("loaded %1 uuids from %2", m_aWhitelistUuids.Count(), m_sWhitelistFilePath));
	}

	//------------------------------------------------------------------------------------------------
	//! Read one uid per line, trimmed, blanks and repeats dropped.
	//! \return False when the file is missing or cannot be opened; uids is then empty.
	static bool ReadUuids(string path, notnull array<string> uids)
	{
		uids.Clear();
		if (path.IsEmpty() || !FileIO.FileExists(path))
			return false;

		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (!file)
			return false;

		string uuid;
		while (file.ReadLine(uuid) > 0)
		{
			uuid.TrimInPlace();
			if (!uuid.IsEmpty() && !uids.Contains(uuid))
				uids.Insert(uuid);
		}

		file.Close();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Session only: the legacy file is no longer written (#2575). The roster
	//! (LF_WhitelistManager) is the saved list; this keeps the in-memory copy in step.
	void AddNewUuidToFile(string uuid)
	{
		if (!Replication.IsServer())
			return;

		if (m_aWhitelistUuids.Contains(uuid))
			return;

		m_aWhitelistUuids.Insert(uuid);
		LF_Debug.Log("Whitelist", string.Format("added uuid %1 to the session copy of %2 (legacy file not written)", uuid, m_sWhitelistFilePath));
	}

	//------------------------------------------------------------------------------------------------
	//! Session only: the legacy file stays as it is on disk (#2575).
	void RemoveUUIDFromFile(string uuid)
	{
		if (!Replication.IsServer())
			return;

		if (!m_aWhitelistUuids.Contains(uuid))
			return;

		m_aWhitelistUuids.RemoveItem(uuid);
		LF_Debug.Log("Whitelist", string.Format("removed uuid %1 from the session copy of %2 (legacy file not written)", uuid, m_sWhitelistFilePath));
	}

	//------------------------------------------------------------------------------------------------
	void LF_Whitelist()
	{
		if (GetGame() && GetGame().InPlayMode())
			LoadUuidsFromFile();
	}
}

//------------------------------------------------------------------------------------------------
//! One-time move of a legacy LF_Whitelist file into the LF_WhitelistManager roster (#2575).
//! POLICE uids join the POLICE faction roster. CONNECT and MEDIC have no account roster (there
//! is no medic faction, and the job roster is keyed by character): their uids are counted and
//! logged, never migrated; the file stays on disk for a later roster.
//! The legacy file is only read. A marker line per file records a finished migration, so a uid
//! an admin later removes from the roster is not put back on the next start; repeats and uids
//! already on the roster are skipped either way. A file is marked only after the roster saved,
//! so a refused save is retried on the next start and no entry is lost.
class LF_WhitelistLegacyMigration
{
	static const string MARKER_PATH = "$profile:LifeFramework/whitelist-legacy-migrated.txt";

	//------------------------------------------------------------------------------------------------
	//! \return The number of uids this call added to the roster.
	static int Migrate(LF_WhitelistType type, string path, string markerPath = MARKER_PATH)
	{
		string typeName = typename.EnumToString(LF_WhitelistType, type);
		array<string> uids = {};

		if (type != LF_WhitelistType.POLICE)
		{
			LF_Whitelist.ReadUuids(path, uids);
			LF_Debug.Info("Whitelist", string.Format("legacy %1 list has %2 uids, not migrated (no %1 roster): %3", typeName, uids.Count(), path));
			return 0;
		}

		string key = MarkerKey(type, path);
		if (IsMarked(markerPath, key))
		{
			LF_Debug.Log("Whitelist", string.Format("legacy %1 list already migrated, ignored: %2", typeName, path));
			return 0;
		}

		if (!LF_Whitelist.ReadUuids(path, uids))
		{
			LF_Debug.Warn("Whitelist", string.Format("legacy %1 whitelist file missing, nothing migrated: %2", typeName, path));
			return 0;
		}

		LF_WhitelistManager.Initialize();
		if (LF_WhitelistManager.IsLoadFailed())
		{
			LF_Debug.Error("Whitelist", string.Format("roster unreadable, legacy %1 list not migrated (retried next start): %2", typeName, path));
			return 0;
		}

		int migrated = 0;
		foreach (string uid : uids)
		{
			if (LF_WhitelistManager.AddFactionWhitelist(uid, LF_EFactionType.POLICE))
				migrated++;
		}

		if (!LF_WhitelistManager.Save())
		{
			LF_Debug.Error("Whitelist", string.Format("roster save failed, legacy %1 list not marked migrated (retried next start): %2", typeName, path));
			return migrated;
		}

		if (!Mark(markerPath, key))
			LF_Debug.Error("Whitelist", string.Format("cannot write migration marker %1; the legacy list is read again next start", markerPath));

		LF_Debug.Info("Whitelist", string.Format("migrated %1 legacy %2 uids from %3", migrated, typeName, path));
		return migrated;
	}

	//------------------------------------------------------------------------------------------------
	protected static string MarkerKey(LF_WhitelistType type, string path)
	{
		return typename.EnumToString(LF_WhitelistType, type) + "|" + path;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsMarked(string markerPath, string key)
	{
		array<string> lines = {};
		LF_Whitelist.ReadUuids(markerPath, lines);
		return lines.Contains(key);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Mark(string markerPath, string key)
	{
		string parent = FilePath.StripFileName(markerPath);
		if (!parent.IsEmpty())
			FileIO.MakeDirectory(parent);

		FileMode mode = FileMode.APPEND;
		if (!FileIO.FileExists(markerPath))
			mode = FileMode.WRITE;

		FileHandle file = FileIO.OpenFile(markerPath, mode);
		if (!file)
			return false;

		file.WriteLine(key);
		file.Close();
		return true;
	}
}
