//------------------------------------------------------------------------------------------------
//! What a robbable place's alarm shows on the map: nothing, a robbery going on, or robbed a while ago.
enum LF_EMapAlarmState
{
	NONE,
	IN_PROGRESS,
	RECENT
}

//------------------------------------------------------------------------------------------------
//! One place's alarm as the server keeps it and every client reads it: the place key ("<kind>:<id>",
//! the same pair the map draws the badge from), the state and when the alarm went off (unix seconds).
class LF_MapAlarmRow : Managed
{
	string m_sPlace;
	LF_EMapAlarmState m_eState;
	int m_iRaised;

	//------------------------------------------------------------------------------------------------
	static LF_MapAlarmRow Make(string place, LF_EMapAlarmState state, int raised)
	{
		LF_MapAlarmRow row = new LF_MapAlarmRow();
		row.m_sPlace = place;
		row.m_eState = state;
		row.m_iRaised = raised;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The pure rules of the map alarm: which reports ring a place, how the state moves with time, the
//! wire text, the hover line. No world, no statics: the LOGIC tests call them directly.
class LF_MapAlarmRules
{
	//! A robbery shows as in progress this long after the last alarm at the place (a held till runs
	//! about a minute, a vault drill longer; a new report at the place starts it again).
	static const int LIVE_SECONDS = 180; // lf-fixed: map display window, not a balance knob
	//! Then "robbed recently" for this long: the shop till's own cooldown (900 s, #2418).
	static const int RECENT_SECONDS = 900; // lf-fixed: map display window, matches the till cooldown
	//! A report rings the nearest map place within this many metres of its site.
	static const float PLACE_RADIUS = 30; // lf-fixed: till-to-badge reach, not a balance knob
	//! At most this many places ring at once; the oldest drops first (keeps the replicated text short).
	static const int MAX_ROWS = 12; // lf-fixed: wire size cap
	static const string ROW_SEP = ";";
	static const string FIELD_SEP = "|";
	static const string KEY_SEP = ":";

	//------------------------------------------------------------------------------------------------
	//! A crime report that is a robbery of a place (a till, a vault, the hospital cabinet, the armoury).
	//! Carjacking, weapon theft, shoplifting, murder and jail breaks ring no shop.
	static bool IsRobbery(string offence)
	{
		return offence == LF_WantedReports.OFFENCE_ROBBERY
			|| offence == LF_WantedReports.OFFENCE_BREAK_IN
			|| offence == LF_WantedReports.OFFENCE_HOSPITAL
			|| offence == LF_WantedReports.OFFENCE_ARMORY;
	}

	//------------------------------------------------------------------------------------------------
	//! Only places everyone may see ring on everyone's map: a hidden or role-only place never goes on
	//! the shared wire.
	static bool MayRing(LF_EMapPoiVisibility visibility)
	{
		return visibility == LF_EMapPoiVisibility.PUBLIC || visibility == LF_EMapPoiVisibility.DISCOVER;
	}

	//------------------------------------------------------------------------------------------------
	static string Key(string kind, string id)
	{
		return kind + KEY_SEP + id;
	}

	//------------------------------------------------------------------------------------------------
	//! The state an alarm raised at `raised` has at `now`.
	static LF_EMapAlarmState StateAt(int raised, int now)
	{
		int age = now - raised;
		if (age < 0)
			age = 0;

		if (age < LIVE_SECONDS)
			return LF_EMapAlarmState.IN_PROGRESS;

		if (age < LIVE_SECONDS + RECENT_SECONDS)
			return LF_EMapAlarmState.RECENT;

		return LF_EMapAlarmState.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Whole minutes since the alarm, never negative (a client clock a little behind the server's).
	static int MinutesAgo(int raised, int now)
	{
		int age = now - raised;
		if (age < 0)
			return 0;

		return age / 60;
	}

	//------------------------------------------------------------------------------------------------
	//! The hover card line: the localization key and its one argument (minutes, empty when unused).
	static string CardKey(LF_EMapAlarmState state, int minutes, out string arg)
	{
		arg = "";
		if (state == LF_EMapAlarmState.RECENT)
			return "#LF-MapAlarm_Recent";

		if (state != LF_EMapAlarmState.IN_PROGRESS)
			return "";

		if (minutes < 1)
			return "#LF-MapAlarm_InProgressNow";

		arg = minutes.ToString();
		return "#LF-MapAlarm_InProgress";
	}

	//------------------------------------------------------------------------------------------------
	//! The replicated text: "<kind>:<id>|<state>|<raised>;..." Rows with no state are left out.
	static string Encode(notnull array<ref LF_MapAlarmRow> rows)
	{
		string text;
		foreach (LF_MapAlarmRow row : rows)
		{
			if (!row || row.m_eState == LF_EMapAlarmState.NONE || row.m_sPlace.IsEmpty())
				continue;

			if (!text.IsEmpty())
				text += ROW_SEP;

			text += row.m_sPlace + FIELD_SEP + ((int)row.m_eState).ToString() + FIELD_SEP + row.m_iRaised.ToString();
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads Encode's text; a malformed row is skipped. \return How many rows were added.
	static int Decode(string text, notnull array<ref LF_MapAlarmRow> into)
	{
		if (text.IsEmpty())
			return 0;

		int added = 0;
		array<string> rows = {};
		text.Split(ROW_SEP, rows, true);
		foreach (string rowText : rows)
		{
			array<string> fields = {};
			rowText.Split(FIELD_SEP, fields, false);
			if (fields.Count() != 3 || fields[0].IsEmpty())
				continue;

			int state = fields[1].ToInt();
			if (state != LF_EMapAlarmState.IN_PROGRESS && state != LF_EMapAlarmState.RECENT)
				continue;

			into.Insert(LF_MapAlarmRow.Make(fields[0], state, fields[2].ToInt()));
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest place within PLACE_RADIUS of site, or null.
	static LF_MapPoiPlace Nearest(notnull array<ref LF_MapPoiPlace> places, vector site)
	{
		LF_MapPoiPlace best;
		float bestSq = PLACE_RADIUS * PLACE_RADIUS;
		foreach (LF_MapPoiPlace place : places)
		{
			if (!place)
				continue;

			float d = vector.DistanceSqXZ(place.m_vPos, site);
			if (d <= bestSq)
			{
				bestSq = d;
				best = place;
			}
		}

		return best;
	}
}

//------------------------------------------------------------------------------------------------
//! The robbery alarm on the map. The server turns each robbery report (LF_Dispatch.Report publishes it
//! as a police LF_OnServiceCall with the site) into a row for the nearest robbable map place, moves the
//! rows on with time (in progress, then robbed recently, then gone) and writes them as one short text
//! to the game mode's RplProp: one carrier, bumped only when a row appears, changes state or leaves,
//! and a late joiner gets the text with the game mode. Every client reads the same rows (it is the
//! shop's alarm, not a police secret); police keep their own dispatch. Nothing is saved.
class LF_MapAlarms
{
	static const string FEATURE = "MapAlarm";
	static const int TICK_MS = 5000; // lf-fixed: state sweep cadence

	protected static ref array<ref LF_MapAlarmRow> s_aRows;
	protected static bool s_bTicking;
	//! Client: the rows decoded from the last replicated text, and who to tell when it changes.
	protected static ref array<ref LF_MapAlarmRow> s_aClient;
	protected static ref ScriptInvoker s_OnChanged;
	//! Test seam: when set, the clock the server rules read (unix seconds).
	protected static int s_iNowForTest;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		LF_MapAlarmSubscriber.ResetStatics();
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_aRows = null;
		s_bTicking = false;
		s_aClient = null;
		s_OnChanged = null;
		s_iNowForTest = 0;
	}

	//------------------------------------------------------------------------------------------------
	static void Install()
	{
		if (Replication.IsServer())
			LF_MapAlarmSubscriber.Install();
	}

	//------------------------------------------------------------------------------------------------
	static int Now()
	{
		if (s_iNowForTest > 0)
			return s_iNowForTest;

		return System.GetUnixTime();
	}

	//------------------------------------------------------------------------------------------------
	static void SetNowForTest(int now)
	{
		s_iNowForTest = now;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_MapAlarmRow> Rows()
	{
		if (!s_aRows)
			s_aRows = {};

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a police call arrived. A robbery with a site rings the nearest robbable map place.
	//! \return The place key it rang, or empty.
	static string OnCall(notnull LF_OnServiceCall call)
	{
		if (call.m_sService != LF_MapShares.ROLE_POLICE || !LF_MapAlarmRules.IsRobbery(call.m_sSummary))
			return "";

		if (call.m_vPosition == vector.Zero)
			return "";

		array<ref LF_MapPoiPlace> places = {};
		GatherRingable(places);
		LF_MapPoiPlace place = LF_MapAlarmRules.Nearest(places, call.m_vPosition);
		if (!place)
		{
			LF_Debug.Info(FEATURE, string.Format("robbery at %1 has no map place within %2 m, no map alarm", call.m_vPosition, LF_MapAlarmRules.PLACE_RADIUS));
			return "";
		}

		string key = LF_MapAlarmRules.Key(place.m_sKind, place.m_sId);
		Raise(key, Now());
		return key;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the live places a robbery may ring (kinds with a PUBLIC or DISCOVER map row).
	static void GatherRingable(notnull array<ref LF_MapPoiPlace> into)
	{
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		if (!config || !config.m_aKinds)
			return;

		foreach (LF_MapPoiKind row : config.m_aKinds)
		{
			if (row && !LF_MapPoiConfig.IsSlotRow(row) && LF_MapAlarmRules.MayRing(row.m_eVisibility))
				LF_MapPois.GatherKind(row.m_sKind, into);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the alarm at a place went off now (a new report restarts an old row).
	static void Raise(string key, int now)
	{
		array<ref LF_MapAlarmRow> rows = Rows();
		for (int i = rows.Count() - 1; i >= 0; i--)
		{
			if (rows[i].m_sPlace == key)
				rows.RemoveOrdered(i);
		}

		rows.Insert(LF_MapAlarmRow.Make(key, LF_EMapAlarmState.IN_PROGRESS, now));
		while (rows.Count() > LF_MapAlarmRules.MAX_ROWS)
			rows.RemoveOrdered(0);

		LF_Debug.Info(FEATURE, string.Format("robbery in progress at %1", key));
		Publish();
		StartTicking();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: move every row on to its state at now. \return True when any row changed or left.
	static bool Advance(int now)
	{
		bool changed;
		array<ref LF_MapAlarmRow> rows = Rows();
		for (int i = rows.Count() - 1; i >= 0; i--)
		{
			LF_MapAlarmRow row = rows[i];
			LF_EMapAlarmState next = LF_MapAlarmRules.StateAt(row.m_iRaised, now);
			if (next == row.m_eState)
				continue;

			changed = true;
			if (next == LF_EMapAlarmState.NONE)
			{
				LF_Debug.Info(FEATURE, string.Format("alarm cleared at %1", row.m_sPlace));
				rows.RemoveOrdered(i);
				continue;
			}

			LF_Debug.Info(FEATURE, string.Format("robbed recently at %1", row.m_sPlace));
			row.m_eState = next;
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	protected static void StartTicking()
	{
		if (s_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (Advance(Now()))
			Publish();

		if (Rows().IsEmpty() && s_bTicking)
		{
			GetGame().GetCallqueue().Remove(Tick);
			s_bTicking = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the text the carrier holds now.
	static string Snapshot()
	{
		return LF_MapAlarmRules.Encode(Rows());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: write the rows to the game mode's replicated text (it bumps only on a different text).
	protected static void Publish()
	{
		LF_GameModeRoleplay gameMode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (!gameMode)
		{
			LF_Debug.Warn(FEATURE, "no roleplay game mode, map alarms not replicated");
			return;
		}

		gameMode.LF_SetMapAlarms(Snapshot());
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: the carrier's text arrived or changed (the listen host calls it too).
	static void OnReplicated(string text)
	{
		s_aClient = {};
		LF_MapAlarmRules.Decode(text, s_aClient);
		if (s_OnChanged)
			s_OnChanged.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: the alarm state of a map place and when it went off.
	static LF_EMapAlarmState StateOf(string kind, string id, out int raised)
	{
		raised = 0;
		if (!s_aClient)
			return LF_EMapAlarmState.NONE;

		string key = LF_MapAlarmRules.Key(kind, id);
		foreach (LF_MapAlarmRow row : s_aClient)
		{
			if (row.m_sPlace == key)
			{
				raised = row.m_iRaised;
				return row.m_eState;
			}
		}

		return LF_EMapAlarmState.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Any machine: how many places ring now (harness and tests).
	static int ClientCount()
	{
		if (!s_aClient)
			return 0;

		return s_aClient.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Called with no arguments whenever the replicated rows change (the open map redraws).
	static ScriptInvoker OnChanged()
	{
		if (!s_OnChanged)
			s_OnChanged = new ScriptInvoker();

		return s_OnChanged;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: forget every alarm (world reset).
	static int ClearAll()
	{
		int count = Rows().Count();
		Rows().Clear();
		Publish();
		return count;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: police calls ring the map.
class LF_MapAlarmSubscriber : LF_EventSubscriber
{
	protected static ref LF_MapAlarmSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnServiceCall call = LF_OnServiceCall.Cast(busEvent);
		if (!call)
			return false;

		LF_MapAlarms.OnCall(call);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void Install()
	{
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnServiceCall, s_Subscriber);

		s_Subscriber = new LF_MapAlarmSubscriber();
		LF_EventBus.Subscribe(LF_OnServiceCall, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnServiceCall, s_Subscriber);

		s_Subscriber = null;
	}
}

//------------------------------------------------------------------------------------------------
//! The one carrier: a short text on the always-replicated game mode. JIP gets it in the snapshot.
modded class LF_GameModeRoleplay
{
	[RplProp(onRplName: "LF_OnMapAlarmsRpl")]
	protected string m_sLF_MapAlarms;

	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_MapAlarms.Install();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: set the alarm text; bumps only when it differs.
	void LF_SetMapAlarms(string text)
	{
		if (text == m_sLF_MapAlarms)
			return;

		m_sLF_MapAlarms = text;
		Replication.BumpMe();
		LF_OnMapAlarmsRpl();
	}

	//------------------------------------------------------------------------------------------------
	string LF_GetMapAlarms()
	{
		return m_sLF_MapAlarms;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnMapAlarmsRpl()
	{
		LF_MapAlarms.OnReplicated(m_sLF_MapAlarms);
	}
}

//------------------------------------------------------------------------------------------------
//! The world reset clears the alarms with the other crime state.
modded class LF_WorldResetRegistry
{
	override void Collect(notnull array<ref LF_WorldResetStep> steps)
	{
		super.Collect(steps);
		steps.Insert(new LF_WorldReset_MapAlarms());
	}
}

//------------------------------------------------------------------------------------------------
class LF_WorldReset_MapAlarms : LF_WorldResetStep
{
	override string GetName()
	{
		return "mapalarms";
	}

	override bool IsCrimeState()
	{
		return true;
	}

	override int Run()
	{
		return LF_MapAlarms.ClearAll();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MapAlarms' statics (LF_StaticReset finds it by reflection).
class LF_MapAlarms_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MapAlarms";
	}

	override void Apply()
	{
		LF_MapAlarms.ResetStatics();
	}
}
