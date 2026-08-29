enum EL_Faction
{
	CIVILIAN,
	POLICE
}

class EL_PlayerAccount : Managed
{
	/*protected*/ string m_sPersistentId;
	/*protected*/ ref array<ref EL_PlayerCharacter> m_aCharacters = {};
	/*protected*/ int m_iActiveCharacterIdx;
	/*protected*/ EL_Faction m_eFaction = EL_Faction.CIVILIAN;
	/*protected*/ bool m_bOnDuty = false;
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
	void SetFaction(EL_Faction faction)
	{
		m_eFaction = faction;
	}

	//------------------------------------------------------------------------------------------------
	EL_Faction GetFaction()
	{
		return m_eFaction;
	}

	//------------------------------------------------------------------------------------------------
	void SetOnDuty(bool onDuty)
	{
		m_bOnDuty = onDuty;
	}

	//------------------------------------------------------------------------------------------------
	bool IsOnDuty()
	{
		return m_bOnDuty;
	}

	//------------------------------------------------------------------------------------------------
	void SetWantedLevel(int level)
	{
		m_iWantedLevel = Math.Clamp(level, 0, 5); // Max 5
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
	void AddCharacter(notnull EL_PlayerCharacter character, bool setAsActive = false)
	{
		int idx = m_aCharacters.Insert(character);
		if (setAsActive)
			m_iActiveCharacterIdx = idx;
	}

	//------------------------------------------------------------------------------------------------
	void RemoveCharacter(notnull EL_PlayerCharacter character)
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
	array<EL_PlayerCharacter> GetCharacters()
	{
		array<EL_PlayerCharacter> results();
		results.Reserve(m_aCharacters.Count());
		foreach (EL_PlayerCharacter character : m_aCharacters)
		{
			results.Insert(character);
		}
		return results;
	}

	//------------------------------------------------------------------------------------------------
	EL_PlayerCharacter GetActiveCharacter()
	{
		if (!m_aCharacters.IsEmpty())
			return m_aCharacters.Get(m_iActiveCharacterIdx);

		return null;
	}

	//------------------------------------------------------------------------------------------------
	void SetActiveCharacter(notnull EL_PlayerCharacter character)
	{
		m_iActiveCharacterIdx = m_aCharacters.Find(character);
	}

	//------------------------------------------------------------------------------------------------
	static EL_PlayerAccount Create(string playerUid)
	{
		EL_PlayerAccount account();
		account.SetPersistentId(playerUid);
		return account;
	}
};

class EL_PlayerCharacter
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
	static EL_PlayerCharacter Create(ResourceName prefab, string firstName, string lastName, int age)
	{
		EL_PlayerCharacter character();
		character.m_sId = PersistenceIdUtils.Generate();
		character.m_rPrefab = prefab;
		character.m_sFirstName = firstName;
		character.m_sLastName = lastName;
		character.m_iAge = age;
		return character;
	}

	// Convenience overload: create with default placeholder name/age when only prefab is provided
	static EL_PlayerCharacter Create(ResourceName prefab)
	{
		string defaultFirst = "New";
		string defaultLast = "Player";
		int defaultAge = 18;
		return Create(prefab, defaultFirst, defaultLast, defaultAge);
	}
};
