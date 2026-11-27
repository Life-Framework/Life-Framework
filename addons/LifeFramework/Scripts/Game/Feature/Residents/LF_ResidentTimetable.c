// LF_ResidentTimetable.c - where each resident should be at a game minute (#2665, epic #991).
// Server side, data only: nobody walks. A resident's day is a list of lines by start time; the line
// whose start is the latest at or before the minute is where they are, and the last line of the
// day holds across midnight. The place of a line is a kind found through LF_Places, never a name
// or a coordinate (rule 9). Materialising residents near players is wave 2.

//------------------------------------------------------------------------------------------------
//! One line of a built day: where a resident is from `m_iStartMinute` on.
class LF_ResidentSlot
{
	int m_iStartMinute;
	string m_sRole;
	string m_sKind;
	//! The one place of the kind (LF_Places id), or empty for the nearest (#3231 home, #4560 work).
	string m_sPlaceId;
	string m_sAnchor;
	string m_sActivity;
}

//------------------------------------------------------------------------------------------------
//! Whether LF_Places knows a place kind: at least one place of it is placed on this map. A test
//! overrides Knows to name the kinds a world would have.
class LF_ResidentKindCheck
{
	bool Knows(string kind)
	{
		if (kind.IsEmpty())
			return false;

		array<IEntity> places = {};
		return LF_Places.FindAll(kind, places) > 0;
	}
}

//------------------------------------------------------------------------------------------------
//! One resident's day, validated and sorted by start time.
class LF_ResidentTimetable
{
	static const string FEATURE = "Residents";
	static const string ROLE_HOME = "home";
	static const string ROLE_WORK = "work";
	static const int MINUTES_PER_DAY = 1440;	// lf-fixed: a game day is 24 h by definition

	protected static ref map<string, ref LF_ResidentTimetable> s_mDays;
	protected static string s_sLastRefusal;
	protected static ref array<string> s_aDropped;

	protected string m_sResident;
	protected ref array<ref LF_ResidentSlot> m_aSlots = {};

	//------------------------------------------------------------------------------------------------
	//! Minute of the day for 'HH:MM', or -1 when it is not a valid 24 h time.
	static int MinuteOf(string time)
	{
		time.TrimInPlace();
		int colon = time.IndexOf(":");
		if (colon < 1 || colon != time.Length() - 3)
			return -1;

		string hourText = time.Substring(0, colon);
		string minuteText = time.Substring(colon + 1, 2);
		if (!IsDigits(hourText) || !IsDigits(minuteText))
			return -1;

		int hour = hourText.ToInt();
		int minute = minuteText.ToInt();
		if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
			return -1;

		return hour * 60 + minute;
	}

	//------------------------------------------------------------------------------------------------
	//! The game minute of the day now (0 to 1439), from the world clock (LF_Atmosphere.HourNow).
	static int NowMinute()
	{
		return MinuteOfHour(LF_Atmosphere.HourNow());
	}

	//------------------------------------------------------------------------------------------------
	//! The minute of the day for a clock hour as a float (7.5 is 07:30), wrapped into one day.
	static int MinuteOfHour(float hour)
	{
		int minute = Math.Floor(hour * 60);
		return WrapMinute(minute);
	}

