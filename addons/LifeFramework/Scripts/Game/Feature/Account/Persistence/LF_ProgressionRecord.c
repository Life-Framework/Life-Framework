//------------------------------------------------------------------------------------------------
//! Persisted progression of one player, keyed by the player uid (the account key). Saved as the
//! "progression" payload (save version 7) by EL_PersistenceComponentSerializer.
//!
//! Career, police, and benefit fields mirror state owned elsewhere (LF_CivilianJob,
//! EL_PlayerAccount, LF_UnemploymentBenefit). Level, licences, and quest turn-ins live on the
//! character entity, so LF_Progression keeps them here between bodies.
//!
//! Licences are stored by enum name, not by EL_LicenseBits position, so a reordered or renamed
//! licence list drops one unknown name instead of shifting every saved bit.
//------------------------------------------------------------------------------------------------
class LF_ProgressionRecord : Managed
{
	static const int VERSION = 1;

	int m_iVersion = VERSION;
	string m_sPlayerUid;

	// Civilian delivery career (LF_CivilianJob).
	int m_iCareerLevel = 1;
	float m_fCareerExperience;
	int m_iCareerPaidTotal;

	// Police (EL_PlayerAccount).
	int m_iPoliceRank;
	int m_iOnDutySeconds;
	int m_iUnpaidDutySeconds;

	//! Seconds between the last benefit collection and the save. -1 = never collected.
	float m_fBenefitElapsedSeconds = -1;

	// Player level (EL_PlayerLevelComponent). Applied only when m_bHasLevel.
	bool m_bHasLevel;
	int m_iPlayerLevel = 1;
	float m_fPlayerExperience;
	int m_iSkillPoints;
	int m_iTotalSkillPoints;

	//! EL_ELicenseType names. Empty = the body keeps its starter licences.
	ref array<string> m_aLicenses = {};
	ref array<string> m_aTurnedInQuests = {};

	//------------------------------------------------------------------------------------------------
	static LF_ProgressionRecord Create(string playerUid)
	{
		LF_ProgressionRecord record();
		record.m_iVersion = VERSION;
		record.m_sPlayerUid = playerUid;
		record.m_aLicenses = {};
		record.m_aTurnedInQuests = {};
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A deep copy, so the store never shares arrays with a caller's record.
	LF_ProgressionRecord Copy()
	{
		LF_ProgressionRecord copy = Create(m_sPlayerUid);
		copy.m_iCareerLevel = m_iCareerLevel;
		copy.m_fCareerExperience = m_fCareerExperience;
		copy.m_iCareerPaidTotal = m_iCareerPaidTotal;
		copy.m_iPoliceRank = m_iPoliceRank;
		copy.m_iOnDutySeconds = m_iOnDutySeconds;
		copy.m_iUnpaidDutySeconds = m_iUnpaidDutySeconds;
		copy.m_fBenefitElapsedSeconds = m_fBenefitElapsedSeconds;
		copy.m_bHasLevel = m_bHasLevel;
		copy.m_iPlayerLevel = m_iPlayerLevel;
		copy.m_fPlayerExperience = m_fPlayerExperience;
		copy.m_iSkillPoints = m_iSkillPoints;
		copy.m_iTotalSkillPoints = m_iTotalSkillPoints;
		if (m_aLicenses)
		{
			foreach (string license : m_aLicenses)
			{
				copy.m_aLicenses.Insert(license);
			}
		}

		if (m_aTurnedInQuests)
		{
			foreach (string questId : m_aTurnedInQuests)
			{
				copy.m_aTurnedInQuests.Insert(questId);
			}
		}

		return copy;
	}
}
