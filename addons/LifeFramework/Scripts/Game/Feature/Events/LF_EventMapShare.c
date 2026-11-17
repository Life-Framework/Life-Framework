//------------------------------------------------------------------------------------------------
//! Event places on the map (card M15, #2243, everon-map.md sections 2 and 3). While an event runs its
//! place shows to everyone: an event start puts an `EVERYONE` shared mark keyed `event:<eventId>` at
//! the event's place (LF_EventPlace, the scene's `event_target` marker found through LF_Places) in
//! LF_MapShares, carrying the place's radius as an EVENT-style area: the share row has it on the wire, and
//! each client draws it as a local LF_MapAreas circle beside the mark (LF_MapShareAreas). The event's
//! end removes the row and with it the area, so the picture lives exactly as long as the event. Server
//! only; nothing is saved (an event saved mid-phase loads into cooldown, so no run, no mark: events restart).
//!
//! The timer: the row carries a safety expiry (EXPIRY_SECONDS) that every phase change renews, so a
//! mark whose event is lost without an end (a shell stopped, a crash mid-run) still leaves every map.
//! A late joiner or a respawn gets the row from LF_MapShares (card M9), not from here.
//!
//! An event with no place (never bound, or its marker not placed) shows nothing and logs why. One
//! place per event: its row is replaced, not duplicated, when a phase change moves the radius.
class LF_EventMapShare
{
	static const string FEATURE = "Events";
	static const string KEY_PREFIX = "event:";
	static const string TEXT = "#LF-EventMap_Place";
	//! Indexes into the vanilla placed-marker config (Configs/Map/MapMarkerConfig.conf): the first general
	//! icon, and the third colour.
	static const int ICON = 0; // lf-fixed: vanilla config index, not a server tunable
	static const int COLOUR = 3; // lf-fixed: vanilla config index
	//! A mark whose event never ends goes after this long; each phase change renews it.
	static const float EXPIRY_SECONDS = 7200; // lf-fixed: safety net only; the event's end removes the mark

	protected static ref set<string> s_aOpen;
	protected static ref LF_EventMapShareSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	//! The share key of an event (the client names its area after it).
	static string KeyOf(string eventId)
	{
		return KEY_PREFIX + eventId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listen to event phase changes (idempotent). Called at game start.
	static void Install()
	{
		if (!Replication.IsServer())
			return;

		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnEventPhase, s_Subscriber);

		s_Subscriber = new LF_EventMapShareSubscriber();
		LF_EventBus.Subscribe(LF_OnEventPhase, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! One phase change of an event: a start or an advance puts (or renews) its place on the map, an end
	//! takes it off.
	static void OnPhase(notnull LF_OnEventPhase change)
	{
		if (change.m_sKind == "end")
		{
			Close(change.m_sEventId);
			return;
		}

		Open(change.m_sEventId, change.m_sToPhase);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: show the place of a running event to everyone. A second call replaces the first.
	//! \return false when the event has no place (logged) or the share was refused.
	static bool Open(string eventId, string phaseName)
	{
		string sceneId = LF_EventPlace.SceneOf(eventId);
		if (sceneId.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("event %1 has no place: nothing on the map", eventId));
			return false;
		}

		vector centre;
		if (!LF_Places.FindOrigin(LF_EventWaypointComponent.KIND_TARGET, sceneId, centre))
		{
			LF_Debug.Warn(FEATURE, string.Format("event %1 place: scene '%2' has no event_target marker placed: nothing on the map", eventId, sceneId));
			return false;
		}

		int x = Math.Round(centre[0]);
		int z = Math.Round(centre[2]);
		float expires = LF_MapShares.Now() + EXPIRY_SECONDS;
		int radius = Math.Round(LF_EventPlace.RadiusOf(eventId, phaseName));
		if (radius <= 0)
			LF_Debug.Warn(FEATURE, string.Format("event %1 has no radius: its mark shows without an area", eventId));

		LF_MapShareRow row = LF_MapShareRow.Make(KeyOf(eventId), LF_EMapShareAudience.EVERYONE, "", x, z, ICON, COLOUR, TEXT, expires);
		row.WithArea(radius, LF_MapAreaStyle.EVENT);
		if (!LF_MapShares.Put(row))
			return false;

		Opened().Insert(eventId);
		LF_Debug.Info(FEATURE, string.Format("event %1 shown to everyone at %2 %3", eventId, x, z));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take an event's place off every map. \return false when it was not on the map.
	static bool Close(string eventId)
	{
		if (!Opened().Contains(eventId))
			return false;

		Opened().RemoveItem(eventId);
		LF_MapShares.Remove(KeyOf(eventId));
		LF_Debug.Info(FEATURE, string.Format("event %1 off the map", eventId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True while the event's place is on the map.
	static bool IsOpen(string eventId)
	{
		return s_aOpen && s_aOpen.Contains(eventId);
	}

	//------------------------------------------------------------------------------------------------
	protected static set<string> Opened()
	{
		if (!s_aOpen)
			s_aOpen = new set<string>();

		return s_aOpen;
	}

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset) and tests. LF_MapShares clears its own table.
	static void ResetStatics()
	{
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnEventPhase, s_Subscriber);

		s_Subscriber = null;
		s_aOpen = null;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: an event started, advanced or ended.
class LF_EventMapShareSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnEventPhase change = LF_OnEventPhase.Cast(busEvent);
		if (!change)
			return false;

		LF_EventMapShare.OnPhase(change);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Self-start: the server listens from the world's start.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_EventMapShare.Install();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_EventMapShare's statics (registered at script load; Core/LF_StaticReset.c).
class LF_EventMapShare_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_EventMapShare_StaticResetHook());

	override string Owner()
	{
		return "LF_EventMapShare";
	}

	override void Apply()
	{
		LF_EventMapShare.ResetStatics();
	}
}
