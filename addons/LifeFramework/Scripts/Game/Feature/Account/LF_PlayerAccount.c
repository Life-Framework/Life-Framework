enum LF_Faction
{
	CIVILIAN,
	POLICE,
	MEDIC
}

class LF_PlayerAccount : Managed
{
	/*protected*/ string m_sPersistentId;
	/*protected*/ ref array<ref LF_PlayerCharacter> m_aCharacters = {};
	/*protected*/ int m_iActiveCharacterIdx;
	/*protected*/ LF_Faction m_eFaction = LF_Faction.CIVILIAN;
	/*protected*/ bool m_bOnDuty = false;
	/*protected*/ bool m_bDutyClockRunning = false;
	/*protected*/ int m_iDutyStartedAtMs = 0;
	protected bool m_bArmoryDrawnThisShift = false;
	//! Wanted level, police rank and duty totals belong to a character (#331). This copy holds them
	//! only while the account has no character; the first character adopts it (AddCharacter).
	protected ref LF_CharacterStanding m_AccountStanding = new LF_CharacterStanding();

	//------------------------------------------------------------------------------------------------
	string GetPersistentId()
	{
		return m_sPersistentId;
	}

	//------------------------------------------------------------------------------------------------
	void SetPersistentId(string id)
	{
		m_sPersistentId = id;
	}

	//------------------------------------------------------------------------------------------------
	//! The standing (wanted, rank, duty totals) of the active character, or the account's own copy
	//! while it has no character yet. Never null.
	LF_CharacterStanding GetStanding()
	{
		LF_PlayerCharacter active = GetActiveCharacter();
		if (active)
			return active.GetStanding();

		return m_AccountStanding;
	}

