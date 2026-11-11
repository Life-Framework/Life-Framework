//------------------------------------------------------------------------------------------------
//! Persisted form of a player account and its characters.
//!
//! A DEDICATED RECORD, NOT LF_PlayerAccount ITSELF. The live class is the mutable session cache
//! whose shape may change with gameplay work; a save format must not. Explicit records also make
//! the field ORDER visible in one place, which is what the binary save context keys off.
//!
//! FIELDS MIRROR LF_PlayerAccount: character list, active character index, faction, on-duty and
//! wanted level. LF_PlayerCharacterRecord mirrors LF_PlayerCharacter (id, prefab, first/last name,
//! age). The account's own key (m_sPersistentId) is the player's Steam UID.
//------------------------------------------------------------------------------------------------
class LF_PlayerAccountRecord
{
	string m_sPersistentId;
	ref array<ref LF_PlayerCharacterRecord> m_aCharacters = {};
	int m_iActiveCharacterIdx;
	LF_Faction m_eFaction = LF_Faction.CIVILIAN;
	bool m_bOnDuty = false;
	int m_iWantedLevel = 0;

	//------------------------------------------------------------------------------------------------
	//! \param[in] persistentId Player Steam UID this account belongs to.
	//! \return A blank account record carrying only the key.
	static LF_PlayerAccountRecord Create(string persistentId)
	{
		LF_PlayerAccountRecord record();
		record.m_sPersistentId = persistentId;
		return record;
	}
}

//------------------------------------------------------------------------------------------------
//! Persisted form of one character of an account.
//------------------------------------------------------------------------------------------------
class LF_PlayerCharacterRecord
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
	static LF_PlayerCharacterRecord Create(string id, ResourceName prefab, string firstName, string lastName, int age)
	{
		LF_PlayerCharacterRecord record();
		record.m_sId = id;
		record.m_rPrefab = prefab;
		record.m_sFirstName = firstName;
		record.m_sLastName = lastName;
		record.m_iAge = age;
		return record;
	}
}
//------------------------------------------------------------------------------------------------
//! One character's row in the "characters" payload (save version 24, #331): the record key its
//! character-scoped stores use and its wanted level. Matched to the account prefix by account uid
//! and character id. A save before version 24 has no rows; LF_PlayerAccountManager.ApplyAll then
//! keys the account's one character (slot 1) by the account uid, which is what its records use.
//------------------------------------------------------------------------------------------------
class LF_CharacterSlotRecord
{
	string m_sAccountUid;
	string m_sCharacterId;
	string m_sRecordKey;
	int m_iWantedLevel;

	//------------------------------------------------------------------------------------------------
	static LF_CharacterSlotRecord Create(string accountUid, string characterId, string recordKey, int wantedLevel)
	{
		LF_CharacterSlotRecord record();
		record.m_sAccountUid = accountUid;
		record.m_sCharacterId = characterId;
		record.m_sRecordKey = recordKey;
		record.m_iWantedLevel = wantedLevel;
		return record;
	}
}
