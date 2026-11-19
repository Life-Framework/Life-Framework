//------------------------------------------------------------------------------------------------
//! One held course of one character, as saved.
class LF_QualificationRow : Managed
{
	static const int ROW_VERSION = 1;

	//! Row format, written first. 1 is this shape.
	int m_iVersion;
	//! The character's record key.
	string m_sKey;
	//! The course id (LF_QualificationCourse.m_sId).
	string m_sCourse;

	//------------------------------------------------------------------------------------------------
	static LF_QualificationRow Create(string key, string courseId)
	{
		LF_QualificationRow row = new LF_QualificationRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sKey = key;
		row.m_sCourse = courseId;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "qualifications" (#3425): one LF_QualificationRow per held course. A course the
//! table no longer lists is dropped on load and logged.
//------------------------------------------------------------------------------------------------
class LF_QualificationsSavePayload : LF_SavePayload
{
	static const string NAME = "qualifications";

	//------------------------------------------------------------------------------------------------
	override string Name()
	{
		return NAME;
	}

	//------------------------------------------------------------------------------------------------
	override int Version()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_QualificationRow> rows = LF_Qualifications.ExportAll();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 qualification row(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_QualificationRow> rows = new array<ref LF_QualificationRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_QualificationConfig.FEATURE, "qualifications failed to read, held courses stay as they were");
			return false;
		}

		int kept = LF_Qualifications.ApplyAll(rows);
		LF_Debug.Info("Persistence", string.Format("applied %1 of %2 qualification row(s)", kept, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before qualifications: nothing to seed. A boot starts with nobody holding a course,
	//! and a re-apply to a live session keeps the live set (the save holds none to replace it with).
	override void OnAbsent()
	{
		LF_Debug.Info(LF_QualificationConfig.FEATURE, "save has no qualifications; held courses stay as the session holds them (none on a boot)");
	}
}
