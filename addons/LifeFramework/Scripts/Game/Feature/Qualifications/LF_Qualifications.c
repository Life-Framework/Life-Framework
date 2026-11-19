//------------------------------------------------------------------------------------------------
//! The courses each character holds (#3425, slice Q2 of #1035), keyed by the character's record key
//! (LF_PlayerCharacter.GetRecordKey). A held course is remembered by its course id; what it unlocks is
//! read from the course table (LF_QualificationConfig) each time, so a pack that changes a course's
//! unlocks changes them for everyone who holds it.
//!
//! Server state. Nothing here checks who may take a course (LF_QualificationRules does), charges a fee
//! or times the study: the course desk calls Grant once the course is complete. Saved by the
//! "qualifications" save section (LF_QualificationsSavePayload).
//------------------------------------------------------------------------------------------------
class LF_Qualifications
{
	//! Character record key -> held course ids, in the order granted.
	protected static ref map<string, ref array<string>> s_mHeld;

	//------------------------------------------------------------------------------------------------
	//! World-end reset (LF_StaticReset).
	static void ResetStatics()
	{
		s_mHeld = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref array<string>> Held()
	{
		if (!s_mHeld)
			s_mHeld = new map<string, ref array<string>>();

		return s_mHeld;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the character the course. Refused (and logged) for an empty key, a course the table does
	//! not list, or a course already held.
	//! \return True when the course was added.
	static bool Grant(string character, string courseId)
	{
		if (character.IsEmpty())
		{
			LF_Debug.Warn(LF_QualificationConfig.FEATURE, string.Format("grant of '%1' refused: no character key", courseId));
			return false;
		}

		LF_QualificationConfig config = LF_QualificationConfig.Get();
		if (!config || !config.Find(courseId))
		{
			LF_Debug.Warn(LF_QualificationConfig.FEATURE, string.Format("grant to %1 refused: unknown course '%2'", character, courseId));
			return false;
		}

		array<string> courses = Held().Get(character);
		if (!courses)
		{
			courses = new array<string>();
			Held().Insert(character, courses);
		}

		if (courses.Contains(courseId))
		{
			LF_Debug.Info(LF_QualificationConfig.FEATURE, string.Format("grant to %1 skipped: '%2' is already held", character, courseId));
			return false;
		}

		courses.Insert(courseId);
		LF_Debug.Info(LF_QualificationConfig.FEATURE, string.Format("%1 now holds '%2'", character, courseId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the character holds a course whose unlock ids include `unlock`.
	static bool Has(string character, string unlock)
	{
		if (!s_mHeld || character.IsEmpty() || unlock.IsEmpty())
			return false;

		array<string> courses = s_mHeld.Get(character);
		if (!courses)
			return false;

		LF_QualificationConfig config = LF_QualificationConfig.Get();
		if (!config)
			return false;

		foreach (string courseId : courses)
		{
			LF_QualificationCourse course = config.Find(courseId);
			if (!course)
				continue;

			array<string> unlocks = {};
			course.GetUnlocks(unlocks);
			if (unlocks.Contains(unlock))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the character holds that course id.
	static bool HasCourse(string character, string courseId)
	{
		if (!s_mHeld)
			return false;

		array<string> courses = s_mHeld.Get(character);
		return courses && courses.Contains(courseId);
	}

	//------------------------------------------------------------------------------------------------
	//! The character's held course ids (the `held` of LF_QualificationRules.Check).
	static void GetHeld(string character, notnull array<string> outCourses)
	{
		outCourses.Clear();
		if (!s_mHeld)
			return;

		array<string> courses = s_mHeld.Get(character);
		if (courses)
			outCourses.Copy(courses);
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: one row per held course.
	static array<ref LF_QualificationRow> ExportAll()
	{
		array<ref LF_QualificationRow> rows = new array<ref LF_QualificationRow>();
		if (!s_mHeld)
			return rows;

		foreach (string character, array<string> courses : s_mHeld)
		{
			if (!courses)
				continue;

			foreach (string courseId : courses)
			{
				rows.Insert(LF_QualificationRow.Create(character, courseId));
			}
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: replaces every held course with the rows (a load, or a re-apply to a live
	//! session, ends in the same state). A row with no key, a course the table no longer lists, or a
	//! repeat is dropped and logged.
	//! \return Rows applied.
	static int ApplyAll(notnull array<ref LF_QualificationRow> rows)
	{
		s_mHeld = new map<string, ref array<string>>();
		LF_QualificationConfig config = LF_QualificationConfig.Get();
		int applied = 0;
		foreach (LF_QualificationRow row : rows)
		{
			if (!row || row.m_sKey.IsEmpty())
			{
				LF_Debug.Warn(LF_QualificationConfig.FEATURE, "dropped a saved qualification with no character key");
				continue;
			}

			if (!config || !config.Find(row.m_sCourse))
			{
				LF_Debug.Warn(LF_QualificationConfig.FEATURE, string.Format("dropped saved qualification '%1' of %2: the course table does not list it", row.m_sCourse, row.m_sKey));
				continue;
			}

			array<string> courses = s_mHeld.Get(row.m_sKey);
			if (!courses)
			{
				courses = new array<string>();
				s_mHeld.Insert(row.m_sKey, courses);
			}

			if (courses.Contains(row.m_sCourse))
				continue;

			courses.Insert(row.m_sCourse);
			applied++;
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Nobody holds anything (a save from before qualifications).
	static void ClearAll()
	{
		s_mHeld = new map<string, ref array<string>>();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Qualifications's statics (LF_StaticReset finds it by reflection).
class LF_Qualifications_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_Qualifications";
	}

	override void Apply()
	{
		LF_Qualifications.ResetStatics();
	}
}