	//------------------------------------------------------------------------------------------------
	//! The standing saved under this record key: the character holding the key, or the account's own
	//! copy for its uid while it has no character. Null when the key is not this account's.
	LF_CharacterStanding GetStandingFor(string recordKey)
	{
		LF_PlayerCharacter character = GetCharacterByKey(recordKey);
		if (character)
			return character.GetStanding();

		if (m_aCharacters.IsEmpty() && recordKey == m_sPersistentId)
			return m_AccountStanding;

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The key every character-scoped record of the active character is saved under (bank,
	//! progression, wanted reports, tickets, ...): its record key, or the account uid while the
	//! account has no character. Slot 1 of an account keeps the uid (see LF_PlayerCharacter).
	string GetActiveCharacterKey()
	{
		LF_PlayerCharacter active = GetActiveCharacter();
		if (active)
			return active.GetRecordKey();

		return m_sPersistentId;
	}

	//------------------------------------------------------------------------------------------------
	//! The current role, not a pick: POLICE only while clocked in at the duty desk (LF_DutyAction,
	//! LF_DutyLocker), CIVILIAN otherwise. There is no faction screen.
	void SetFaction(LF_Faction faction)
	{
		m_eFaction = faction;
		LF_AccountReplication.Publish(this);
	}

	//------------------------------------------------------------------------------------------------
	LF_Faction GetFaction()
	{
		return m_eFaction;
	}

	//------------------------------------------------------------------------------------------------
	void SetOnDuty(bool onDuty)
	{
		m_bOnDuty = onDuty;
		LF_AccountReplication.Publish(this);
	}

	//------------------------------------------------------------------------------------------------
	bool IsOnDuty()
	{
		return m_bOnDuty;
	}

	//------------------------------------------------------------------------------------------------
	//! Police rank index of the active character. 0 is the starting rank. Saved by the progression
	//! payload under the character's record key.
	int GetPoliceRank()
	{
		return GetStanding().m_iPoliceRank;
	}

	//------------------------------------------------------------------------------------------------
	void SetPoliceRank(int rank)
	{
		GetStanding().m_iPoliceRank = rank;
		LF_AccountReplication.Publish(this);
	}

	//------------------------------------------------------------------------------------------------
	int GetOnDutySeconds()
	{
		return GetStanding().m_iOnDutySeconds;
	}

	//------------------------------------------------------------------------------------------------
	void AddOnDutySeconds(int seconds)
	{
		if (seconds <= 0)
			return;

		LF_CharacterStanding standing = GetStanding();
		standing.m_iOnDutySeconds = standing.m_iOnDutySeconds + seconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam (LF_Progression): the saved total. Negative is read as 0.
	void SetOnDutySeconds(int seconds)
	{
		GetStanding().m_iOnDutySeconds = Math.Max(0, seconds);
	}

	//------------------------------------------------------------------------------------------------
	bool IsDutyClockRunning()
	{
		return m_bDutyClockRunning;
	}

	//------------------------------------------------------------------------------------------------
	void StartDutyClock(int nowMs)
	{
		m_bDutyClockRunning = true;
		m_iDutyStartedAtMs = nowMs;
	}

	//------------------------------------------------------------------------------------------------
	void StopDutyClock()
	{
		m_bDutyClockRunning = false;
		m_iDutyStartedAtMs = 0;
	}

	//------------------------------------------------------------------------------------------------
	int GetDutyStartedAtMs()
	{
		return m_iDutyStartedAtMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Duty seconds settled this shift and not yet paid. Saved with the progression payload
	//! (save version 7) together with the carried cash, so a rollback rolls both back.
	int GetUnpaidDutySeconds()
	{
		return GetStanding().m_iUnpaidDutySeconds;
	}

	//------------------------------------------------------------------------------------------------
	void AddUnpaidDutySeconds(int seconds)
	{
		if (seconds <= 0)
			return;

		LF_CharacterStanding standing = GetStanding();
		standing.m_iUnpaidDutySeconds = standing.m_iUnpaidDutySeconds + seconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam (LF_Progression): the saved unpaid seconds. Negative is read as 0.
	void SetUnpaidDutySeconds(int seconds)
	{
		GetStanding().m_iUnpaidDutySeconds = Math.Max(0, seconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the unpaid duty seconds and zeroes them, so a second take returns 0.
	int TakeUnpaidDutySeconds()
	{
		LF_CharacterStanding standing = GetStanding();
		int seconds = standing.m_iUnpaidDutySeconds;
		standing.m_iUnpaidDutySeconds = 0;
		return seconds;
	}

	//------------------------------------------------------------------------------------------------
	//! True once the officer drew the armory kit this duty shift. Session state only: the account
	//! save record does not carry it, so a restart allows one more draw.
	bool HasDrawnArmoryThisShift()
	{
		return m_bArmoryDrawnThisShift;
	}

	//------------------------------------------------------------------------------------------------
	void SetArmoryDrawnThisShift(bool drawn)
	{
		m_bArmoryDrawnThisShift = drawn;
	}

	//------------------------------------------------------------------------------------------------
	void SetWantedLevel(int level)
	{
		LF_CharacterStanding standing = GetStanding();
		int oldLevel = standing.m_iWantedLevel;
		standing.m_iWantedLevel = Math.Clamp(level, 0, 5); // Max 5

		// Wanted cleared (jail, fine, admin): the posters for this person come down.
		if (standing.m_iWantedLevel == 0)
			LF_WantedReports.ClearFor(GetActiveCharacterKey());

		LF_AccountReplication.Publish(this);
		// The wanted board's face shows the level (LF_WantedPosters).
		if (oldLevel != standing.m_iWantedLevel && Replication.IsServer())
			LF_EventBus.Publish(LF_OnWantedChanged.Create(GetActiveCharacterKey(), oldLevel, standing.m_iWantedLevel));
	}

	//------------------------------------------------------------------------------------------------
	int GetWantedLevel()
	{
		return GetStanding().m_iWantedLevel;
	}

	//------------------------------------------------------------------------------------------------
	void IncreaseWantedLevel(int amount = 1)
	{
		SetWantedLevel(GetWantedLevel() + amount);
	}

	//------------------------------------------------------------------------------------------------
	//! A character without a record key gets one here: the first character of an account is keyed by
	//! the account uid (a save from before #331 needs no rewrite), every later one by its own id.
	//! The first character also adopts the standing the account held while it had none.
	void AddCharacter(notnull LF_PlayerCharacter character, bool setAsActive = false)
	{
		bool first = m_aCharacters.IsEmpty();
		if (character.GetRecordKey().IsEmpty())
		{
			if (first)
				character.SetRecordKey(m_sPersistentId);
			else
				character.SetRecordKey(character.GetId());
		}

		if (first)
		{
			LF_CharacterStanding adopted = character.GetStanding();
			adopted.TakeValuesOf(m_AccountStanding);
		}

		int idx = m_aCharacters.Insert(character);
		if (setAsActive)
			m_iActiveCharacterIdx = idx;
	}

	//------------------------------------------------------------------------------------------------
	void RemoveCharacter(notnull LF_PlayerCharacter character)
	{
		int removedIdx = m_aCharacters.Find(character);
		if (removedIdx == -1)
			return;

		m_aCharacters.RemoveItemOrdered(character);

		// RemoveItemOrdered reindexes the array, so the active index must be
		// re-clamped. Removing the active character falls back to the first
		// remaining one; removing one before it shifts it down one.
		if (m_aCharacters.IsEmpty())
		{
			m_iActiveCharacterIdx = 0;
		}
		else if (removedIdx == m_iActiveCharacterIdx)
		{
			m_iActiveCharacterIdx = 0;
		}
		else if (removedIdx < m_iActiveCharacterIdx)
		{
			m_iActiveCharacterIdx--;
		}
		else if (m_iActiveCharacterIdx >= m_aCharacters.Count())
		{
			m_iActiveCharacterIdx = m_aCharacters.Count() - 1;
		}
	}

	//------------------------------------------------------------------------------------------------
	bool HasCharacters()
	{
		return !m_aCharacters.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	array<LF_PlayerCharacter> GetCharacters()
	{
		array<LF_PlayerCharacter> results();
		results.Reserve(m_aCharacters.Count());
		foreach (LF_PlayerCharacter character : m_aCharacters)
		{
			results.Insert(character);
		}
		return results;
	}

	//------------------------------------------------------------------------------------------------
	int GetCharacterCount()
	{
		return m_aCharacters.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The character in this roster slot (0-based), or null.
	LF_PlayerCharacter GetCharacterAt(int slot)
	{
		if (slot < 0 || slot >= m_aCharacters.Count())
			return null;

		return m_aCharacters.Get(slot);
	}

	//------------------------------------------------------------------------------------------------
	LF_PlayerCharacter GetCharacterById(string characterId)
	{
		if (characterId.IsEmpty())
			return null;

		foreach (LF_PlayerCharacter character : m_aCharacters)
		{
			if (character && character.GetId() == characterId)
				return character;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	LF_PlayerCharacter GetCharacterByKey(string recordKey)
	{
		if (recordKey.IsEmpty())
			return null;

		foreach (LF_PlayerCharacter character : m_aCharacters)
		{
			if (character && character.GetRecordKey() == recordKey)
				return character;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	LF_PlayerCharacter GetActiveCharacter()
	{
		if (!m_aCharacters.IsEmpty())
			return m_aCharacters.Get(m_iActiveCharacterIdx);

		return null;
	}

	//------------------------------------------------------------------------------------------------
	void SetActiveCharacter(notnull LF_PlayerCharacter character)
	{
		int idx = m_aCharacters.Find(character);
		if (idx != -1)
			m_iActiveCharacterIdx = idx;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PlayerAccount Create(string playerUid)
	{
		LF_PlayerAccount account();
		account.SetPersistentId(playerUid);
		return account;
	}
};

//------------------------------------------------------------------------------------------------
//! What a character answers for, apart from its account: wanted level, police rank, and duty
//! totals (#331). Saved through the account prefix (wanted, active character only), the
//! "characters" payload (wanted, every character, save version 24) and the "progression" payload
//! (rank and duty totals, keyed by the character's record key).
class LF_CharacterStanding : Managed
{
	int m_iWantedLevel;
	int m_iPoliceRank;
	int m_iOnDutySeconds;
	int m_iUnpaidDutySeconds;

	//------------------------------------------------------------------------------------------------
	void TakeValuesOf(notnull LF_CharacterStanding other)
	{
		m_iWantedLevel = other.m_iWantedLevel;
		m_iPoliceRank = other.m_iPoliceRank;
		m_iOnDutySeconds = other.m_iOnDutySeconds;
		m_iUnpaidDutySeconds = other.m_iUnpaidDutySeconds;
	}
}

//------------------------------------------------------------------------------------------------
//! One character of an account. m_sId is its persistence id (survival, carried inventory, duty
//! locker). m_sRecordKey is what every other character-scoped store keys it by (LF_Utils.GetPlayerUID
//! returns it): slot 1 of an account keeps the account uid, so a save from before #331 loads into
//! slot 1 with nothing rewritten; every later character is keyed by its own id. The key never
//! changes once given.
class LF_PlayerCharacter
{
	protected string m_sId;
	protected string m_sRecordKey;
	protected ref LF_CharacterStanding m_Standing = new LF_CharacterStanding();
	protected ResourceName m_rPrefab;
	protected string m_sFirstName;
	protected string m_sLastName;
	protected int m_iAge;

	//------------------------------------------------------------------------------------------------
	string GetId()
	{
		return m_sId;
	}

	//------------------------------------------------------------------------------------------------
	void SetId(string id)
	{
		m_sId = id;
	}

	//------------------------------------------------------------------------------------------------
	string GetRecordKey()
	{
		return m_sRecordKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam and LF_PlayerAccount.AddCharacter only.
	void SetRecordKey(string recordKey)
	{
		m_sRecordKey = recordKey;
	}

	//------------------------------------------------------------------------------------------------
	LF_CharacterStanding GetStanding()
	{
		return m_Standing;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetPrefab()
	{
		return m_rPrefab;
	}

	//------------------------------------------------------------------------------------------------
	string GetFirstName()
	{
		return m_sFirstName;
	}

	//------------------------------------------------------------------------------------------------
	string GetLastName()
	{
		return m_sLastName;
	}

	//------------------------------------------------------------------------------------------------
	int GetAge()
	{
		return m_iAge;
	}

	//------------------------------------------------------------------------------------------------
	string GetFullName()
	{
		return m_sFirstName + " " + m_sLastName;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PlayerCharacter Create(ResourceName prefab, string firstName, string lastName, int age)
	{
		LF_PlayerCharacter character();
		character.m_sId = PersistenceIdUtils.Generate();
		character.m_rPrefab = prefab;
		character.m_sFirstName = firstName;
		character.m_sLastName = lastName;
		character.m_iAge = age;
		return character;
	}

	// Convenience overload: create with default placeholder name/age when only prefab is provided
	static LF_PlayerCharacter Create(ResourceName prefab)
	{
		string defaultFirst = "New";
		string defaultLast = "Player";
		int defaultAge = 18;
		return Create(prefab, defaultFirst, defaultLast, defaultAge);
	}
};
