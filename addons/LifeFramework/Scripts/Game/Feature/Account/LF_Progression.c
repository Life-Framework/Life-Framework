//------------------------------------------------------------------------------------------------
//! Progression that survives a restart, keyed by player uid: the civilian delivery career, police
//! rank and duty seconds, the benefit cooldown, the player level, licences, and quest turn-ins.
//! Saved as the "progression" payload (save version 7).
//!
//! OWNERS. Career, police, and benefit state stay with their owners (LF_CivilianJob,
//! EL_PlayerAccount, LF_UnemploymentBenefit); ExportAll reads them and ApplyAll writes them back.
//! Level, licences, and quest turn-ins live on the character entity, which is replaced on every
//! spawn, so their owners write through to this store (Note*) and EL_SpawnLogic applies the
//! record to each new body (ApplyToCharacter). Death keeps progression; only carried items drop.
//!
//! TOLERANCE. A licence name the enum no longer has is skipped and logged. A quest id no giver
//! defines is kept. A rank above the ladder clamps to the top usable rank.
//!
//! IDEMPOTENT. ApplyAll replaces the store and sets owner state to the saved values; applying the
//! same records twice ends in the same state. Sign-ups, active runs, and the duty clock are
//! session state and are left alone.
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
			EL_Debug.Warn("Progression", string.Format("turn-in not remembered: player '%1' quest '%2'", playerUid, questId));
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
	//! Write-through from EL_PlayerLevelComponent.
	static void NoteLevel(string playerUid, int level, float experience, int skillPoints, int totalSkillPoints)
	{
		if (playerUid.IsEmpty())
			return;

		LF_ProgressionRecord record = Entry(playerUid);
		record.m_bHasLevel = true;
		record.m_iPlayerLevel = level;
		record.m_fPlayerExperience = experience;
		record.m_iSkillPoints = skillPoints;
		record.m_iTotalSkillPoints = totalSkillPoints;
	}

	//------------------------------------------------------------------------------------------------
	//! Write-through from EL_LicenseManagerComponent.UnlockLicense: the whole unlocked list.
	static void NoteLicenses(string playerUid, notnull array<EL_ELicenseType> licenses)
	{
		if (playerUid.IsEmpty())
			return;

		LF_ProgressionRecord record = Entry(playerUid);
		record.m_aLicenses.Clear();
		foreach (EL_ELicenseType license : licenses)
		{
			record.m_aLicenses.Insert(typename.EnumToString(EL_ELicenseType, license));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the stored licence names. Unknown names are skipped and logged.
	//! \return How many licences resolved.
	static int GetLicenses(string playerUid, notnull array<EL_ELicenseType> outLicenses)
	{
		outLicenses.Clear();
		LF_ProgressionRecord record = Get(playerUid);
		if (!record)
			return 0;

		foreach (string name : record.m_aLicenses)
		{
			EL_ELicenseType license = typename.StringToEnum(EL_ELicenseType, name);
			if (typename.EnumToString(EL_ELicenseType, license) != name)
			{
				EL_Debug.Warn("Progression", string.Format("saved licence '%1' for %2 no longer exists, skipped", name, playerUid));
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
		foreach (string ownerUid : owners)
		{
			AddUid(ownerUid, uids, seen);
		}

		EL_PlayerAccountManager accounts = EL_PlayerAccountManager.GetInstance();
		foreach (EL_PlayerAccountRecord accountRecord : EL_PlayerAccountManager.ExportAll())
		{
			if (!accountRecord || !accounts)
				continue;

			EL_PlayerAccount account = accounts.GetFromCache(accountRecord.m_sPersistentId);
			if (!account)
				continue;

			if (account.GetFaction() == EL_Faction.POLICE && account.IsDutyClockRunning())
				LF_PoliceRanks.SettleDutyAt(account, true, nowMs);

			if (account.GetPoliceRank() != 0 || account.GetOnDutySeconds() != 0 || account.GetUnpaidDutySeconds() != 0)
				AddUid(accountRecord.m_sPersistentId, uids, seen);
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
			{
				record.m_iCareerLevel = LF_CivilianJob.GetLevel(uid);
				record.m_fCareerExperience = LF_CivilianJob.GetExperience(uid);
				record.m_iCareerPaidTotal = LF_CivilianJob.GetPaidTotal(uid);
			}

			EL_PlayerAccount account;
			if (accounts)
				account = accounts.GetFromCache(uid);
			if (account)
			{
				record.m_iPoliceRank = account.GetPoliceRank();
				record.m_iOnDutySeconds = account.GetOnDutySeconds();
				record.m_iUnpaidDutySeconds = account.GetUnpaidDutySeconds();
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
		EL_PlayerAccountManager accounts = EL_PlayerAccountManager.GetInstance();

		int applied = 0;
		foreach (LF_ProgressionRecord record : records)
		{
			if (!record || record.m_sPlayerUid.IsEmpty())
			{
				EL_Debug.Warn("Progression", "skipped a progression record with no player id");
				continue;
			}

			string uid = record.m_sPlayerUid;
			s_mRecords.Set(uid, record.Copy());

			LF_CivilianJob.RestoreCareer(uid, record.m_iCareerLevel, record.m_fCareerExperience, record.m_iCareerPaidTotal);
			LF_UnemploymentBenefit.RestoreElapsedSinceCollection(uid, record.m_fBenefitElapsedSeconds, now);

			EL_PlayerAccount account;
			if (accounts)
				account = accounts.GetFromCache(uid);
			if (account)
			{
				account.SetPoliceRank(UsableRank(uid, record.m_iPoliceRank));
				account.SetOnDutySeconds(record.m_iOnDutySeconds);
				account.SetUnpaidDutySeconds(record.m_iUnpaidDutySeconds);
			}
			else if (record.m_iPoliceRank != 0 || record.m_iOnDutySeconds != 0)
			{
				EL_Debug.Warn("Progression", string.Format("police progression for %1 has no account, rank %2 not applied", uid, record.m_iPoliceRank));
			}

			applied++;
		}

		int bodies = ApplyToLivePlayers();
		EL_Debug.Info("Progression", string.Format("applied %1 progression record(s), %2 live body(ies)", applied, bodies));
	}

	//------------------------------------------------------------------------------------------------
	//! Gives a new body the player's saved level, licences, and turned-in quests.
	//! \return true when the store had a record for the player.
	static bool ApplyToCharacter(string playerUid, IEntity character)
	{
		if (!character || playerUid.IsEmpty())
			return false;

		LF_ProgressionRecord record = Get(playerUid);
		if (!record)
			return false;

		if (record.m_bHasLevel)
		{
			EL_PlayerLevelComponent level = EL_Component<EL_PlayerLevelComponent>.Find(character);
			if (level)
			{
				level.SetLevel(record.m_iPlayerLevel);
				level.SetExperience(record.m_fPlayerExperience);
				level.SetSkillPoints(record.m_iSkillPoints);
				level.SetTotalSkillPointsEarned(record.m_iTotalSkillPoints);
			}
		}

		if (!record.m_aLicenses.IsEmpty())
		{
			EL_LicenseManagerComponent licenseManager = EL_Component<EL_LicenseManagerComponent>.Find(character);
			if (licenseManager)
			{
				array<EL_ELicenseType> licenses = {};
				GetLicenses(playerUid, licenses);
				licenseManager.SetUnlockedLicenses(licenses);
			}
		}

		int quests = 0;
		if (!record.m_aTurnedInQuests.IsEmpty())
		{
			LF_QuestTrackerComponent tracker = EL_Component<LF_QuestTrackerComponent>.Find(character);
			if (tracker)
				quests = tracker.RestoreTurnedIn(record.m_aTurnedInQuests);
		}

		EL_Debug.Info("Progression", string.Format("progression applied to %1: level %2, %3 licence(s), %4 quest(s) restored", playerUid, record.m_iPlayerLevel, record.m_aLicenses.Count(), quests));
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

			if (ApplyToCharacter(EL_Utils.GetPlayerUID(playerId), body))
				bodies++;
		}

		return bodies;
	}

	//------------------------------------------------------------------------------------------------
	protected static int UsableRank(string playerUid, int rank)
	{
		if (LF_PoliceRanks.IsRankUsable(rank))
			return rank;

		int clamped = LF_PoliceRanks.PATROL;
		if (rank > LF_PoliceRanks.SENIOR)
			clamped = LF_PoliceRanks.SENIOR;

		EL_Debug.Warn("Progression", string.Format("saved police rank %1 for %2 is not on the ladder, using %3", rank, playerUid, clamped));
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
