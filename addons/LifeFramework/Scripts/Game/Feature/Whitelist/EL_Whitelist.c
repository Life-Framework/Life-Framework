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

	//------------------------------------------------------------------------------------------------
	bool VerifyPlayer(string uuid)
	{
		PrintFormat("[%1-WHITELIST] Verifying UUID: %2 -> %3", SCR_Enum.GetEnumName(EL_WhitelistType, m_eType), uuid, m_aWhitelistUuids.Contains(uuid).ToString());
		return m_aWhitelistUuids.Contains(uuid);
	}

	//------------------------------------------------------------------------------------------------
	void LoadUuidsFromFile()
	{
		if (!Replication.IsServer())
			return;

		if (!FileIO.FileExists(m_sWhitelistFilePath))
		{
			PrintFormat("[%1-WHITELIST] Whitelist file %2 does not exist", SCR_Enum.GetEnumName(EL_WhitelistType, m_eType), m_sWhitelistFilePath);
			return;
		}

		m_aWhitelistUuids.Clear();
		FileHandle whitelistFile = FileIO.OpenFile(m_sWhitelistFilePath, FileMode.READ);

		string uuid;
		while (whitelistFile.ReadLine(uuid) > 0)
		{
			m_aWhitelistUuids.Insert(uuid);
			PrintFormat("[%1-WHITELIST] Loaded UUID %2", SCR_Enum.GetEnumName(EL_WhitelistType, m_eType), uuid);
		}

		PrintFormat("[%1-WHITELIST] Loaded %2 UUIDs from file", SCR_Enum.GetEnumName(EL_WhitelistType, m_eType), m_aWhitelistUuids.Count());
		whitelistFile.Close();
	}

	//------------------------------------------------------------------------------------------------
	void AddNewUuidToFile(string uuid)
	{
		if (!Replication.IsServer())
			return;

		if (!FileIO.FileExists(m_sWhitelistFilePath))
			return;

		FileHandle whitelistFile = FileIO.OpenFile(m_sWhitelistFilePath, FileMode.APPEND);
		whitelistFile.WriteLine(uuid);
		whitelistFile.Close();

		PrintFormat("[%1-WHITELIST] Added %2 to file", SCR_Enum.GetEnumName(EL_WhitelistType, m_eType), uuid);

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