	//------------------------------------------------------------------------------------------------
	//! Any minute count wrapped into 0 to 1439: 1500 is 01:00 the next day, -60 is 23:00 the day before.
	static int WrapMinute(int minute)
	{
		int wrapped = minute % MINUTES_PER_DAY;
		if (wrapped < 0)
			wrapped += MINUTES_PER_DAY;

		return wrapped;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds one resident's day. Refused (null, logged as an LF_Debug.Error naming the resident) for
	//! no id, an empty timetable, an unreadable time or place, or two lines sharing a start. A line
	//! whose place kind LF_Places does not know is logged and dropped; if that leaves nothing the day
	//! is refused. The reason is in GetLastRefusal.
	//! \param[in] check Which kinds exist; null asks LF_Places.
	static LF_ResidentTimetable Build(LF_ResidentDef def, LF_ResidentKindCheck check = null)
	{
		s_sLastRefusal = string.Empty;
		s_aDropped = null;
		if (!def)
			return Refuse("?", "no definition");

		string id = def.m_sId;
		if (id.IsEmpty())
			return Refuse("?", "a resident has no id");

		if (!def.m_aTimetable || def.m_aTimetable.IsEmpty())
			return Refuse(id, "the timetable is empty");

		ref LF_ResidentKindCheck active = check;
		if (!active)
			active = new LF_ResidentKindCheck();

		array<ref LF_ResidentSlot> parsed = {};
		set<int> starts = new set<int>();
		foreach (LF_ResidentEntryDef entry : def.m_aTimetable)
		{
			if (!entry)
				continue;

			int start = MinuteOf(entry.m_sTime);
			if (start < 0)
				return Refuse(id, string.Format("'%1' is not a 24 h HH:MM time", entry.m_sTime));

			string role = entry.m_sPlace;
			role.ToLower();
			string kind;
			string placeId;
			if (role == ROLE_HOME)
			{
				kind = def.m_sHomeKind;
				placeId = def.m_sHomePlaceId;
			}
			else if (role == ROLE_WORK)
			{
				kind = def.m_sWorkKind;
				placeId = def.m_sWorkPlaceId;
			}
			else
				return Refuse(id, string.Format("line %1 says place '%2', not home or work", entry.m_sTime, entry.m_sPlace));

			if (starts.Contains(start))
				return Refuse(id, string.Format("two lines start at %1 and overlap", entry.m_sTime));

			starts.Insert(start);
			LF_ResidentSlot slot = new LF_ResidentSlot();
			slot.m_iStartMinute = start;
			slot.m_sRole = role;
			slot.m_sKind = kind;
			slot.m_sPlaceId = placeId;
			slot.m_sAnchor = entry.m_sAnchor;
			slot.m_sActivity = entry.m_sActivity;
			parsed.Insert(slot);
		}

		LF_ResidentTimetable day = new LF_ResidentTimetable();
		day.m_sResident = id;
		foreach (LF_ResidentSlot slot : parsed)
		{
			if (!active.Knows(slot.m_sKind))
			{
				LF_Debug.Warn(FEATURE, string.Format("resident '%1': line %2 dropped, LF_Places does not know the %3 place kind '%4'", id, FormatMinute(slot.m_iStartMinute), slot.m_sRole, slot.m_sKind));
				if (!s_aDropped)
					s_aDropped = {};

				s_aDropped.Insert(FormatMinute(slot.m_iStartMinute));
				continue;
			}

			day.Insert(slot);
		}

		if (day.m_aSlots.IsEmpty())
			return Refuse(id, "no line has a place kind LF_Places knows");

		return day;
	}

	//------------------------------------------------------------------------------------------------
	//! Where the resident is at `minute` (any minute count; it wraps). Never null for a built day.
	LF_ResidentSlot At(int minute)
	{
		int wrapped = WrapMinute(minute);
		LF_ResidentSlot current = m_aSlots[m_aSlots.Count() - 1];	// before the first line: the last of the day before
		foreach (LF_ResidentSlot slot : m_aSlots)
		{
			if (slot.m_iStartMinute > wrapped)
				break;

			current = slot;
		}

		return current;
	}

	//------------------------------------------------------------------------------------------------
	//! 'home' or 'work' at `minute`.
	string Where(int minute)
	{
		return At(minute).m_sRole;
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_aSlots.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! 'HH:MM' for a minute of the day.
	static string FormatMinute(int minute)
	{
		int wrapped = WrapMinute(minute);
		int hour = wrapped / 60;
		int rest = wrapped % 60;
		string hourText = hour.ToString();
		if (hour < 10)
			hourText = "0" + hourText;

		string minuteText = rest.ToString();
		if (rest < 10)
			minuteText = "0" + minuteText;

		return hourText + ":" + minuteText;
	}

	//------------------------------------------------------------------------------------------------
	//! Builds every resident of `config` and keeps the good days. A refused resident is logged and
	//! left out; a second resident under an id is refused. \return How many residents were kept.
	static int Load(LF_ResidentsConfig config, LF_ResidentKindCheck check = null)
	{
		s_mDays = new map<string, ref LF_ResidentTimetable>();
		if (!config || !config.m_aResidents)
		{
			LF_Debug.Error(FEATURE, "no residents config to load");
			return 0;
		}

		foreach (LF_ResidentDef def : config.m_aResidents)
		{
			LF_ResidentTimetable day = Build(def, check);
			if (!day)
				continue;

			if (s_mDays.Contains(day.m_sResident))
			{
				Refuse(day.m_sResident, "a second resident has this id");
				continue;
			}

			s_mDays.Set(day.m_sResident, day);
		}

		LF_Debug.Info(FEATURE, string.Format("timetables loaded: %1 of %2 residents", s_mDays.Count(), config.m_aResidents.Count()));
		return s_mDays.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server. The loaded day of `id` (loading the config on first use), or null and a log line.
	static LF_ResidentTimetable Find(string id)
	{
		if (!s_mDays)
			Load(LF_ResidentsConfig.Get());

		LF_ResidentTimetable day;
		if (!s_mDays || !s_mDays.Find(id, day))
		{
			LF_Debug.Warn(FEATURE, string.Format("no timetable for resident '%1'", id));
			return null;
		}

		return day;
	}

	//------------------------------------------------------------------------------------------------
	//! Server. Where resident `id` should be at `minute`; null when the resident has no day.
	static LF_ResidentSlot WhereIs(string id, int minute)
	{
		LF_ResidentTimetable day = Find(id);
		if (!day)
			return null;

		return day.At(minute);
	}

	//------------------------------------------------------------------------------------------------
	//! Ids of every loaded resident, appended to into. \return how many.
	static int GetIds(notnull array<string> into)
	{
		if (!s_mDays)
			Load(LF_ResidentsConfig.Get());

		if (!s_mDays)
			return 0;

		foreach (string id, LF_ResidentTimetable day : s_mDays)
			into.Insert(id);

		return s_mDays.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Why the last Build or Load refused a resident (empty when it did not).
	static string GetLastRefusal()
	{
		return s_sLastRefusal;
	}

	//------------------------------------------------------------------------------------------------
	//! Start times of the lines the last Build dropped for an unknown place kind.
	static int GetDroppedCount()
	{
		if (!s_aDropped)
			return 0;

		return s_aDropped.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends; the config is read again next world.
	static void ResetStatics()
	{
		s_mDays = null;
		s_sLastRefusal = string.Empty;
		s_aDropped = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void Insert(LF_ResidentSlot slot)
	{
		int at = m_aSlots.Count();
		for (int i = 0; i < m_aSlots.Count(); i++)
		{
			if (m_aSlots[i].m_iStartMinute > slot.m_iStartMinute)
			{
				at = i;
				break;
			}
		}

		m_aSlots.InsertAt(slot, at);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ResidentTimetable Refuse(string id, string why)
	{
		s_sLastRefusal = string.Format("resident '%1': %2", id, why);
		LF_Debug.Error(FEATURE, "timetable refused, " + s_sLastRefusal);
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsDigits(string text)
	{
		if (text.IsEmpty())
			return false;

		for (int i = 0; i < text.Length(); i++)
		{
			if ("0123456789".IndexOf(text.Substring(i, 1)) < 0)
				return false;
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ResidentTimetable's statics (LF_StaticReset finds it by reflection).
class LF_ResidentTimetable_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ResidentTimetable";
	}

	override void Apply()
	{
		LF_ResidentTimetable.ResetStatics();
	}
}
