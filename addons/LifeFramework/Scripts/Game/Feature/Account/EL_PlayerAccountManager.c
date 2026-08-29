class EL_PlayerAccountManager : Managed
{
	protected static ref EL_PlayerAccountManager s_pInstance;
	protected ref map<string, ref EL_PlayerAccount> m_mAccounts;

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: export every cached account as a record for the serializer.
	//! \return Array of account records (never null), one per cached account.
	static array<ref EL_PlayerAccountRecord> ExportAll()
	{
		EL_PlayerAccountManager instance = GetInstance();
		if (!instance)
			return new array<ref EL_PlayerAccountRecord>();

		array<ref EL_PlayerAccountRecord> records = new array<ref EL_PlayerAccountRecord>();
		foreach (string playerUid, EL_PlayerAccount account : instance.m_mAccounts)
		{
			if (!account)
				continue;

			EL_PlayerAccountRecord record = EL_PlayerAccountRecord.Create(account.GetPersistentId());
			record.m_iActiveCharacterIdx = account.m_iActiveCharacterIdx;
			record.m_eFaction = account.m_eFaction;
			record.m_bOnDuty = account.m_bOnDuty;
			record.m_iWantedLevel = account.m_iWantedLevel;

			record.m_aCharacters = new array<ref EL_PlayerCharacterRecord>();
			foreach (EL_PlayerCharacter character : account.m_aCharacters)
			{
				if (!character)
					continue;

				record.m_aCharacters.Insert(EL_PlayerCharacterRecord.Create(
					character.GetId(),
					character.GetPrefab(),
					character.GetFirstName(),
					character.GetLastName(),
					character.GetAge()
				));
			}

			records.Insert(record);
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: apply records back into the cache (idempotent). Existing ids are
	//! overwritten so a re-apply to a live session converges on the saved state.
	//! \param records Records read from the save.
	static void ApplyAll(notnull array<ref EL_PlayerAccountRecord> records)
	{
		EL_PlayerAccountManager instance = GetInstance();

		foreach (EL_PlayerAccountRecord record : records)
		{
			if (!record)
				continue;

			EL_PlayerAccount account = EL_PlayerAccount.Create(record.m_sPersistentId);
			account.m_iActiveCharacterIdx = record.m_iActiveCharacterIdx;
			account.m_eFaction = record.m_eFaction;
			account.m_bOnDuty = record.m_bOnDuty;
			account.m_iWantedLevel = record.m_iWantedLevel;

			if (record.m_aCharacters)
			{
				foreach (EL_PlayerCharacterRecord characterRecord : record.m_aCharacters)
				{
					if (!characterRecord)
						continue;

					EL_PlayerCharacter character = EL_PlayerCharacter.Create(
						characterRecord.m_rPrefab,
						characterRecord.m_sFirstName,
						characterRecord.m_sLastName,
						characterRecord.m_iAge
					);
					character.SetId(characterRecord.m_sId);
					account.AddCharacter(character, false);
				}
			}

			instance.AddToCache(account);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the cached account for a player, creating and caching a new one when absent.
	//! \param playerUid The players Bohemia UID
	//! \return The account for the player (never null).
	static EL_PlayerAccount GetOrCreate(string playerUid)
	{
		EL_PlayerAccountManager instance = GetInstance();
		EL_PlayerAccount account = instance.m_mAccounts.Get(playerUid);
		if (!account)
		{
			account = EL_PlayerAccount.Create(playerUid);
			instance.AddToCache(account);
		}

		return account;
	}

	//------------------------------------------------------------------------------------------------
	//! Remove the account from the cache, releasing it. Persistence is now owned by the game-mode
	//! serializer; per-release saves no longer happen, so this only drops the cached instance.
	//! \param account Account instance to release
	void SaveAndReleaseAccount(EL_PlayerAccount account)
	{
		if (account)
			m_mAccounts.Remove(account.GetPersistentId());
	}

	//------------------------------------------------------------------------------------------------
	//! Add the player account instance to the cache so it is returned on the next GetAccount call
	//! \param account Account instance to cache
	void AddToCache(notnull EL_PlayerAccount account)
	{
		m_mAccounts.Set(account.GetPersistentId(), account);
	}

	//------------------------------------------------------------------------------------------------
	//! Backwards-compatible alias used by other systems
	void AddAccount(notnull EL_PlayerAccount account)
	{
		AddToCache(account);
	}

	//------------------------------------------------------------------------------------------------
	EL_PlayerAccount GetFromCache(string playerUid)
	{
		return m_mAccounts.Get(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	EL_PlayerAccount GetFromCache(int playerId)
	{
		return GetFromCache(EL_Utils.GetPlayerUID(playerId));
	}

	//------------------------------------------------------------------------------------------------
	EL_PlayerAccount GetFromCache(IEntity player)
	{
		return GetFromCache(EL_Utils.GetPlayerUID(player));
	}

	//------------------------------------------------------------------------------------------------
	//! Convenience synchronous getter used by various systems. Returns the
	//! cached account if present (no blocking load). Callers should handle
	//! a null result or explicitly call GetOrCreate if they need to
	//! ensure availability.
	EL_PlayerAccount GetAccount(string playerUid)
	{
		return GetFromCache(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	EL_PlayerAccount GetAccount(int playerId)
	{
		return GetFromCache(playerId);
	}

	//------------------------------------------------------------------------------------------------
	EL_PlayerAccount GetAccount(IEntity player)
	{
		return GetFromCache(player);
	}

	//------------------------------------------------------------------------------------------------
	static EL_PlayerAccountManager GetInstance()
	{
		if (!s_pInstance)
			s_pInstance = new EL_PlayerAccountManager();

		return s_pInstance;
	}

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		s_pInstance = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void EL_PlayerAccountManager()
	{
		m_mAccounts = new map<string, ref EL_PlayerAccount>()
	}
};
