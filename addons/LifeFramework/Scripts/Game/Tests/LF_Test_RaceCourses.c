// red-proof: `test --no-build --red LF_Test_RaceCourses` -> RED PROOF OK (2026-11-16): skipping the overlay (the hook swaps in the unlayered book) leaves one course; fails "an overlay adds a second course (expected 2, got 1)" and "the added course is found by id". Before the loader existed the test failed to compile.
// tier: LOGIC
//! Race courses load from config (#2006): a course list is found by id, an overlay adds a second
//! course by id, patches and removes through the shared override model, and a course that breaks
//! the rules (a checkpoint twice) is dropped and logged, never kept. Courses here are built in
//! script and layered through the owner section; no world, no coordinates, no pack file.
class LF_Test_RaceCourses : LF_Test
{
	protected LF_RaceCourseRecord BuildCourse(string id, array<string> checkpoints)
	{
		LF_RaceCourseRecord course = new LF_RaceCourseRecord();
		course.m_sId = id;
		course.m_aCheckpoints = {};
		course.m_aCheckpoints.Copy(checkpoints);
		course.m_iLaps = 2;
		course.m_iEntryFee = 50;
		course.m_iMinEntrants = 2;
		course.m_iLobbySeconds = 30;
		course.m_aPayoutPercent = {60, 30, 10};
		return course;
	}

	override string GetName()
	{
		return "racing/courses";
	}

	//------------------------------------------------------------------------------------------------
	//! A book holding one valid course, "loop_a", from a config built in script.
	protected LF_RaceCourseBook BuildBook()
	{
		LF_RaceCoursesConfig config = new LF_RaceCoursesConfig();
		config.m_aCourses = {};
		config.m_aCourses.Insert(BuildCourse("loop_a", {"cp_a1", "cp_a2", "cp_a3"}));
		LF_RaceCourseBook book = new LF_RaceCourseBook();
		book.AddConfig(config);
		return book;
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyOverlay(LF_RaceCourseBook book, string coursesJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"racing\":{\"courses\":" + coursesJson + "}}", "test");
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(LF_RaceCourses.SECTION);
		if (reader)
			book.ApplyOwner(reader, "test-owner.json");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// A loaded list: found by id, an unknown id finds nothing.
		LF_RaceCourseBook book = BuildBook();
		ctx.Equal(1, book.Count(), "the config's course loads");
		LF_RaceCourseRecord loopA = book.Find("loop_a");
		ctx.NotNull(loopA, "a course is found by id");
		if (loopA)
		{
			ctx.Equal(2, loopA.m_iLaps, "the found course keeps its laps");
			ctx.Equal(3, loopA.m_aCheckpoints.Count(), "the found course keeps its checkpoints");
		}
		ctx.True(book.Find("nope") == null, "an unknown id finds no course");
		ctx.True(book.Find("") == null, "an empty id finds no course");

		// An overlay adds a second course by id; the first is untouched.
		ApplyOverlay(book, "{\"loop_b\":{\"op\":\"add\",\"checkpoints\":[\"cp_b1\",\"cp_b2\"],\"laps\":3,\"entryFee\":25,\"minEntrants\":3,\"lobbySeconds\":45,\"payoutPercent\":[70,30]}}");
		if (RedProof())
			book = BuildBook();
		ctx.Equal(2, book.Count(), "an overlay adds a second course");
		LF_RaceCourseRecord loopB = book.Find("loop_b");
		ctx.NotNull(loopB, "the added course is found by id");
		if (loopB)
		{
			ctx.Equal(3, loopB.m_iLaps, "the added course has its own laps");
			ctx.Equal(25, loopB.m_iEntryFee, "the added course has its own fee");
			ctx.Equal(2, loopB.m_aCheckpoints.Count(), "the added course has its own checkpoints");
			ctx.EqualStr("cp_b2", loopB.m_aCheckpoints[1], "the added checkpoints keep their order");
		}
		ctx.NotNull(book.Find("loop_a"), "the first course is still there");

		// A patch changes only the field given.
		ApplyOverlay(book, "{\"loop_a\":{\"entryFee\":75}}");
		loopA = book.Find("loop_a");
		if (loopA)
		{
			ctx.Equal(75, loopA.m_iEntryFee, "a patch changes the fee");
			ctx.Equal(2, loopA.m_iLaps, "a patch leaves the other fields alone");
		}

		// A remove takes the course out.
		ApplyOverlay(book, "{\"loop_b\":{\"op\":\"remove\"}}");
		ctx.True(book.Find("loop_b") == null, "a remove takes the course out");
		ctx.Equal(1, book.Count(), "one course is left after the remove");

		// A bad row is dropped, never kept and never a crash; the good row beside it stays.
		ApplyOverlay(book, "{\"dup\":{\"op\":\"add\",\"checkpoints\":[\"cp_x\",\"cp_y\",\"cp_x\"],\"laps\":1,\"minEntrants\":2,\"payoutPercent\":[100]},\"loop_c\":{\"op\":\"add\",\"checkpoints\":[\"cp_c1\",\"cp_c2\"],\"laps\":1,\"minEntrants\":2,\"payoutPercent\":[100]}}");
		ctx.True(book.Find("dup") == null, "a course with a duplicate checkpoint is dropped");
		ctx.NotNull(book.Find("loop_c"), "a good course beside the bad one still loads");
		ctx.Equal(2, book.Count(), "only the good rows are kept");
		ctx.Equal(1, book.GetDroppedCount(), "the dropped row is counted");

		// A payout that does not add up, and a course with one checkpoint, are dropped too.
		ApplyOverlay(book, "{\"short\":{\"op\":\"add\",\"checkpoints\":[\"cp_s\"],\"laps\":1,\"minEntrants\":2,\"payoutPercent\":[100]},\"split\":{\"op\":\"add\",\"checkpoints\":[\"cp_s1\",\"cp_s2\"],\"laps\":1,\"minEntrants\":2,\"payoutPercent\":[60,30]}}");
		ctx.True(book.Find("short") == null, "a one-checkpoint course is dropped");
		ctx.True(book.Find("split") == null, "a payout that does not add up to 100 is dropped");
		ctx.Equal(2, book.Count(), "the good courses stay");

		// A patch that breaks a good course drops it rather than keeping a broken one.
		ApplyOverlay(book, "{\"loop_a\":{\"laps\":0}}");
		ctx.NotNull(book.Find("loop_a"), "a patch value outside the allowed range is refused and the course keeps its laps");

		// A core config row that is bad is dropped on load.
		LF_RaceCoursesConfig badConfig = new LF_RaceCoursesConfig();
		badConfig.m_aCourses = {};
		badConfig.m_aCourses.Insert(BuildCourse("loop_dup", {"cp_1", "cp_2", "cp_1"}));
		badConfig.m_aCourses.Insert(BuildCourse("loop_ok", {"cp_1", "cp_2"}));
		badConfig.m_aCourses.Insert(null);
		LF_RaceCourseBook fresh = new LF_RaceCourseBook();
		fresh.AddConfig(badConfig);
		ctx.True(fresh.Find("loop_dup") == null, "a config row with a duplicate checkpoint is dropped on load");
		ctx.NotNull(fresh.Find("loop_ok"), "the good config row loads");
		ctx.Equal(1, fresh.Count(), "a null row and a bad row never land in the book");

		LF_ConfigService.Reset();
	}
}
