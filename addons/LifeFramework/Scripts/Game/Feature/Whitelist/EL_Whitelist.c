//------------------------------------------------------------------------------------------------
//! Bit flags a player can hold. CONNECT gates joining, POLICE/MEDIC gate actions.
enum EL_WhitelistType
{
	NONE,
	CONNECT = 1 << 0,
	POLICE = 1 << 1,
	MEDIC = 1 << 2
}

//------------------------------------------------------------------------------------------------
//! One configurable whitelist: a type, a UUID file, and the in-memory copy of that file.
//! Server only: the client never reads the file, it receives flags via EL_PlayerWhitelistComponent.
[BaseContainerProps()]
class EL_Whitelist
{
	[Attribute("", UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(EL_WhitelistType))]
	EL_WhitelistType m_eType;

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
		EL_Debug.Log("Whitelist", string.Format("verify uuid=%1 type=%2 match=%3", uuid, SCR_Enum.GetEnumName(EL_WhitelistType, m_eType), matches));
		return matches;
	}

	//------------------------------------------------------------------------------------------------
	void LoadUuidsFromFile()
	{
		if (!Replication.IsServer())
			return;

		if (!FileIO.FileExists(m_sWhitelistFilePath))
		{
			EL_Debug.Log("Whitelist", string.Format("no whitelist file yet: %1", m_sWhitelistFilePath));
			m_bLoaded = false;
			return;
		}

		m_aWhitelistUuids.Clear();
		FileHandle whitelistFile = FileIO.OpenFile(m_sWhitelistFilePath, FileMode.READ);

		string uuid;
		while (whitelistFile.ReadLine(uuid) > 0)
		{
			uuid.TrimInPlace();
			if (!uuid.IsEmpty())
				m_aWhitelistUuids.Insert(uuid);
		}

		whitelistFile.Close();
		m_bLoaded = true;
		EL_Debug.Log("Whitelist", string.Format("loaded %1 uuids from %2", m_aWhitelistUuids.Count(), m_sWhitelistFilePath));
	}

	//------------------------------------------------------------------------------------------------
	void AddNewUuidToFile(string uuid)
	{
		if (!Replication.IsServer())
			return;

		// Create the file on first grant: a fresh server has no whitelist file, and
		// silently dropping the first grant makes the whitelist unusable.
		if (!FileIO.FileExists(m_sWhitelistFilePath))
			FileIO.OpenFile(m_sWhitelistFilePath, FileMode.WRITE).Close();

		FileHandle whitelistFile = FileIO.OpenFile(m_sWhitelistFilePath, FileMode.APPEND);
		if (!whitelistFile)
		{
			EL_Debug.Error("Whitelist", string.Format("cannot open whitelist file for append: %1", m_sWhitelistFilePath));
			return;
		}

		whitelistFile.WriteLine(uuid);
		whitelistFile.Close();

		EL_Debug.Log("Whitelist", string.Format("added uuid %1 to %2", uuid, m_sWhitelistFilePath));

		m_aWhitelistUuids.Insert(uuid);
	}

	//------------------------------------------------------------------------------------------------
	void RemoveUUIDFromFile(string uuid)
	{
		if (!Replication.IsServer())
			return;

		if (!m_aWhitelistUuids.Contains(uuid))
			return;
		m_aWhitelistUuids.RemoveItem(uuid);

		FileIO.DeleteFile(m_sWhitelistFilePath);
		FileHandle whitelistFile = FileIO.OpenFile(m_sWhitelistFilePath, FileMode.WRITE);
		foreach (string uid : m_aWhitelistUuids)
		{
			whitelistFile.WriteLine(uid);
		}
		whitelistFile.Close();

		PrintFormat("[%1-WHITELIST] Removed %2 from file", SCR_Enum.GetEnumName(EL_WhitelistType, m_eType), uuid);
	}

	//------------------------------------------------------------------------------------------------
	void EL_Whitelist()
	{
		if (GetGame() && GetGame().InPlayMode())
			LoadUuidsFromFile();
	}
}