//------------------------------------------------------------------------------------------------
//! Persisted form of a player account and its characters.
//!
//! A DEDICATED RECORD, NOT EL_PlayerAccount ITSELF. The live class is the mutable session cache
//! whose shape may change with gameplay work; a save format must not. Explicit records also make
//! the field ORDER visible in one place, which is what the binary save context keys off.
//!
//! FIELDS MIRROR EL_PlayerAccount: character list, active character index, faction, on-duty and
//! wanted level. EL_PlayerCharacterRecord mirrors EL_PlayerCharacter (id, prefab, first/last name,
//! age). The account's own key (m_sPersistentId) is the player's Steam UID.
//------------------------------------------------------------------------------------------------
class EL_PlayerAccountRecord
{
	string m_sPersistentId;
	ref array<ref EL_PlayerCharacterRecord> m_aCharacters = {};
	int m_iActiveCharacterIdx;
	EL_Faction m_eFaction = EL_Faction.CIVILIAN;
	bool m_bOnDuty = false;
	int m_iWantedLevel = 0;

	//------------------------------------------------------------------------------------------------
	//! \param[in] persistentId Player Steam UID this account belongs to.
	//! \return A blank account record carrying only the key.
	static EL_PlayerAccountRecord Create(string persistentId)
	{
		EL_PlayerAccountRecord record();
		record.m_sPersistentId = persistentId;
		return record;
	}
}

//------------------------------------------------------------------------------------------------
//! Persisted form of one character of an account.
//------------------------------------------------------------------------------------------------
class EL_PlayerCharacterRecord
{
	string m_sId;
	ResourceName m_rPrefab;
	string m_sFirstName;
	string m_sLastName;
	int m_iAge;

	//------------------------------------------------------------------------------------------------
	//! \param[in] id Character persistence id (PersistenceIdUtils generated UUID).
	//! \param[in] prefab Character prefab to spawn.
	//! \param[in] firstName Character first name.
	//! \param[in] lastName Character last name.
	//! \param[in] age Character age.
	//! \return A fully populated character record.
	static EL_PlayerCharacterRecord Create(string id, ResourceName prefab, string firstName, string lastName, int age)
	{
		EL_PlayerCharacterRecord record();
		record.m_sId = id;
		record.m_rPrefab = prefab;
		record.m_sFirstName = firstName;
		record.m_sLastName = lastName;
		record.m_iAge = age;
		return record;
	}
}