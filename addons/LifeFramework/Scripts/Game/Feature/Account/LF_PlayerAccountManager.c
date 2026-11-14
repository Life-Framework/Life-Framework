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
			// The prefix keeps its pre-#331 shape: the active character's wanted level. The
			// "characters" payload (save version 24) carries every character's.
			record.m_iWantedLevel = account.GetWantedLevel();

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
	//! Record keys get the default here, the same rule AddCharacter uses: slot 1 keeps the account
	//! uid, every later slot its own id. Before #331 registration refused a second character, so an
	//! old save's one character is slot 1 and every uid-keyed record it has stays its own. A save at
	//! version 24 or later then confirms the keys from the "characters" payload (ApplyCharacterSlots).
	//! \param records Records read from the save.
	static void ApplyAll(notnull array<ref LF_PlayerAccountRecord> records)
	{
		LF_PlayerAccountManager instance = GetInstance();

		foreach (LF_PlayerAccountRecord record : records)
		{
			if (!record)
				continue;

			LF_PlayerAccount account = LF_PlayerAccount.Create(record.m_sPersistentId);
			// The faction is the duty role (POLICE only while clocked in). A save from the faction
			// screen era can hold POLICE off duty, or a civilian on duty: both load as civilian.
			bool onDutyRole = record.m_bOnDuty && (record.m_eFaction == LF_Faction.POLICE || record.m_eFaction == LF_Faction.MEDIC);
			if (onDutyRole)
				account.m_eFaction = record.m_eFaction;
			else
				account.m_eFaction = LF_Faction.CIVILIAN;

			account.m_bOnDuty = onDutyRole;

			// Rank and duty totals are not in this record; the progression payload (save version
			// 7) restores them afterwards. Until then the live values carry over, so re-applying a
			// version 6 save to a running session does not demote anyone.
			LF_PlayerAccount live = instance.m_mAccounts.Get(record.m_sPersistentId);

			int activeIdx = record.m_iActiveCharacterIdx;
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
					if (account.m_aCharacters.IsEmpty())
						character.SetRecordKey(record.m_sPersistentId);
					else
						character.SetRecordKey(characterRecord.m_sId);

					if (live)
					{
						LF_PlayerCharacter liveCharacter = live.GetCharacterById(characterRecord.m_sId);
						if (liveCharacter)
						{
							LF_CharacterStanding loadedStanding = character.GetStanding();
							LF_CharacterStanding liveCharacterStanding = liveCharacter.GetStanding();
							loadedStanding.TakeValuesOf(liveCharacterStanding);
						}
					}

					account.m_aCharacters.Insert(character);
				}
			}

			if (account.m_aCharacters.IsEmpty())
				account.m_iActiveCharacterIdx = 0;
			else
				account.m_iActiveCharacterIdx = Math.ClampInt(activeIdx, 0, account.m_aCharacters.Count() - 1);

			// Kept as locals and a plain bool: written as one chained call under
			// `if (live && account.m_aCharacters.IsEmpty())` the copy did not happen (observed in
			// LF_Test_ProgressionOldSave: live rank 1, loaded rank 0).
			bool noRoster = account.m_aCharacters.IsEmpty();
			if (live && noRoster)
			{
				LF_CharacterStanding accountStanding = account.GetStanding();
				LF_CharacterStanding liveStanding = live.GetStanding();
				accountStanding.TakeValuesOf(liveStanding);
			}

			// After the roster: the prefix's wanted level is the active character's.
			account.SetWantedLevel(record.m_iWantedLevel);

			instance.AddToCache(account);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam ("characters" payload, save version 24): every character's record key and
	//! wanted level, by account uid and character id.
	static array<ref LF_CharacterSlotRecord> ExportCharacterSlots()
	{
		array<ref LF_CharacterSlotRecord> records = new array<ref LF_CharacterSlotRecord>();
		LF_PlayerAccountManager instance = GetInstance();
		foreach (string playerUid, LF_PlayerAccount account : instance.m_mAccounts)
		{
			if (!account)
				continue;

			foreach (LF_PlayerCharacter character : account.m_aCharacters)
			{
				if (!character)
					continue;

				records.Insert(LF_CharacterSlotRecord.Create(account.GetPersistentId(), character.GetId(), character.GetRecordKey(), character.GetStanding().m_iWantedLevel));
			}
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: puts the saved record keys and wanted levels back on the characters the
	//! account prefix loaded. Plain assignment, so a second apply changes nothing. A row whose
	//! account or character is gone is skipped and logged.
	//! \return Rows applied.
	static int ApplyCharacterSlots(notnull array<ref LF_CharacterSlotRecord> records)
	{
		LF_PlayerAccountManager instance = GetInstance();
		int applied = 0;
		int rekeyed = 0;
		foreach (LF_CharacterSlotRecord record : records)
		{
			if (!record || record.m_sRecordKey.IsEmpty())
				continue;

			LF_PlayerAccount account = instance.m_mAccounts.Get(record.m_sAccountUid);
			LF_PlayerCharacter character;
			if (account)
				character = account.GetCharacterById(record.m_sCharacterId);

			if (!character)
			{
				LF_Debug.Warn("Account", string.Format("saved character %1 of account %2 is not loaded, its key %3 skipped", record.m_sCharacterId, record.m_sAccountUid, record.m_sRecordKey));
				continue;
			}

			if (character.GetRecordKey() != record.m_sRecordKey)
				rekeyed++;

			character.SetRecordKey(record.m_sRecordKey);
			character.GetStanding().m_iWantedLevel = Math.ClampInt(record.m_iWantedLevel, 0, 5);
			LF_AccountReplication.Publish(account);
			applied++;
		}

		// Progression loaded earlier in the save, under the default keys. A saved key that differs
		// (a roster that lost its first character) moves that rank to the right character.
		if (rekeyed > 0)
		{
			LF_Debug.Info("Account", string.Format("%1 character(s) keep a saved key other than the default; progression re-applied", rekeyed));
			LF_Progression.ReapplyStandings();
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! A save older than version 24 has no "characters" payload: every account keeps the record
	//! keys ApplyAll gave it (its one character is slot 1, keyed by the account uid).
	//! \return Accounts with a character, for the migration log line.
	static int CountAccountsWithCharacters()
	{
		LF_PlayerAccountManager instance = GetInstance();
		int count = 0;
		foreach (string playerUid, LF_PlayerAccount account : instance.m_mAccounts)
		{
			if (account && account.HasCharacters())
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! The record key of the account's active character (what LF_Utils.GetPlayerUID returns on the
	//! server), or the uid itself when no account is loaded for it.
	static string ResolveCharacterKey(string accountUid)
	{
		if (accountUid.IsEmpty() || !s_pInstance)
			return accountUid;

		LF_PlayerAccount account = s_pInstance.m_mAccounts.Get(accountUid);
		if (!account)
			return accountUid;

		return account.GetActiveCharacterKey();
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the cached account for a player, creating and caching a new one when absent.
	//! \param playerUid The player's account uid, or the record key of one of its characters.
	//! \return The account for the player (never null).
	static LF_PlayerAccount GetOrCreate(string playerUid)
	{
		LF_PlayerAccountManager instance = GetInstance();
		LF_PlayerAccount account = instance.GetFromCache(playerUid);
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
	//! A player's real platform identity arrived after their account was opened under a name
	//! stand-in (LF_PlayerIdentity). The stand-in account moves onto the real key so the next lookup
	//! finds it. Its characters keep their record keys, so the bank, rank and every other record
	//! saved under the stand-in still resolves through GetPlayerUID, now and after a restart.
	//! A returning player's real account is never overwritten: an empty stand-in is dropped, a real
	//! account with no character gives way, and when both hold characters both are kept.
	//! \return the account now under realUid, or null when neither key had one.
	static LF_PlayerAccount AdoptStandIn(string standInUid, string realUid)
	{
		if (standInUid.IsEmpty() || realUid.IsEmpty() || standInUid == realUid)
			return null;

		LF_PlayerAccountManager instance = GetInstance();
		LF_PlayerAccount standIn = instance.m_mAccounts.Get(standInUid);
		LF_PlayerAccount real = instance.m_mAccounts.Get(realUid);
		if (!standIn)
			return real;

		if (real && real.HasCharacters())
		{
			if (!standIn.HasCharacters())
			{
				instance.m_mAccounts.Remove(standInUid);
				LF_Debug.Info("Account", string.Format("stand-in account %1 had no character; account %2 is played", standInUid, realUid));
				LF_AccountReplication.Publish(real);
				return real;
			}

			LF_Debug.Warn("Account", string.Format("stand-in account %1 and account %2 both hold characters; %2 is played and %1 stays saved for an administrator", standInUid, realUid));
			return real;
		}

		instance.m_mAccounts.Remove(standInUid);
		standIn.SetPersistentId(realUid);
		instance.m_mAccounts.Set(realUid, standIn);
		LF_Debug.Info("Account", string.Format("account %1 moved to the real identity %2 (%3 character(s), record keys kept)", standInUid, realUid, standIn.GetCharacterCount()));
		LF_AccountReplication.Publish(standIn);
		return standIn;
	}

	//------------------------------------------------------------------------------------------------
	//! Add the player account instance to the cache so it is returned on the next GetAccount call
	//! \param account Account instance to cache
	void AddToCache(notnull LF_PlayerAccount account)
	{
		m_mAccounts.Set(account.GetPersistentId(), account);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops one account from the cache (test cleanup; an admin wipe). The next save leaves it out.
	void RemoveFromCache(string playerUid)
	{
		m_mAccounts.Remove(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Backwards-compatible alias used by other systems
	void AddAccount(notnull LF_PlayerAccount account)
	{
		AddToCache(account);
	}

	//------------------------------------------------------------------------------------------------
	//! The account by its uid, or by the record key of one of its characters (callers key their
	//! records by LF_Utils.GetPlayerUID, which is the active character's key since #331). Slot 1's
	//! key is the uid, so only a later slot pays for the scan.
	LF_PlayerAccount GetFromCache(string playerUid)
	{
		if (playerUid.IsEmpty())
			return null;

		LF_PlayerAccount account = m_mAccounts.Get(playerUid);
		if (account)
			return account;

		foreach (string accountUid, LF_PlayerAccount candidate : m_mAccounts)
		{
			if (candidate && candidate.GetCharacterByKey(playerUid))
				return candidate;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	LF_PlayerAccount GetFromCache(int playerId)
	{
		return GetFromCache(LF_Utils.GetAccountUID(playerId));
	}

	//------------------------------------------------------------------------------------------------
	LF_PlayerAccount GetFromCache(IEntity player)
	{
		return GetFromCache(LF_Utils.GetAccountUID(player));
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
	//! Server: every loaded account's wanted level back to 0 (the world reset, which also takes their
	//! posters down through SetWantedLevel).
	//! eturn How many accounts had a wanted level.
	int ClearAllWanted()
	{
		if (!m_mAccounts)
			return 0;

		int cleared = 0;
		for (int i = 0; i < m_mAccounts.Count(); i++)
		{
			LF_PlayerAccount account = m_mAccounts.GetElement(i);
			if (!account || account.GetWantedLevel() <= 0)
				continue;

			account.SetWantedLevel(0);
			cleared++;
		}

		return cleared;
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

			LF_PlayerCharacter character = account.GetCharacterById(characterId);
			if (character)
				return character;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Any resident account's character holding this record key (a wanted poster, a ticket).
	LF_PlayerCharacter FindCharacterByKey(string recordKey)
	{
		LF_PlayerAccount account = GetFromCache(recordKey);
		if (!account)
			return null;

		return account.GetCharacterByKey(recordKey);
	}

	//------------------------------------------------------------------------------------------------
	//! The standing (wanted, rank, duty totals) saved under this record key, or null.
	LF_CharacterStanding FindStanding(string recordKey)
	{
		LF_PlayerAccount account = GetFromCache(recordKey);
		if (!account)
			return null;

		return account.GetStandingFor(recordKey);
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
