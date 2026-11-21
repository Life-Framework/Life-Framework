//------------------------------------------------------------------------------------------------
//! The verdict of LF_QualificationRules.Check. A pass carries the course's unlock ids. A refusal
//! carries a localization key (the REASON_ constants) and, when the sentence has a number in it, that
//! number as `m_iNumber` (the %1 of the string).
class LF_QualificationVerdict
{
	bool m_bOk;
	string m_sCourseId;
	//! The refusal's localization key; empty on a pass.
	string m_sReason;
	//! The %1 of the refusal (the rank needed, the fee); 0 when the sentence has none.
	int m_iNumber;
	//! The fee a pass costs and the study time it starts, so the caller charges and times it.
	int m_iFee;
	int m_iStudyMinutes;
	ref array<string> m_aUnlocks = {};
}

//------------------------------------------------------------------------------------------------
//! Who may take which course (#3424). Pure rules over plain values: the caller (the course desk,
//! a later slice) reads the character's faction, rank, held course ids and carried cash, and acts
//! on the verdict. Nothing here touches an entity, takes money or saves.
class LF_QualificationRules
{
	static const string REASON_UNKNOWN = "#LF-Qual_Unknown";
	static const string REASON_HELD = "#LF-Qual_Held";
	static const string REASON_FACTION = "#LF-Qual_WrongFaction";
	static const string REASON_RANK = "#LF-Qual_RankTooLow";
	static const string REASON_PURSE = "#LF-Qual_ShortPurse";

	//------------------------------------------------------------------------------------------------
	//! Whether a character of `faction` and `rank`, already holding the courses in `held`, with
	//! `cash` carried, may take `courseId`. Checked in this order, the first failure wins: the
	//! course exists, it is not already held, the faction fits, the rank is high enough, the purse
	//! covers the fee. A refusal is logged; nothing degrades into a throw.
	static LF_QualificationVerdict Check(LF_QualificationConfig config, string courseId, string faction, int rank, notnull array<string> held, int cash)
	{
		LF_QualificationVerdict verdict = new LF_QualificationVerdict();
		verdict.m_sCourseId = courseId;

		LF_QualificationCourse course;
		if (config)
			course = config.Find(courseId);

		if (!course)
			return Refuse(verdict, REASON_UNKNOWN, 0, string.Format("unknown course '%1'", courseId));

		if (held.Find(courseId) != -1)
			return Refuse(verdict, REASON_HELD, 0, string.Format("course '%1' is already held", courseId));

		if (!course.m_sFaction.IsEmpty() && course.m_sFaction != faction)
			return Refuse(verdict, REASON_FACTION, 0, string.Format("course '%1' is for faction '%2', not '%3'", courseId, course.m_sFaction, faction));

		if (rank < course.m_iMinRank)
			return Refuse(verdict, REASON_RANK, course.m_iMinRank, string.Format("course '%1' needs rank %2, has %3", courseId, course.m_iMinRank, rank));

		if (cash < course.m_iFee)
			return Refuse(verdict, REASON_PURSE, course.m_iFee, string.Format("course '%1' costs %2, has %3", courseId, course.m_iFee, cash));

		verdict.m_bOk = true;
		verdict.m_iFee = course.m_iFee;
		verdict.m_iStudyMinutes = course.m_iStudyMinutes;
		course.GetUnlocks(verdict.m_aUnlocks);
		return verdict;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_QualificationVerdict Refuse(notnull LF_QualificationVerdict verdict, string reason, int number, string why)
	{
		verdict.m_bOk = false;
		verdict.m_sReason = reason;
		verdict.m_iNumber = number;
		LF_Debug.Info(LF_QualificationConfig.FEATURE, "refused: " + why);
		return verdict;
	}
}
