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
	/*protected*/ int m_iPoliceRank = 0;
	/*protected*/ int m_iOnDutySeconds = 0;
	/*protected*/ bool m_bDutyClockRunning = false;
	/*protected*/ int m_iDutyStartedAtMs = 0;
	/*protected*/ int m_iUnpaidDutySeconds = 0;
	protected bool m_bArmoryDrawnThisShift = false;
	/*protected*/ int m_iWantedLevel = 0;

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
	//! Police rank index. 0 is the starting rank. This is live session state; the account
	//! save record does not carry it yet, so a restart returns the officer to the starting rank.
	int GetPoliceRank()
	{
		return m_iPoliceRank;
	}

	//------------------------------------------------------------------------------------------------
	void SetPoliceRank(int rank)
	{
		m_iPoliceRank = rank;
		LF_AccountReplication.Publish(this);
	}

	//------------------------------------------------------------------------------------------------
	int GetOnDutySeconds()
	{
		return m_iOnDutySeconds;
	}

	//------------------------------------------------------------------------------------------------
	void AddOnDutySeconds(int seconds)
	{
		if (seconds <= 0)
			return;

		m_iOnDutySeconds = m_iOnDutySeconds + seconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam (LF_Progression): the saved total. Negative is read as 0.
	void SetOnDutySeconds(int seconds)
	{
		m_iOnDutySeconds = Math.Max(0, seconds);
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
		return m_iUnpaidDutySeconds;
	}

	//------------------------------------------------------------------------------------------------
	void AddUnpaidDutySeconds(int seconds)
	{
		if (seconds <= 0)
			return;

		m_iUnpaidDutySeconds = m_iUnpaidDutySeconds + seconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam (LF_Progression): the saved unpaid seconds. Negative is read as 0.
	void SetUnpaidDutySeconds(int seconds)
	{
		m_iUnpaidDutySeconds = Math.Max(0, seconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the unpaid duty seconds and zeroes them, so a second take returns 0.
	int TakeUnpaidDutySeconds()
	{
		int seconds = m_iUnpaidDutySeconds;
		m_iUnpaidDutySeconds = 0;
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
		m_iWantedLevel = Math.Clamp(level, 0, 5); // Max 5

		// Wanted cleared (jail, fine, admin): the posters for this person come down.
		if (m_iWantedLevel == 0)
			LF_WantedReports.ClearFor(m_sPersistentId);

		LF_AccountReplication.Publish(this);
	}

	//------------------------------------------------------------------------------------------------
	int GetWantedLevel()
	{
		return m_iWantedLevel;
	}

	//------------------------------------------------------------------------------------------------
	void IncreaseWantedLevel(int amount = 1)
	{
		SetWantedLevel(m_iWantedLevel + amount);
	}

	//------------------------------------------------------------------------------------------------
	void AddCharacter(notnull LF_PlayerCharacter character, bool setAsActive = false)
	{
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

class LF_PlayerCharacter
{
	protected string m_sId;
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
