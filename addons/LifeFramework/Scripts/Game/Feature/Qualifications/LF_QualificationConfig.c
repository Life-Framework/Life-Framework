//------------------------------------------------------------------------------------------------
//! One course a character can take (Configs/Qualifications/LF_Qualifications.conf). The id is the
//! course id. A course is open to one faction at a minimum rank, costs a fee in carried cash, takes
//! study time, and on completion unlocks ids that other features check by string (`detective`,
//! `traffic`), never a number. Core ships neutral courses; a pack adds, replaces, patches or
//! removes rows by id through its mission header (LF_ConfigOverlay).
[BaseContainerProps()]
class LF_QualificationCourse : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Course id, e.g. detective_course. Unique; a held course is remembered by it")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "Faction id the course is open to (police, medic, ...). Empty is open to every faction")]
	string m_sFaction;

	[Attribute("0", UIWidgets.Auto, "Lowest rank (0 is the entry rank) that may take the course", params: "0 20")]
	int m_iMinRank;

	[Attribute("0", UIWidgets.Auto, "Fee in carried cash", params: "0 1000000")]
	int m_iFee;

	[Attribute("60", UIWidgets.Auto, "Study time in in-game minutes before the course is complete", params: "0 100000")]
	int m_iStudyMinutes;

	[Attribute("", UIWidgets.EditBox, "Ids the course unlocks, comma separated (detective,traffic). Other features check these strings")]
	string m_sUnlocks;

	//------------------------------------------------------------------------------------------------
	static LF_QualificationCourse Make(string id)
	{
		LF_QualificationCourse course = new LF_QualificationCourse();
		course.m_sId = id;
		return course;
	}

	//------------------------------------------------------------------------------------------------
	//! The unlock ids, trimmed, empty entries dropped, in the order written.
	void GetUnlocks(notnull array<string> unlocks)
	{
		array<string> parts = {};
		m_sUnlocks.Split(",", parts, true);
		foreach (string part : parts)
		{
			string id = part.Trim();
			if (!id.IsEmpty())
				unlocks.Insert(id);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! The course table: Configs/Qualifications/LF_Qualifications.conf with each pack overlay layered
//! over it (LF_ConfigOverlay, #962). No owner section: the courses are content, set by the pack.
[BaseContainerProps(configRoot: true)]
class LF_QualificationConfig
{
	static const string FEATURE = "Qualifications";
	static const ResourceName CONFIG_PATH = "{4C1A9E7D30B58F26}Configs/Qualifications/LF_Qualifications.conf";

	[Attribute("", UIWidgets.Object, "One row per course")]
	ref array<ref LF_QualificationCourse> m_aCourses;

	protected static ref LF_QualificationConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered table, or null (logged once) when the core file does not resolve.
	static LF_QualificationConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("course table does not resolve, no course can be taken: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_QualificationConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(FEATURE, string.Format("course table has an unexpected root class: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached table so the next Get() loads and layers it again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of this class, in load order, over this instance.
	void ApplyLayers()
	{
		if (!m_aCourses)
			m_aCourses = new array<ref LF_QualificationCourse>();

		LF_QualificationCourseList list = new LF_QualificationCourseList(this);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_QualificationConfig, "qualifications", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aCourses", names[i], claims);
			LF_Debug.Info(FEATURE, string.Format("pack overlay %1 changed %2 course rows", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The course with this id, or null.
	LF_QualificationCourse Find(string id)
	{
		if (!m_aCourses || id.IsEmpty())
			return null;

		foreach (LF_QualificationCourse course : m_aCourses)
		{
			if (course && course.m_sId == id)
				return course;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_QualificationConfig's cache (LF_StaticReset finds it by reflection).
class LF_QualificationConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_QualificationConfig";
	}

	override void Apply()
	{
		LF_QualificationConfig.Reload();
	}
}

//------------------------------------------------------------------------------------------------
//! The course table's `m_aCourses` list for the shared merge (LF_ConfigOverlay), keyed by course id.
class LF_QualificationCourseList : LF_ConfigList
{
	protected LF_QualificationConfig m_Config;

	void LF_QualificationCourseList(LF_QualificationConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "qualifications.courses";
	}

	override int Count()
	{
		return m_Config.m_aCourses.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aCourses[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_QualificationCourse.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a course id such as detective_course";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_QualificationCourse.Make(id);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_QualificationCourse course = LF_QualificationCourse.Cast(record);
		if (index < m_Config.m_aCourses.Count())
			m_Config.m_aCourses.Set(index, course);
		else
			m_Config.m_aCourses.Insert(course);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aCourses.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_QualificationCourse course = LF_QualificationCourse.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sFaction") && container.Get("m_sFaction", course.m_sFaction))
			changed++;
		if (container.IsVariableSetDirectly("m_iMinRank") && container.Get("m_iMinRank", course.m_iMinRank))
			changed++;
		if (container.IsVariableSetDirectly("m_iFee") && container.Get("m_iFee", course.m_iFee))
			changed++;
		if (container.IsVariableSetDirectly("m_iStudyMinutes") && container.Get("m_iStudyMinutes", course.m_iStudyMinutes))
			changed++;
		if (container.IsVariableSetDirectly("m_sUnlocks") && container.Get("m_sUnlocks", course.m_sUnlocks))
			changed++;
		return changed;
	}
}
