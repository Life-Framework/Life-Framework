//------------------------------------------------------------------------------------------------
//! One placed scene marker as the check reads it (#2067): the scene, the role and the order. Plain
//! data, so the check runs on lists in script with no world.
class LF_EventSceneMarker
{
	string m_sSceneId;
	LF_EEventMarkerRole m_eRole;
	int m_iOrder;

	//------------------------------------------------------------------------------------------------
	static LF_EventSceneMarker Make(string sceneId, LF_EEventMarkerRole role, int order)
	{
		LF_EventSceneMarker marker = new LF_EventSceneMarker();
		marker.m_sSceneId = sceneId;
		marker.m_eRole = role;
		marker.m_iOrder = order;
		return marker;
	}
}

//------------------------------------------------------------------------------------------------
//! Checks every placed scene marker at game start (#2067, chapter #1015) and logs one
//! `[LFDebug:Events]` warning per broken scene, so a map pack finds a missing marker the moment the
//! server boots, not when a family walks off the road. Three faults: a gap in the route's orders
//! (waypoints 0, 2, 3), waypoints with no target, and no spawn marker. The check only reads and
//! logs; the scene stays usable and no event is switched off. Server only.
class LF_EventSceneCheck
{
	static const string FEATURE = "Events";
	//! Markers register as the world's entities initialise, which can finish after OnGameStart.
	static const int CHECK_DELAY_MS = 3000; // lf-fixed: engine-side settle time so placed markers have registered; not a gameplay tunable

	//------------------------------------------------------------------------------------------------
	//! Pure check. Appends one line per broken scene to faults, in the order scenes first appear.
	//! A marker with no scene id belongs to no scene and is skipped (its own component logs it).
	//! \return how many scenes are broken.
	static int Check(notnull array<ref LF_EventSceneMarker> markers, notnull array<string> faults)
	{
		array<string> scenes = {};
		foreach (LF_EventSceneMarker marker : markers)
		{
			if (!marker || marker.m_sSceneId.IsEmpty())
				continue;

			if (scenes.Find(marker.m_sSceneId) == -1)
				scenes.Insert(marker.m_sSceneId);
		}

		int broken = 0;
		foreach (string sceneId : scenes)
		{
			string line = CheckScene(sceneId, markers);
			if (line.IsEmpty())
				continue;

			faults.Insert(line);
			broken++;
		}

		return broken;
	}

	//------------------------------------------------------------------------------------------------
	//! \return "" when the scene is sound, else one line naming the scene and every fault once.
	protected static string CheckScene(string sceneId, notnull array<ref LF_EventSceneMarker> markers)
	{
		array<int> orders = {};
		int spawns = 0;
		int targets = 0;
		foreach (LF_EventSceneMarker marker : markers)
		{
			if (!marker || marker.m_sSceneId != sceneId)
				continue;

			if (marker.m_eRole == LF_EEventMarkerRole.WAYPOINT)
				orders.Insert(marker.m_iOrder);
			else if (marker.m_eRole == LF_EEventMarkerRole.SPAWN)
				spawns++;
			else
				targets++;
		}

		array<string> problems = {};

		// The route runs 0, 1, 2 ... : every order below the highest must be there.
		if (orders.Count() > 0)
		{
			int highest = 0;
			foreach (int order : orders)
			{
				if (order > highest)
					highest = order;
			}

			string missing = "";
			for (int wanted = 0; wanted < highest; wanted++)
			{
				if (orders.Find(wanted) != -1)
					continue;

				if (!missing.IsEmpty())
					missing += ", ";

				missing += wanted.ToString();
			}

			if (!missing.IsEmpty())
				problems.Insert(string.Format("a gap in the route's orders (missing %1)", missing));

			if (targets == 0)
				problems.Insert("waypoints but no target");
		}

		if (spawns == 0)
			problems.Insert("no spawn marker");

		if (problems.IsEmpty())
			return "";

		string line = string.Format("scene '%1' is broken: ", sceneId);
		foreach (int i, string problem : problems)
		{
			if (i > 0)
				line += "; ";

			line += problem;
		}

		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! Game start, server: runs the check once the world's markers have registered.
	static void RunSoon()
	{
		if (!Replication.IsServer() || !GetGame())
			return;

		GetGame().GetCallqueue().Remove(RunQueued);
		GetGame().GetCallqueue().CallLater(RunQueued, CHECK_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected static void RunQueued()
	{
		Run();
	}

	//------------------------------------------------------------------------------------------------
	//! Reads every placed marker through LF_Places and logs one warning per broken scene.
	//! \return how many scenes are broken. Never throws; a marker it cannot read is skipped.
	static int Run()
	{
		array<ref LF_EventSceneMarker> markers = {};
		Collect(LF_EventWaypointComponent.KIND_WAYPOINT, markers);
		Collect(LF_EventWaypointComponent.KIND_SPAWN, markers);
		Collect(LF_EventWaypointComponent.KIND_TARGET, markers);

		array<string> faults = {};
		int broken = Check(markers, faults);
		foreach (string line : faults)
		{
			LF_Debug.Warn(FEATURE, line);
		}

		LF_Debug.Info(FEATURE, string.Format("scene check: %1 marker(s) read, %2 scene(s) broken", markers.Count(), broken));
		return broken;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Collect(string kind, notnull array<ref LF_EventSceneMarker> into)
	{
		array<IEntity> entities = {};
		LF_Places.FindAll(kind, entities);
		foreach (IEntity entity : entities)
		{
			LF_EventWaypointComponent component = LF_EventWaypointComponent.Cast(entity.FindComponent(LF_EventWaypointComponent));
			if (!component)
				continue;

			into.Insert(LF_EventSceneMarker.Make(component.GetSceneId(), component.GetRole(), component.GetOrder()));
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Runs the check once per world at game start, on the server, without editing
//! LF_GameModeRoleplay.c (M4 rule).
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();

		LF_EventSceneCheck.RunSoon();
	}
}
