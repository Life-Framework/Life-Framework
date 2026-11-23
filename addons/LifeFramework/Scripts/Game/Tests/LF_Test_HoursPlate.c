// red-proof: RED-OK signs/hours-plate (2026-11-23, `test --red LF_Test_HoursPlate`: with the hook on the plate is never built, so the five plate-text assertions fail)
//! signs/hours-plate (#4236): the opening-hours plate on a shop door is written from the keeper's resident
//! timetable, so the sign never disagrees with the shop. The plate's open spans are exactly the minutes the
//! timetable says `work`; a changed timetable changes the plate; a keeper who never works has no plate.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HoursPlate : LF_Test
{
	override string GetName()
	{
		return "signs/hours-plate";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ResidentTimetable DayOf(LF_ResidentDef def)
	{
		return LF_ResidentTimetable.Build(def, new LF_Test_AnyKinds());
	}

	//------------------------------------------------------------------------------------------------
	protected string PlateOf(LF_ResidentDef def)
	{
		if (RedProof())
			return string.Empty;

		return LF_HoursPlate.Text(DayOf(def));
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ResidentTimetable.ResetStatics();

		// A baker: home until 06:00, behind the counter 06:00 to 14:00.
		LF_ResidentDef baker = LF_ResidentDef.Make("t_baker", "t_home", "t_shop");
		baker.Add("00:00", "home", "bench").Add("06:00", "work", "counter", "serve").Add("14:00", "home", "bench");
		ctx.EqualStr("OPEN 6 AM - 2 PM", PlateOf(baker), "the plate reads the keeper's shift");

		// The same shop with a later timetable: the plate follows, nothing else was edited.
		LF_ResidentDef late = LF_ResidentDef.Make("t_late", "t_home", "t_shop");
		late.Add("00:00", "home", "bench").Add("09:30", "work", "counter", "serve").Add("17:00", "home", "bench");
		ctx.EqualStr("OPEN 9:30 AM - 5 PM", PlateOf(late), "a later timetable gives a later plate, half hours kept");

		// A split shift prints both spans, earliest first.
		LF_ResidentDef split = LF_ResidentDef.Make("t_split", "t_home", "t_shop");
		split.Add("00:00", "home", "bench").Add("08:00", "work", "counter", "serve").Add("12:00", "home", "bench").Add("16:00", "work", "counter", "serve").Add("20:00", "home", "bench");
		ctx.EqualStr("OPEN 8 AM - 12 PM, 4 PM - 8 PM", PlateOf(split), "a split shift prints both spans");

		// A night shift across midnight is one span.
		LF_ResidentDef night = LF_ResidentDef.Make("t_night", "t_home", "t_shop");
		night.Add("06:00", "home", "bench").Add("22:00", "work", "counter", "serve");
		ctx.EqualStr("OPEN 10 PM - 6 AM", PlateOf(night), "a shift across midnight reads as one span");

		// Always at work: open all day. Never at work: no plate.
		LF_ResidentDef always = LF_ResidentDef.Make("t_always", "t_home", "t_shop");
		always.Add("00:00", "work", "counter", "serve");
		ctx.EqualStr("OPEN 24 HOURS", PlateOf(always), "a keeper always at the counter is open all day");

		LF_ResidentDef never = LF_ResidentDef.Make("t_never", "t_home", "t_shop");
		never.Add("00:00", "home", "bench");
		ctx.EqualStr("", PlateOf(never), "a keeper who never works has no plate");
		ctx.True(LF_HoursPlate.Text(null).IsEmpty(), "no timetable, no plate");

		if (RedProof())
			return;

		// The plate and the timetable agree minute by minute: a minute is inside a plate span exactly when
		// the timetable says the keeper is at work.
		array<ref LF_ResidentDef> all = {baker, late, split, night, always, never};
		foreach (LF_ResidentDef def : all)
		{
			LF_ResidentTimetable day = DayOf(def);
			array<int> spans = {};
			LF_HoursPlate.Spans(day, spans);
			int disagree;
			for (int minute = 0; minute < LF_ResidentTimetable.MINUTES_PER_DAY; minute++)
			{
				bool open = LF_HoursPlate.IsInside(spans, minute);
				bool atWork = day.Where(minute) == LF_ResidentTimetable.ROLE_WORK;
				if (open != atWork)
					disagree++;
			}

			ctx.True(disagree == 0, string.Format("%1: plate and timetable agree on every minute (%2 differ)", def.m_sId, disagree));
		}

		// The sign row: one labelled row, the spans as the value; none for a shop with no hours.
		array<ref LF_SignRow> rows = {};
		LF_HoursPlate.BuildRows(DayOf(baker), rows);
		ctx.True(rows.Count() == 1, "one hours row");
		if (rows.Count() == 1)
		{
			ctx.EqualStr("#LF-HoursPlate_Open", rows[0].m_sLabel, "the row label is a localization key");
			ctx.EqualStr("6 AM - 2 PM", rows[0].m_sValue, "the row value is the span text");
		}

		rows.Clear();
		LF_HoursPlate.BuildRows(DayOf(never), rows);
		ctx.True(rows.IsEmpty(), "a shop with no hours prints no row");

		// The work place kind comes from the timetable, so the plate finds its shop by place, not by name.
		ctx.EqualStr("t_shop", LF_HoursPlate.WorkKind(DayOf(baker)), "the keeper's work kind is read from the day");
		ctx.EqualStr("", LF_HoursPlate.WorkKind(DayOf(never)), "a keeper who never works has no work kind");

		LF_ResidentTimetable.ResetStatics();
	}
}
