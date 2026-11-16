// red-proof: `test --no-build --red LF_Test_EventPlayersNearby` -> RED PROOF OK (2026-11-17): with the hook on the place is never bound, so `a player inside keeps the NEARBY_BELOW abort quiet (expected 'hot', got '')` and `one LF_OnEventPhase per change (expected 3, got 0)` fail; without it `passed=2 failed=0`.
// tier: WORLD
//! Events count the players near their place (#2105): the test event is bound to the target of the
//! scene in grid cell `event-scene` (scene `grid_event_scene`, found through LF_Places by component)
//! with a 10 m radius, and a stubbed player position stands inside, then outside. The shell measures
//! 1 and then 0; a PLAYERS_NEARBY start condition lets the event start only with a player inside and
//! a NEARBY_BELOW abort ends it only once the place is empty. An event with no place counts 0 and
//! warns once.
class LF_Test_EventPlayersNearby_Listener : LF_EventSubscriber
{
	ref array<string> m_aSeen = {};

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnEventPhase e = LF_OnEventPhase.Cast(busEvent);
		if (e && e.m_sEventId == "test_nearby")
			m_aSeen.Insert(string.Format("%1:%2:%3", e.m_sKind, e.m_sToPhase, e.m_sReason));
		return true;
	}
}

class LF_Test_EventPlayersNearby : LF_Test
{
	static const string SCENE = "grid_event_scene";
	static const string EVENT = "test_nearby";
	static const string EVENT_NO_PLACE = "test_nearby_noplace";

