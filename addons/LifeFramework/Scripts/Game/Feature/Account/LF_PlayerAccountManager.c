class LF_PlayerAccountManager : Managed
{
	protected static ref LF_PlayerAccountManager s_pInstance;
	protected ref map<string, ref LF_PlayerAccount> m_mAccounts;

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: export every cached account as a record for the serializer.
	//! \return Array of account records (never null), one per cached account.
	static array<ref LF_PlayerAccountRecord> ExportAll()
	{
		LF_PlayerAccountManager instance = GetInstance();
		if (!instance)
			return new array<ref LF_PlayerAccountRecord>();

		array<ref LF_PlayerAccountRecord> records = new array<ref LF_PlayerAccountRecord>();
		foreach (string playerUid, LF_PlayerAccount account : instance.m_mAccounts)
		{
			if (!account)
				continue;

			LF_PlayerAccountRecord record = LF_PlayerAccountRecord.Create(account.GetPersistentId());
			record.m_iActiveCharacterIdx = account.m_iActiveCharacterIdx;
			record.m_eFaction = account.m_eFaction;
			record.m_bOnDuty = account.m_bOnDuty;
			record.m_iWantedLevel = account.m_iWantedLevel;

			record.m_aCharacters = new array<ref LF_PlayerCharacterRecord>();
			foreach (LF_PlayerCharacter character : account.m_aCharacters)
			{
				if (!character)
					continue;

				record.m_aCharacters.Insert(LF_PlayerCharacterRecord.Create(
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
	static void ApplyAll(notnull array<ref LF_PlayerAccountRecord> records)
	{
		LF_PlayerAccountManager instance = GetInstance();

		foreach (LF_PlayerAccountRecord record : records)
		{
			if (!record)
				continue;

			LF_PlayerAccount account = LF_PlayerAccount.Create(record.m_sPersistentId);
			account.m_iActiveCharacterIdx = record.m_iActiveCharacterIdx;
			// The faction is the duty role (POLICE only while clocked in). A save from the faction
			// screen era can hold POLICE off duty, or a civilian on duty: both load as civilian.
			bool onDutyPolice = record.m_bOnDuty && record.m_eFaction == LF_Faction.POLICE;
			if (onDutyPolice)
				account.m_eFaction = LF_Faction.POLICE;
			else
				account.m_eFaction = LF_Faction.CIVILIAN;

			account.m_bOnDuty = onDutyPolice;
			account.SetWantedLevel(record.m_iWantedLevel);

			// Rank and duty totals are not in this record; the progression payload (save version
			// 7) restores them afterwards. Until then the live values carry over, so re-applying a
			// version 6 save to a running session does not demote anyone.
			LF_PlayerAccount live = instance.m_mAccounts.Get(record.m_sPersistentId);
			if (live)
			{
				account.SetPoliceRank(live.GetPoliceRank());
				account.SetOnDutySeconds(live.GetOnDutySeconds());
				account.SetUnpaidDutySeconds(live.GetUnpaidDutySeconds());
			}

			if (record.m_aCharacters)
			{
				foreach (LF_PlayerCharacterRecord characterRecord : record.m_aCharacters)
				{
					if (!characterRecord)
						continue;

					LF_PlayerCharacter character = LF_PlayerCharacter.Create(
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
	static LF_PlayerAccount GetOrCreate(string playerUid)
	{
		LF_PlayerAccountManager instance = GetInstance();
		LF_PlayerAccount account = instance.m_mAccounts.Get(playerUid);
		if (!account)
		{
			account = LF_PlayerAccount.Create(playerUid);
			instance.AddToCache(account);
		}

		return account;
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps the account cached for the rest of the session. Persistence is owned by the
	//! game-mode serializer (LF_PersistenceComponentSerializer exports every cached account on
	//! save), so short-lived writes must not drop the instance: evicting here made every
	//! crime/duty/arrest/fine/faction call site lose the account until the next save, which is
	//! why consecutive wanted bumps silently stopped stacking. Reset() drops all accounts at
	//! game-mode teardown.
	//! \param account Account instance to keep resident
	void SaveAndReleaseAccount(LF_PlayerAccount account)
	{
		if (account)
			m_mAccounts.Set(account.GetPersistentId(), account);
	}

	//------------------------------------------------------------------------------------------------
	//! Add the player account instance to the cache so it is returned on the next GetAccount call
	//! \param account Account instance to cache
	void AddToCache(notnull LF_PlayerAccount account)
	{
		m_mAccounts.Set(account.GetPersistentId(), account);
	}

	//------------------------------------------------------------------------------------------------
	//! Backwards-compatible alias used by other systems
	void AddAccount(notnull LF_PlayerAccount account)
	{
		AddToCache(account);
	}

	//------------------------------------------------------------------------------------------------
	LF_PlayerAccount GetFromCache(string playerUid)
	{
		return m_mAccounts.Get(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	LF_PlayerAccount GetFromCache(int playerId)
	{
		return GetFromCache(LF_Utils.GetPlayerUID(playerId));
	}

	//------------------------------------------------------------------------------------------------
	LF_PlayerAccount GetFromCache(IEntity player)
	{
		return GetFromCache(LF_Utils.GetPlayerUID(player));
	}

	//------------------------------------------------------------------------------------------------
	//! Convenience synchronous getter used by various systems. Returns the
	//! cached account if present (no blocking load). Callers should handle
	//! a null result or explicitly call GetOrCreate if they need to
	//! ensure availability.
	LF_PlayerAccount GetAccount(string playerUid)
	{
		return GetFromCache(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	LF_PlayerAccount GetAccount(int playerId)
	{
		return GetFromCache(playerId);
	}

	//------------------------------------------------------------------------------------------------
	LF_PlayerAccount GetAccount(IEntity player)
	{
		return GetFromCache(player);
	}

	//------------------------------------------------------------------------------------------------
	//! Any resident account's character with this id (an organization lists offline members too).
	LF_PlayerCharacter FindCharacter(string characterId)
	{
		if (characterId.IsEmpty())
			return null;

		foreach (string playerUid, LF_PlayerAccount account : m_mAccounts)
		{
			if (!account)
				continue;

			foreach (LF_PlayerCharacter character : account.m_aCharacters)
			{
				if (character && character.GetId() == characterId)
					return character;
			}
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PlayerAccountManager GetInstance()
	{
		if (!s_pInstance)
			s_pInstance = new LF_PlayerAccountManager();

		return s_pInstance;
	}

	//------------------------------------------------------------------------------------------------
	static void Reset()
	{
		s_pInstance = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_PlayerAccountManager()
	{
		m_mAccounts = new map<string, ref LF_PlayerAccount>()
	}
};
