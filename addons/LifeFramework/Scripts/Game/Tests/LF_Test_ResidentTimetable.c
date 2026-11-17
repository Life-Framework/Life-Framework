// red-proof: RED-OK residents/resident-timetable (fails with the hook on, 2 assertions; the hook skips building the day)
// tier: LOGIC
//! Resident cast and timetable as data (#2665): a resident's day resolves by game minute, a bad day
//! is refused at load naming the resident, and an unknown place kind drops its line.

//! A world that has these place kinds and no others.
class LF_Test_ResidentKinds : LF_ResidentKindCheck
{
	override bool Knows(string kind)
	{
		return kind == "t_home" || kind == "t_work";
	}
}

class LF_Test_ResidentTimetable : LF_Test
{
	override string GetName()
	{
		return "residents/resident-timetable";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ResidentTimetable.ResetStatics();
		LF_Test_ResidentKinds kinds = new LF_Test_ResidentKinds();

		LF_ResidentDef baker = LF_ResidentDef.Make("t_baker", "t_home", "t_work");
		baker.Add("06:00", "home", "bench").Add("07:00", "work", "counter", "stand").Add("18:00", "home", "bench");

		// Red-proof hook: with --red the day is never built, so every assertion on it must fail.
		LF_ResidentTimetable day;
		if (!RedProof())
			day = LF_ResidentTimetable.Build(baker, kinds);

		ctx.True(day != null, "a good day builds");
		if (!day)
			return;

		ctx.True(day.Count() == 3, "all three lines are kept");
		ctx.True(day.Where(LF_ResidentTimetable.MinuteOf("07:30")) == "work", "07:30 reads work");
		ctx.True(day.Where(LF_ResidentTimetable.MinuteOf("23:00")) == "home", "23:00 reads home");
		ctx.True(day.Where(LF_ResidentTimetable.MinuteOf("06:00")) == "home", "a line starts on its minute");
		ctx.True(day.Where(LF_ResidentTimetable.MinuteOf("06:59")) == "home", "06:59 is still the first line");
		ctx.True(day.Where(LF_ResidentTimetable.MinuteOf("07:00")) == "work", "07:00 reads work");
		ctx.True(day.Where(LF_ResidentTimetable.MinuteOf("17:59")) == "work", "17:59 is still work");

		// Across midnight: the last line holds until the first of the next day.
		ctx.True(day.Where(LF_ResidentTimetable.MinuteOf("00:00")) == "home", "midnight reads home");
		ctx.True(day.Where(LF_ResidentTimetable.MinuteOf("03:00")) == "home", "03:00 reads home (before the first line)");
		ctx.True(day.Where(24 * 60 + 7 * 60 + 30) == "work", "07:30 the next day reads work");
		ctx.True(day.Where(-30) == "home", "23:30 the day before reads home");

		LF_ResidentSlot at = day.At(LF_ResidentTimetable.MinuteOf("08:00"));
		ctx.True(at.m_sKind == "t_work" && at.m_sAnchor == "counter" && at.m_sActivity == "stand", "the slot carries the place kind, anchor and activity");
		ctx.True(LF_ResidentTimetable.FormatMinute(at.m_iStartMinute) == "07:00", "the slot knows its start");

		// Bad times and places are refused, naming the resident.
		ctx.True(LF_ResidentTimetable.MinuteOf("24:00") == -1 && LF_ResidentTimetable.MinuteOf("7:5") == -1 && LF_ResidentTimetable.MinuteOf("noon") == -1, "only a 24 h HH:MM is a time");

		// Empty timetables are refused.
		LF_ResidentDef empty = LF_ResidentDef.Make("t_empty", "t_home", "t_work");
		ctx.True(LF_ResidentTimetable.Build(empty, kinds) == null, "an empty timetable is refused");
		ctx.True(LF_ResidentTimetable.GetLastRefusal().Contains("t_empty"), "the refusal names the resident");
		LF_ResidentDef none = new LF_ResidentDef();
		none.m_sId = "t_none";
		ctx.True(LF_ResidentTimetable.Build(none, kinds) == null, "a resident with no timetable list is refused");

		// Overlapping lines (two on one minute) are refused.
		LF_ResidentDef clash = LF_ResidentDef.Make("t_clash", "t_home", "t_work");
		clash.Add("06:00", "home").Add("07:00", "work").Add("07:00", "home");
		ctx.True(LF_ResidentTimetable.Build(clash, kinds) == null, "two lines on one minute are refused");
		ctx.True(LF_ResidentTimetable.GetLastRefusal().Contains("t_clash") && LF_ResidentTimetable.GetLastRefusal().Contains("overlap"), "the overlap refusal names the resident");

		LF_ResidentDef badTime = LF_ResidentDef.Make("t_late", "t_home", "t_work").Add("25:00", "home");
		ctx.True(LF_ResidentTimetable.Build(badTime, kinds) == null && LF_ResidentTimetable.GetLastRefusal().Contains("t_late"), "an unreadable time is refused");

		LF_ResidentDef badPlace = LF_ResidentDef.Make("t_away", "t_home", "t_work").Add("06:00", "market");
		ctx.True(LF_ResidentTimetable.Build(badPlace, kinds) == null && LF_ResidentTimetable.GetLastRefusal().Contains("t_away"), "a place that is neither home nor work is refused");

		// A place kind LF_Places does not know drops its line; the day goes on without it.
		LF_ResidentDef drifter = LF_ResidentDef.Make("t_drifter", "t_home", "t_gone");
		drifter.Add("06:00", "home").Add("07:00", "work").Add("18:00", "home");
		LF_ResidentTimetable partial = LF_ResidentTimetable.Build(drifter, kinds);
		ctx.True(partial != null && partial.Count() == 2 && LF_ResidentTimetable.GetDroppedCount() == 1, "a line on an unknown place kind is dropped");
		if (partial)
			ctx.True(partial.Where(LF_ResidentTimetable.MinuteOf("07:30")) == "home", "the dropped line no longer moves the resident");

		// Nothing left after the drops: refused. LF_Places itself knows no kind in a logic test.
		LF_ResidentDef lost = LF_ResidentDef.Make("t_lost", "t_nowhere", "");
		lost.Add("06:00", "home").Add("07:00", "work");
		ctx.True(LF_ResidentTimetable.Build(lost) == null && LF_ResidentTimetable.GetLastRefusal().Contains("t_lost"), "the real LF_Places check drops unplaced kinds and refuses an empty day");

		// Load keeps the good residents and leaves the bad ones out.
		LF_ResidentsConfig config = new LF_ResidentsConfig();
		config.m_aResidents = {baker, empty, clash};
		ctx.True(LF_ResidentTimetable.Load(config, kinds) == 1, "Load keeps only the good resident");
		LF_ResidentSlot found = LF_ResidentTimetable.WhereIs("t_baker", LF_ResidentTimetable.MinuteOf("12:00"));
		ctx.True(found != null && found.m_sRole == "work", "a loaded resident is found by id");
		ctx.True(LF_ResidentTimetable.WhereIs("t_clash", 0) == null, "a refused resident has no timetable");

		// The shipped config: its two example residents each get a day once their place kinds exist.
		LF_ResidentsConfig shipped = LF_ResidentsConfig.Get();
		ctx.True(shipped != null && shipped.Count() == 2, "the core config ships two example residents");
		LF_ResidentTimetable.ResetStatics();
		LF_ResidentTimetable.Load(shipped, new LF_Test_ShippedKinds());
		LF_ResidentSlot shopkeeper = LF_ResidentTimetable.WhereIs("example_shopkeeper", LF_ResidentTimetable.MinuteOf("07:30"));
		ctx.True(shopkeeper != null && shopkeeper.m_sRole == "work" && shopkeeper.m_sAnchor == "counter", "the example shopkeeper is at the counter at 07:30");
		LF_ResidentSlot neighbour = LF_ResidentTimetable.WhereIs("example_neighbour", LF_ResidentTimetable.MinuteOf("23:00"));
		ctx.True(neighbour != null && neighbour.m_sRole == "home", "the example neighbour is at home at 23:00");

		LF_ResidentTimetable.ResetStatics();
		ctx.True(LF_ResidentTimetable.GetLastRefusal().IsEmpty(), "the static reset clears the last refusal");
	}
}

//! The place kinds the core example residents name.
class LF_Test_ShippedKinds : LF_ResidentKindCheck
{
	override bool Knows(string kind)
	{
		return kind == "resident_home" || kind == "resident_workplace";
	}
}
