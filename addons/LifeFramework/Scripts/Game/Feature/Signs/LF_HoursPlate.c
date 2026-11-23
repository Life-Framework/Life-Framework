// LF_HoursPlate.c - the opening-hours plate on a shop door, written from the keeper's resident timetable
// (#4236, world W2.5). The plate never holds its own hours: it reads the minutes the timetable says
// `work` (LF_ResidentTimetable), so the sign cannot disagree with the shop. Pure over a built day; the
// row source below finds the shop's keeper by place kind and prints one row on a styled sign.

//------------------------------------------------------------------------------------------------
class LF_HoursPlate
{
	static const string FEATURE = "Signs";
	static const string SOURCE_KIND = "hours_plate";
	static const string ROW_LABEL = "#LF-HoursPlate_Open";
	static const string OPEN_PREFIX = "OPEN ";
	static const string ALL_DAY = "24 HOURS";
	//! A keeper's work place counts as this plate's shop when it stands this near the sign (horizontal metres).
	static const float SHOP_RADIUS_M = 15;	// lf-fixed: a door plate sits within a few metres of its counter's building

	//------------------------------------------------------------------------------------------------
	//! The spans the keeper is at work: start, end pairs (minute of day, end exclusive, may be before
	//! the start across midnight), earliest start first. A keeper at work all day is one span 0, 0.
	//! \return how many spans.
	static int Spans(LF_ResidentTimetable day, notnull array<int> into)
	{
		if (!day)
			return 0;

		array<bool> open = {};
		int openMinutes;
		for (int minute = 0; minute < LF_ResidentTimetable.MINUTES_PER_DAY; minute++)
		{
			bool atWork = day.Where(minute) == LF_ResidentTimetable.ROLE_WORK;
			open.Insert(atWork);
			if (atWork)
				openMinutes++;
		}

		if (openMinutes == 0)
			return 0;

		if (openMinutes == LF_ResidentTimetable.MINUTES_PER_DAY)
		{
			into.Insert(0);
			into.Insert(0);
			return 1;
		}

		int count;
		for (int start = 0; start < LF_ResidentTimetable.MINUTES_PER_DAY; start++)
		{
			int before = LF_ResidentTimetable.WrapMinute(start - 1);
			if (!open[start] || open[before])
				continue;

			int end = start;
			while (open[LF_ResidentTimetable.WrapMinute(end)])
				end++;

			into.Insert(start);
			into.Insert(LF_ResidentTimetable.WrapMinute(end));
			count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! True when `minute` falls inside one of the spans Spans wrote.
	static bool IsInside(notnull array<int> spans, int minute)
	{
		int wrapped = LF_ResidentTimetable.WrapMinute(minute);
		for (int i = 0; i + 1 < spans.Count(); i += 2)
		{
			int start = spans[i];
			int end = spans[i + 1];
			if (start == end)
				return true;

			if (start < end && wrapped >= start && wrapped < end)
				return true;

			if (start > end && (wrapped >= start || wrapped < end))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! "6 AM", "9:30 AM", "12 PM" (noon), "12 AM" (midnight): a minute of the day in US 12 h time.
	static string FormatClock(int minute)
	{
		int wrapped = LF_ResidentTimetable.WrapMinute(minute);
		int hour = wrapped / 60;
		int rest = wrapped % 60;
		string suffix = "AM";
		if (hour >= 12)
			suffix = "PM";

		int clock = hour % 12;
		if (clock == 0)
			clock = 12;

		string text = clock.ToString();
		if (rest > 0)
		{
			string restText = rest.ToString();
			if (rest < 10)
				restText = "0" + restText;

			text = text + ":" + restText;
		}

		return text + " " + suffix;
	}

	//------------------------------------------------------------------------------------------------
	//! "6 AM - 2 PM", "8 AM - 12 PM, 4 PM - 8 PM", "24 HOURS", or empty when the keeper never works.
	static string SpanText(LF_ResidentTimetable day)
	{
		array<int> spans = {};
		if (Spans(day, spans) == 0)
			return string.Empty;

		string text;
		for (int i = 0; i + 1 < spans.Count(); i += 2)
		{
			if (spans[i] == spans[i + 1])
				return ALL_DAY;

			if (i > 0)
				text += ", ";

			text += FormatClock(spans[i]) + " - " + FormatClock(spans[i + 1]);
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! The whole plate: "OPEN 6 AM - 2 PM". Empty when the keeper never works (no plate).
	static string Text(LF_ResidentTimetable day)
	{
		string spans = SpanText(day);
		if (spans.IsEmpty())
			return string.Empty;

		return OPEN_PREFIX + spans;
	}

	//------------------------------------------------------------------------------------------------
	//! The one row a plate prints: a localization-key label and the span text. No row when there are no hours.
	static void BuildRows(LF_ResidentTimetable day, notnull array<ref LF_SignRow> rows)
	{
		string spans = SpanText(day);
		if (spans.IsEmpty())
			return;

		rows.Insert(LF_SignRow.Create(ROW_LABEL, spans));
	}

	//------------------------------------------------------------------------------------------------
	//! The place kind the keeper works at (the first line that puts them at work), or empty.
	static string WorkKind(LF_ResidentTimetable day)
	{
		if (!day)
			return string.Empty;

		for (int minute = 0; minute < LF_ResidentTimetable.MINUTES_PER_DAY; minute++)
		{
			LF_ResidentSlot slot = day.At(minute);
			if (slot.m_sRole == LF_ResidentTimetable.ROLE_WORK)
				return slot.m_sKind;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The resident whose work place of the keeper's kind stands nearest this sign, within SHOP_RADIUS_M, or
	//! empty. Residents are tried by id so a tie is stable.
	static string KeeperNear(notnull IEntity sign)
	{
		array<string> ids = {};
		LF_ResidentTimetable.GetIds(ids);
		ids.Sort();
		vector at = sign.GetOrigin();
		string best;
		float bestSq = SHOP_RADIUS_M * SHOP_RADIUS_M;
		foreach (string id : ids)
		{
			string kind = WorkKind(LF_ResidentTimetable.Find(id));
			if (kind.IsEmpty())
				continue;

			array<IEntity> places = {};
			LF_Places.FindAll(kind, places);
			foreach (IEntity place : places)
			{
				if (!place)
					continue;

				vector there = place.GetOrigin();
				float dx = there[0] - at[0];
				float dz = there[2] - at[2];
				float distSq = dx * dx + dz * dz;
				if (distSq <= bestSq)
				{
					bestSq = distSq;
					best = id;
				}
			}
		}

		return best;
	}
}

//------------------------------------------------------------------------------------------------
//! The local row source: a sign naming `hours_plate` prints its nearest shop keeper's hours. The client
//! already holds the resident config, so it asks no one (the same way the bus timetable does).
class LF_HoursPlateSource : LF_SignRowSource
{
	//------------------------------------------------------------------------------------------------
	override string GetKind()
	{
		return LF_HoursPlate.SOURCE_KIND;
	}

	//------------------------------------------------------------------------------------------------
	override bool IsLocal()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void LocalRows(notnull IEntity sign, notnull array<ref LF_SignRow> rows)
	{
		string keeper = LF_HoursPlate.KeeperNear(sign);
		if (keeper.IsEmpty())
		{
			LF_Debug.Warn(LF_HoursPlate.FEATURE, "hours plate: no shop keeper's work place stands near this sign");
			return;
		}

		LF_HoursPlate.BuildRows(LF_ResidentTimetable.Find(keeper), rows);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_SignRowSourceSet
{
	override void Collect(notnull array<ref LF_SignRowSource> sources)
	{
		super.Collect(sources);
		sources.Insert(new LF_HoursPlateSource());
	}
}
