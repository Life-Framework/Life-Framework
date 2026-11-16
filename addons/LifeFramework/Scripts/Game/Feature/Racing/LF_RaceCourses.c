// LF_RaceCourses.c - the race courses the running game has (#2006). The core ships no course:
// a course is pack data (the flagship's dirt loop is #1927). Courses layer the same way every
// content list does (docs/config/override-model.md): each pack overlay of LF_RaceCoursesConfig in
// load order, then the owner's "racing" section. Every course then goes through
// LF_RaceCourse.IsValid; a bad row is logged and dropped, never kept and never a crash.

//------------------------------------------------------------------------------------------------
//! The courses list as the shared override model sees it (LF_ConfigOverlay), keyed by course id.
class LF_RaceCourseList : LF_ConfigList
{
	protected array<ref LF_RaceCourseRecord> m_aRecords;

	void LF_RaceCourseList(notnull array<ref LF_RaceCourseRecord> records)
	{
		m_aRecords = records;
	}

	override string GetName()
	{
		return "racing.courses";
	}

	override int Count()
	{
		return m_aRecords.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_aRecords[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_RaceCourseRecord.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a course id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_RaceCourseRecord record = new LF_RaceCourseRecord();
		record.m_sId = id;
		record.m_aCheckpoints = {};
		record.m_aPayoutPercent = {};
		record.m_iLaps = 1;
		record.m_iMinEntrants = 2;
		record.m_iLobbySeconds = 60;
		return record;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_RaceCourseRecord course = LF_RaceCourseRecord.Cast(record);
		if (index < m_aRecords.Count())
			m_aRecords.Set(index, course);
		else
			m_aRecords.Insert(course);
	}

	override void RemoveAt(int index)
	{
		m_aRecords.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_RaceCourseRecord course = LF_RaceCourseRecord.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_aCheckpoints"))
		{
			array<string> checkpoints = {};
			if (container.Get("m_aCheckpoints", checkpoints))
			{
				course.m_aCheckpoints = {};
				course.m_aCheckpoints.Copy(checkpoints);
				changed++;
			}
		}

		if (container.IsVariableSetDirectly("m_aPayoutPercent"))
		{
			array<int> payout = {};
			if (container.Get("m_aPayoutPercent", payout))
			{
				course.m_aPayoutPercent = {};
				course.m_aPayoutPercent.Copy(payout);
				changed++;
			}
		}

		if (container.IsVariableSetDirectly("m_iLaps") && container.Get("m_iLaps", course.m_iLaps))
			changed++;
		if (container.IsVariableSetDirectly("m_iEntryFee") && container.Get("m_iEntryFee", course.m_iEntryFee))
			changed++;
		if (container.IsVariableSetDirectly("m_iMinEntrants") && container.Get("m_iMinEntrants", course.m_iMinEntrants))
			changed++;
		if (container.IsVariableSetDirectly("m_iLobbySeconds") && container.Get("m_iLobbySeconds", course.m_iLobbySeconds))
			changed++;
		return changed;
	}

	override void PatchFromReader(notnull LF_ConfigRecord record, notnull LF_ConfigReader reader)
	{
		LF_RaceCourseRecord course = LF_RaceCourseRecord.Cast(record);
		array<string> recordKeys = {"op", "checkpoints", "laps", "entryFee", "minEntrants", "lobbySeconds", "payoutPercent"};
		reader.WarnUnknownKeys(recordKeys);

		array<string> checkpoints = {};
		if (reader.ReadStrings("checkpoints", checkpoints))
		{
			course.m_aCheckpoints = {};
			course.m_aCheckpoints.Copy(checkpoints);
		}

		array<int> payout = {};
		if (reader.ReadInts("payoutPercent", payout, 0, 100))
		{
			course.m_aPayoutPercent = {};
			course.m_aPayoutPercent.Copy(payout);
		}

		reader.ReadInt("laps", course.m_iLaps, LF_RaceCourses.LAPS_MIN, LF_RaceCourses.LAPS_MAX);
		reader.ReadInt("entryFee", course.m_iEntryFee, 0, LF_RaceCourses.FEE_MAX);
		reader.ReadInt("minEntrants", course.m_iMinEntrants, 1, LF_RaceCourses.ENTRANTS_MAX);
		reader.ReadInt("lobbySeconds", course.m_iLobbySeconds, 0, LF_RaceCourses.LOBBY_MAX);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_RaceCourseRecord course = LF_RaceCourseRecord.Cast(record);
		if (course.m_sId.IsEmpty())
			return "a course needs an id";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! The layered courses, by id. Holds only courses that passed LF_RaceCourse.IsValid.
class LF_RaceCourseBook
{
	protected ref array<ref LF_RaceCourseRecord> m_aCourses = {};
	protected int m_iDropped;

	//------------------------------------------------------------------------------------------------
	array<ref LF_RaceCourseRecord> GetRecords()
	{
		return m_aCourses;
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_aCourses.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Rows dropped for breaking the course rules over this book's life.
	int GetDroppedCount()
	{
		return m_iDropped;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The course with this id, or null.
	LF_RaceCourseRecord Find(string id)
	{
		if (id.IsEmpty())
			return null;

		foreach (LF_RaceCourseRecord course : m_aCourses)
		{
			if (course && course.m_sId == id)
				return course;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the courses of one config (a pack file, or one built in script) under the rules: a null row,
	//! a course whose id is already here and a course that is not valid are logged and dropped.
	void AddConfig(LF_RaceCoursesConfig config, string source = "")
	{
		if (!config || !config.m_aCourses)
			return;

		foreach (LF_RaceCourseRecord course : config.m_aCourses)
		{
			if (!course)
			{
				LF_Debug.Error(LF_RaceCourses.FEATURE, string.Format("%1: an empty course row is dropped", source));
				m_iDropped++;
				continue;
			}

			if (Find(course.m_sId))
			{
				LF_Debug.Error(LF_RaceCourses.FEATURE, string.Format("%1: course '%2' is listed twice, the second is dropped", source, course.m_sId));
				m_iDropped++;
				continue;
			}

			m_aCourses.Insert(course);
		}

		Validate();
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "racing" section: `courses` is an object keyed by course id, each record
	//! with an optional "op" (add, replace, patch, remove).
	void ApplyOwner(notnull LF_ConfigReader reader, string source = "config.json")
	{
		array<string> ownerKeys = {"courses"};
		reader.WarnUnknownKeys(ownerKeys);
		LF_ConfigOverlay.ApplyRecords(reader, "courses", new LF_RaceCourseList(m_aCourses), source);
		Validate();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every course that fails LF_RaceCourse.IsValid (which logs the reason). \return How many were dropped.
	int Validate()
	{
		int dropped;
		for (int i = m_aCourses.Count() - 1; i >= 0; i--)
		{
			if (LF_RaceCourse.IsValid(m_aCourses[i]))
				continue;

			m_aCourses.RemoveOrdered(i);
			dropped++;
		}

		m_iDropped += dropped;
		return dropped;
	}
}

//------------------------------------------------------------------------------------------------
//! The one place a race course is looked up. Server logic; a client needing a course asks the race.
class LF_RaceCourses
{
	static const string FEATURE = "Racing";

	//! The owner config.json section this feature reads (LF_ConfigService).
	static const string SECTION = "racing";

	// Owner-file limits; the course rules (LF_RaceCourse.IsValid) still apply on top.
	static const int LAPS_MIN = 1;	// lf-fixed: the owner-file schema limit
	static const int LAPS_MAX = 50;	// lf-fixed: the owner-file schema limit
	static const int FEE_MAX = 1000000;	// lf-fixed: the owner-file schema limit
	static const int ENTRANTS_MAX = 64;	// lf-fixed: the owner-file schema limit
	static const int LOBBY_MAX = 3600;	// lf-fixed: the owner-file schema limit

	protected static ref LF_RaceCourseBook s_Book;

	//------------------------------------------------------------------------------------------------
	//! The layered book, built once. Empty (and logged) when no pack adds a course.
	static LF_RaceCourseBook Get()
	{
		if (s_Book)
			return s_Book;

		s_Book = new LF_RaceCourseBook();
		ApplyLayers(s_Book);
		if (s_Book.Count() < 1)
			LF_Debug.Info(FEATURE, "no race course loaded: the pack adds none");

		return s_Book;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers the pack overlays, then the owner's section, over an empty book (the core has no course).
	static void ApplyLayers(notnull LF_RaceCourseBook book)
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(ResourceName.Empty, LF_RaceCoursesConfig, SECTION, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(new LF_RaceCourseList(book.GetRecords()), resource.GetResource().ToBaseContainer(), "m_aCourses", names[i], claims);
			int dropped = book.Validate();
			LF_Debug.Info(FEATURE, string.Format("racing: pack overlay %1 changed %2 courses, dropped %3", names[i], changed, dropped));
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (reader)
		{
			book.ApplyOwner(reader, LF_ConfigService.GetOwnerSource());
			LF_Debug.Info(FEATURE, string.Format("racing: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The course with this id, or null (logged when the id is not empty).
	static LF_RaceCourseRecord Find(string id)
	{
		LF_RaceCourseRecord course = Get().Find(id);
		if (!course && !id.IsEmpty())
			LF_Debug.Warn(FEATURE, string.Format("race course '%1' is not loaded", id));

		return course;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached book so the next read layers everything again (/configreload).
	static void Reload()
	{
		s_Book = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `book` the one Get returns until Reload.
	static void UseForTest(LF_RaceCourseBook book)
	{
		s_Book = book;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_RaceCourses's statics (registered at script load; Core/LF_StaticReset.c).
class LF_RaceCourses_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_RaceCourses_StaticResetHook());

	override string Owner()
	{
		return "LF_RaceCourses";
	}

	override void Apply()
	{
		LF_RaceCourses.Reload();
	}
}
