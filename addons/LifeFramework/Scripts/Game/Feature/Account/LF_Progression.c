//------------------------------------------------------------------------------------------------
//! Progression that survives a restart, keyed by player uid: the civilian delivery career pay, police
//! rank and duty seconds, the benefit cooldown, licences, and quest turn-ins. Saved as the
//! "progression" payload (save version 7).
//!
//! OWNERS. Career, police, and benefit state stay with their owners (LF_CivilianJob,
//! LF_PlayerAccount, LF_UnemploymentBenefit); ExportAll reads them and ApplyAll writes them back.
//! Licences and quest turn-ins live on the character entity, which is replaced on every spawn, so
//! their owners write through to this store (Note*) and LF_SpawnLogic applies the record to each
//! new body (ApplyToCharacter). Death keeps progression; only carried items drop.
//!
//! JOB LEVELS (#749). Per-job XP is LF_JobLevels and its own "joblevels" payload (save version 28).
//! The record keeps its pre-#749 layout (positional save): ExportAll mirrors CIVILIAN XP into the
//! old single-level fields and DELIVERY XP into the career fields, and ApplyAll seeds LF_JobLevels
//! from them (LF_JobLevels.SeedFromLegacy). A version 7-27 save has nothing else, so that seed is
//! its migration; a version 28 save then replaces it with its "joblevels" rows.
//!
//! TOLERANCE. A licence name the enum no longer has is skipped and logged. A quest id no giver
//! defines is kept. A rank above the ladder clamps to the top usable rank.
//!
//! IDEMPOTENT. ApplyAll replaces the store and sets owner state to the saved values; applying the
//! same records twice ends in the same state. Active runs and the duty clock are session state
//! and are left alone. The delivery sign-up is its own payload (LF_CivilianJob.ApplySignUps).
//------------------------------------------------------------------------------------------------
class LF_Progression
{
	protected static ref map<string, ref LF_ProgressionRecord> s_mRecords;

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset). The next world's save load fills the store again.
	static void ResetStatics()
	{
		s_mRecords = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_ProgressionRecord> Records()
	{
		if (!s_mRecords)
			s_mRecords = new map<string, ref LF_ProgressionRecord>();

		return s_mRecords;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ProgressionRecord Entry(string playerUid)
	{
		LF_ProgressionRecord record = Records().Get(playerUid);
		if (!record)
		{
			record = LF_ProgressionRecord.Create(playerUid);
			Records().Set(playerUid, record);
		}

		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The stored record (read-only use), or null.
	static LF_ProgressionRecord Get(string playerUid)
	{
		if (!s_mRecords)
			return null;

		return s_mRecords.Get(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Write-through from LF_QuestTrackerComponent.TurnInQuest.
	static void NoteTurnedIn(string playerUid, string questId)
	{
		if (playerUid.IsEmpty() || questId.IsEmpty())
		{
			LF_Debug.Warn("Progression", string.Format("turn-in not remembered: player '%1' quest '%2'", playerUid, questId));
			return;
		}

		LF_ProgressionRecord record = Entry(playerUid);
		if (record.m_aTurnedInQuests.Find(questId) < 0)
			record.m_aTurnedInQuests.Insert(questId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the store remembers the player turning the quest in.
	static bool HasTurnedIn(string playerUid, string questId)
	{
		LF_ProgressionRecord record = Get(playerUid);
		if (!record)
			return false;

		return record.m_aTurnedInQuests.Find(questId) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Write-through from LF_LicenseManagerComponent.UnlockLicense: the whole unlocked list.
	static void NoteLicenses(string playerUid, notnull array<LF_ELicenseType> licenses)
	{
		if (playerUid.IsEmpty())
			return;

		LF_ProgressionRecord record = Entry(playerUid);
		record.m_aLicenses.Clear();
		foreach (LF_ELicenseType license : licenses)
		{
			record.m_aLicenses.Insert(typename.EnumToString(LF_ELicenseType, license));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the stored licence names. Unknown names are skipped and logged.
	//! \return How many licences resolved.
	static int GetLicenses(string playerUid, notnull array<LF_ELicenseType> outLicenses)
	{
		outLicenses.Clear();
		LF_ProgressionRecord record = Get(playerUid);
		if (!record)
			return 0;

		foreach (string name : record.m_aLicenses)
		{
			LF_ELicenseType license = typename.StringToEnum(LF_ELicenseType, name);
			if (typename.EnumToString(LF_ELicenseType, license) != name)
			{
				LF_Debug.Warn("Progression", string.Format("saved licence '%1' for %2 no longer exists, skipped", name, playerUid));
				continue;
			}

			if (outLicenses.Find(license) < 0)
				outLicenses.Insert(license);
		}

		return outLicenses.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops everything progression holds for the player (test cleanup, admin wipe). The account's
	//! rank and duty totals belong to the account and are left alone.
	static void Forget(string playerUid)
	{
		if (s_mRecords)
			s_mRecords.Remove(playerUid);

		LF_CivilianJob.ForgetCareer(playerUid);
		LF_UnemploymentBenefit.ForgetCollection(playerUid);
		LF_JobLevels.Forget(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets every earned licence (the licences persistence switch, #689): each record keeps an
	//! empty list, which leaves a body its starter licences at the next spawn.
	//! \return How many records held a licence.
	static int ClearAllLicences()
	{
		int cleared;
		foreach (string uid, LF_ProgressionRecord record : Records())
		{
			if (!record || !record.m_aLicenses || record.m_aLicenses.IsEmpty())
				continue;

			record.m_aLicenses.Clear();
			cleared++;
		}

		return cleared;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: one record per player with any progression.
	//! A running duty clock is settled first (the clock keeps running), so a shift that ends in
	//! the shutdown save keeps its seconds.
	static array<ref LF_ProgressionRecord> ExportAll()
	{
		float now = LF_JobGuards.Now();
		int nowMs = Math.Round(now * 1000);

		array<string> uids = {};
		map<string, bool> seen = new map<string, bool>();
		if (s_mRecords)
		{
			foreach (string storedUid, LF_ProgressionRecord stored : s_mRecords)
			{
				AddUid(storedUid, uids, seen);
			}
		}

		array<string> owners = {};
		LF_CivilianJob.CollectCareerUids(owners);
		LF_UnemploymentBenefit.CollectLedgerUids(owners);
		LF_JobLevels.CollectUids(owners);
		foreach (string ownerUid : owners)
		{
			AddUid(ownerUid, uids, seen);
		}

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		foreach (LF_PlayerAccountRecord accountRecord : LF_PlayerAccountManager.ExportAll())
		{
			if (!accountRecord || !accounts)
				continue;

			LF_PlayerAccount account = accounts.GetFromCache(accountRecord.m_sPersistentId);
			if (!account)
				continue;

			if (account.GetFaction() != LF_Faction.CIVILIAN && account.IsDutyClockRunning())
				LF_PoliceRanks.SettleDutyAt(account, true, nowMs);

			// Rank and duty totals are per character (#331), saved under each character's key.
			array<string> keys = {};
			if (account.HasCharacters())
			{
				foreach (LF_PlayerCharacter character : account.GetCharacters())
				{
					if (character)
						keys.Insert(character.GetRecordKey());
				}
			}
			else
			{
				keys.Insert(account.GetPersistentId());
			}

			foreach (string key : keys)
			{
				LF_CharacterStanding standing = account.GetStandingFor(key);
				if (standing && (standing.m_iPoliceRank != 0 || standing.m_iOnDutySeconds != 0 || standing.m_iUnpaidDutySeconds != 0))
					AddUid(key, uids, seen);
			}
		}

		array<ref LF_ProgressionRecord> records = new array<ref LF_ProgressionRecord>();
		foreach (string uid : uids)
		{
			LF_ProgressionRecord record;
			LF_ProgressionRecord stored = Get(uid);
			if (stored)
				record = stored.Copy();
			else
				record = LF_ProgressionRecord.Create(uid);

			if (LF_CivilianJob.HasCareer(uid))
				record.m_iCareerPaidTotal = LF_CivilianJob.GetPaidTotal(uid);

			MirrorJobLevels(uid, record);

			LF_CharacterStanding standing;
			if (accounts)
				standing = accounts.FindStanding(uid);
			if (standing)
			{
				record.m_iPoliceRank = standing.m_iPoliceRank;
				record.m_iOnDutySeconds = standing.m_iOnDutySeconds;
				record.m_iUnpaidDutySeconds = standing.m_iUnpaidDutySeconds;
			}

			record.m_fBenefitElapsedSeconds = LF_UnemploymentBenefit.GetElapsedSinceCollection(uid, now);
			records.Insert(record);
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: replaces the store and writes the saved values back to their owners,
	//! then to every live body. Runs after the account prefix, so the accounts exist.
	static void ApplyAll(notnull array<ref LF_ProgressionRecord> records)
	{
		float now = LF_JobGuards.Now();
		s_mRecords = new map<string, ref LF_ProgressionRecord>();
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();

		// Job XP from the record's pre-#749 fields; a version 28 save's "joblevels" payload, read
		// later in the same load, replaces it.
		int seeded = LF_JobLevels.SeedFromLegacy(records);

		int applied = 0;
		foreach (LF_ProgressionRecord record : records)
		{
			if (!record || record.m_sPlayerUid.IsEmpty())
			{
				LF_Debug.Warn("Progression", "skipped a progression record with no player id");
				continue;
			}

			string uid = record.m_sPlayerUid;
			s_mRecords.Set(uid, record.Copy());

			LF_CivilianJob.RestoreCareer(uid, record.m_iCareerPaidTotal);
			LF_UnemploymentBenefit.RestoreElapsedSinceCollection(uid, record.m_fBenefitElapsedSeconds, now);

			// The standing of the character saved under this key (#331), not the account's active one.
			LF_CharacterStanding standing;
			if (accounts)
				standing = accounts.FindStanding(uid);
			if (standing)
			{
				standing.m_iPoliceRank = UsableRank(uid, record.m_iPoliceRank);
				standing.m_iOnDutySeconds = Math.Max(0, record.m_iOnDutySeconds);
				standing.m_iUnpaidDutySeconds = Math.Max(0, record.m_iUnpaidDutySeconds);
				LF_AccountReplication.Publish(accounts.GetFromCache(uid));
			}
			else if (record.m_iPoliceRank != 0 || record.m_iOnDutySeconds != 0)
			{
				LF_Debug.Warn("Progression", string.Format("police progression for %1 has no account, rank %2 not applied", uid, record.m_iPoliceRank));
			}

			applied++;
		}

		int bodies = ApplyToLivePlayers();
		LF_Debug.Info("Progression", string.Format("applied %1 progression record(s), %2 live body(ies), job xp seeded for %3 character(s)", applied, bodies, seeded));
	}

	//------------------------------------------------------------------------------------------------
	//! Puts every stored rank and duty total back on the character its key names. The "characters"
	//! payload (save version 24) calls it when a saved record key differs from the default key the
	//! account prefix gave while this payload loaded.
	static void ReapplyStandings()
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!s_mRecords || !accounts)
			return;

		foreach (string uid, LF_ProgressionRecord record : s_mRecords)
		{
			LF_CharacterStanding standing = accounts.FindStanding(uid);
			if (!record || !standing)
				continue;

			standing.m_iPoliceRank = UsableRank(uid, record.m_iPoliceRank);
			standing.m_iOnDutySeconds = Math.Max(0, record.m_iOnDutySeconds);
			standing.m_iUnpaidDutySeconds = Math.Max(0, record.m_iUnpaidDutySeconds);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Gives a new body the player's saved licences and turned-in quests. Job levels are keyed by the
	//! character, not the body (LF_JobLevels), so a new body has them already.
	//! \return true when the store had a record for the player.
	static bool ApplyToCharacter(string playerUid, IEntity character)
	{
		if (!character || playerUid.IsEmpty())
			return false;

		LF_ProgressionRecord record = Get(playerUid);
		if (!record)
			return false;

		if (!record.m_aLicenses.IsEmpty())
		{
			LF_LicenseManagerComponent licenseManager = LF_Component<LF_LicenseManagerComponent>.Find(character);
			if (licenseManager)
			{
				array<LF_ELicenseType> licenses = {};
				GetLicenses(playerUid, licenses);
				licenseManager.SetUnlockedLicenses(licenses);
			}
		}

		int quests = 0;
		if (!record.m_aTurnedInQuests.IsEmpty())
		{
			LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(character);
			if (tracker)
				quests = tracker.RestoreTurnedIn(record.m_aTurnedInQuests);
		}

		LF_Debug.Info("Progression", string.Format("progression applied to %1: %2 licence(s), %3 quest(s) restored", playerUid, record.m_aLicenses.Count(), quests));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static int ApplyToLivePlayers()
	{
		if (!GetGame())
			return 0;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return 0;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		int bodies = 0;
		foreach (int playerId : playerIds)
		{
			IEntity body = players.GetPlayerControlledEntity(playerId);
			if (!body)
				continue;

			if (ApplyToCharacter(LF_Utils.GetPlayerUID(playerId), body))
				bodies++;
		}

		return bodies;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the character's CIVILIAN and DELIVERY XP into the record's pre-#749 fields, in the old
	//! curves (LF_JobLevels.XpToLegacyLevel / XpToLegacyCareer), so the record means what a version
	//! 7-27 build wrote. Skill points no longer exist and are written as 0.
	protected static void MirrorJobLevels(string uid, notnull LF_ProgressionRecord record)
	{
		int civilian = LF_JobLevels.XpOf(uid, LF_JobLevels.JOB_CIVILIAN);
		record.m_bHasLevel = civilian > 0;
		int level;
		float experience;
		LF_JobLevels.XpToLegacyLevel(civilian, level, experience);
		record.m_iPlayerLevel = level;
		record.m_fPlayerExperience = experience;
		record.m_iSkillPoints = 0;
		record.m_iTotalSkillPoints = 0;

		LF_JobLevels.XpToLegacyCareer(LF_JobLevels.XpOf(uid, LF_JobLevels.JOB_DELIVERY), level, experience);
		record.m_iCareerLevel = level;
		record.m_fCareerExperience = experience;
	}

	//------------------------------------------------------------------------------------------------
	protected static int UsableRank(string playerUid, int rank)
	{
		if (LF_PoliceRanks.IsRankUsable(rank))
			return rank;

		int clamped = LF_PoliceRanks.PATROL;
		if (rank > LF_PoliceRanks.TopRank())
			clamped = LF_PoliceRanks.TopRank();

		LF_Debug.Warn("Progression", string.Format("saved police rank %1 for %2 is not on the ladder, using %3", rank, playerUid, clamped));
		return clamped;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddUid(string uid, notnull array<string> uids, notnull map<string, bool> seen)
	{
		if (uid.IsEmpty() || seen.Contains(uid))
			return;

		seen.Set(uid, true);
		uids.Insert(uid);
	}
}