	override string GetName()
	{
		return "events/players-nearby";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		vector center;
		ctx.True(LF_Places.FindOrigin(LF_EventWaypointComponent.KIND_TARGET, SCENE, center), "the grid cell's target is placed");

		// Stop the live ticker and swap in the test events; other modules' registrations stay.
		LF_EventShell.Stop();
		LF_EventShell.Unregister("test_nearby_here");
		LF_EventShell.Unregister("test_nearby_never");
		LF_EventShell.Unregister("test_nearby_empty");
		LF_Test_EventPlayersNearby_Listener listener = new LF_Test_EventPlayersNearby_Listener();
		LF_EventBus.Subscribe(LF_OnEventPhase, listener);

		ctx.True(LF_EventShell.RegisterCondition("test_nearby_here", LF_EventPrecondition.MakePlayers(LF_EEventConditionKind.PLAYERS_NEARBY, 1)), "the nearby condition registers");
		ctx.True(LF_EventShell.RegisterCondition("test_nearby_never", LF_EventPrecondition.MakePlayers(LF_EEventConditionKind.PLAYERS_ONLINE, 9999)), "a condition that never holds registers");
		ctx.True(LF_EventShell.RegisterAbort("test_nearby_empty", LF_EventAbort.MakePlayers(LF_EEventAbortKind.NEARBY_BELOW, 1)), "the empty-place abort registers");

		array<ref LF_EventPhase> phases = {};
		array<string> prepModules = {"test_nearby_here"};
		array<string> hotModules = {"test_nearby_never", "test_nearby_empty"};
		phases.Insert(LF_EventPhase.Make("prep", 0, 0, prepModules));
		phases.Insert(LF_EventPhase.Make("hot", 0, 0, hotModules));
		array<ref LF_EventPhase> lonePhases = {};
		lonePhases.Insert(LF_EventPhase.Make("prep", 0, 0, prepModules));

		LF_EventsConfig config = new LF_EventsConfig();
		config.m_aEvents = {};
		config.m_aEvents.Insert(LF_EventDef.Make(EVENT, phases, 300));
		config.m_aEvents.Insert(LF_EventDef.Make(EVENT_NO_PLACE, lonePhases, 300));
		LF_EventShell.Use(config);
		ctx.Equal(2, LF_EventShell.RunCount(), "both test events run");

		LF_EventShell.ForceHour(12);
		LF_EventPlace.Unbind(EVENT);
		if (!RedProof())
			ctx.True(LF_EventPlace.Bind(EVENT, SCENE, 10), "the event binds to its scene with a 10 m radius");
		ctx.False(LF_EventPlace.Bind("", SCENE, 10), "an empty event id is refused");
		ctx.False(LF_EventPlace.Bind("x", SCENE, 0), "a radius of 0 is refused");

		vector inside = center + Vector(3, 0, 0);
		vector outside = center + Vector(30, 0, 0);
		array<vector> positions = {};

		// Nobody there: nearby 0, the nearby condition fails, the event stays idle.
		LF_EventPlace.UseTestPositions(positions);
		LF_EventShell.Tick(0);
		ctx.Equal(0, LF_EventShell.NearbyOf(EVENT), "an empty place counts 0");
		ctx.EqualStr("", LF_EventShell.PhaseOf(EVENT), "with nobody near the event does not start");

		// One player outside the radius: still 0.
		positions.Insert(outside);
		LF_EventPlace.UseTestPositions(positions);
		LF_EventShell.Tick(1);
		ctx.Equal(0, LF_EventShell.NearbyOf(EVENT), "a player outside the radius counts 0");
		ctx.EqualStr("", LF_EventShell.PhaseOf(EVENT), "the event stays idle");

		// One inside, one outside: 1, and the event starts.
		positions.Insert(inside);
		LF_EventPlace.UseTestPositions(positions);
		LF_EventShell.Tick(10);
		ctx.Equal(1, LF_EventShell.NearbyOf(EVENT), "a player inside the radius counts 1, the one outside does not");
		ctx.EqualStr("prep", LF_EventShell.PhaseOf(EVENT), "with a player near the event starts");

		// The condition held: prep advances to hot. A hot phase whose own condition never holds
		// stays while the place has a player in it.
		LF_EventShell.Tick(11);
		ctx.EqualStr("hot", LF_EventShell.PhaseOf(EVENT), "prep advances to hot");
		LF_EventShell.Tick(12);
		ctx.EqualStr("hot", LF_EventShell.PhaseOf(EVENT), "a player inside keeps the NEARBY_BELOW abort quiet");

		// The player leaves: nearby 0 and the abort fires.
		positions.Clear();
		positions.Insert(outside);
		LF_EventPlace.UseTestPositions(positions);
		LF_EventShell.Tick(13);
		ctx.Equal(0, LF_EventShell.NearbyOf(EVENT), "the place is empty again");
		ctx.EqualStr("", LF_EventShell.PhaseOf(EVENT), "an empty place aborts the event");
		ctx.Equal(3, listener.m_aSeen.Count(), "one LF_OnEventPhase per change");
		if (listener.m_aSeen.Count() == 3)
			ctx.EqualStr("end::abort", listener.m_aSeen[2], "the end is an abort");

		// A phase radius wins over the event's for that phase only.
		LF_EventPlace.BindPhaseRadius(EVENT, "prep", 2);
		ctx.True(LF_EventPlace.RadiusOf(EVENT, "prep") == 2, "a phase radius applies to its phase");
		ctx.True(LF_EventPlace.RadiusOf(EVENT, "hot") == 10, "another phase keeps the event's radius");
		positions.Clear();
		positions.Insert(inside);
		LF_EventPlace.UseTestPositions(positions);
		LF_EventShell.Tick(400);
		ctx.Equal(0, LF_EventShell.NearbyOf(EVENT), "the player 3 m away is outside the 2 m prep radius");
		ctx.EqualStr("", LF_EventShell.PhaseOf(EVENT), "so the event does not start");

		// No place: the event has measured 0 since the first tick and warned, once.
		ctx.Equal(0, LF_EventShell.NearbyOf(EVENT_NO_PLACE), "an event with no place counts 0 though a player stands in the cell");
		ctx.True(LF_EventPlace.HasWarned(EVENT_NO_PLACE), "and warns");
		ctx.EqualStr("", LF_EventShell.PhaseOf(EVENT_NO_PLACE), "a nearby condition never holds without a place");

		// A bound scene with no marker placed counts 0 too.
		LF_EventPlace.Bind(EVENT_NO_PLACE, "no_such_scene", 10);
		LF_EventShell.Tick(401);
		ctx.Equal(0, LF_EventShell.NearbyOf(EVENT_NO_PLACE), "a scene with no placed target counts 0");

		LF_EventPlace.Unbind(EVENT);
		LF_EventPlace.Unbind(EVENT_NO_PLACE);
		LF_EventPlace.ClearTestPositions();
		LF_EventBus.Unsubscribe(LF_OnEventPhase, listener);
		LF_EventShell.Unregister("test_nearby_here");
		LF_EventShell.Unregister("test_nearby_never");
		LF_EventShell.Unregister("test_nearby_empty");
		LF_EventShell.ForceHour(-1);
		LF_EventShell.Use(LF_EventShell.LoadConfig());
		LF_EventShell.StartIfServer();
	}
}
