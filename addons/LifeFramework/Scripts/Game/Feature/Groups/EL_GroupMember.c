//------------------------------------------------------------------------------------------------
//! Faction member rank
enum EL_EFactionRank
{
	RECRUIT = 0,
	MEMBER = 1,
	OFFICER = 2,
	LEADER = 3
}

//------------------------------------------------------------------------------------------------
//! Faction type
enum EL_EFactionType
{
	CIVILIAN = 0,
	GANG = 1,
	CORPORATION = 2,
	GOVERNMENT = 3,
	EMERGENCY_SERVICES = 4,
	MILITIA = 5,
	// Whitelist-only factions
	POLICE = 100,
	MILITARY = 101,
	MAFIA = 102
}

//------------------------------------------------------------------------------------------------
//! Faction member data
class EL_FactionMember
{
	string m_sPlayerUID;
	string m_sPlayerName;
	EL_EFactionRank m_eRank;
	float m_fJoinTimestamp;
	
	//------------------------------------------------------------------------------------------------
	void EL_FactionMember(string uid, string name, EL_EFactionRank rank = EL_EFactionRank.RECRUIT)
	{
		m_sPlayerUID = uid;
		m_sPlayerName = name;
		m_eRank = rank;
		m_fJoinTimestamp = System.GetTickCount();
	}
	
	//------------------------------------------------------------------------------------------------
	bool CanManageMembers()
	{
		return m_eRank >= EL_EFactionRank.OFFICER;
	}
	
	//------------------------------------------------------------------------------------------------
	bool CanManageRanks()
	{
		return m_eRank >= EL_EFactionRank.OFFICER;
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsLeader()
	{
		return m_eRank == EL_EFactionRank.LEADER;
	}
	
	//------------------------------------------------------------------------------------------------
	string GetRankName()
	{
		return typename.EnumToString(EL_EFactionRank, m_eRank);
	}
}
