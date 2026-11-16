//------------------------------------------------------------------------------------------------
//! Where an event plays and who stands there (#2105, chapter #1015). An event names its place by
//! scene id, the id its `event_target` marker (LF_EventWaypointComponent, #1954) registers in
//! LF_Places, plus a radius; the shell asks here how many players are inside it, so a PLAYERS_NEARBY
//! condition can hold and a NEARBY_BELOW abort fires only on an empty place. Core holds no
//! coordinates: the position comes from the marker, found by component through LF_Places.
//!
//! The binding is made in code by whatever owns the event (a module, a pack's script); the event
//! config has no place field yet. A radius can be set per phase (a wider ring while the families
//! arrive, a tight one at the table); a phase with none uses the event's. An event with no
//! binding, or one whose marker is not placed, counts 0 and logs one warning. Server only.
//!
//! Tests feed positions with UseTestPositions in place of the connected players.
class LF_EventPlace
{
	static const string FEATURE = "Events";

	protected static ref map<string, string> s_mScenes;						// event id -> scene id
	protected static ref map<string, float> s_mRadii;							// event id -> radius (m)
	protected static ref map<string, float> s_mPhaseRadii;						// "event|phase" -> radius (m)
	protected static ref set<string> s_aWarned;									// events already warned about
	protected static ref array<vector> s_aTestPositions;						// null: count the real players

	//------------------------------------------------------------------------------------------------
	//! Binds `eventId` to the scene whose `event_target` marker is its place. Binding again replaces it.
	//! \return false (logged) for an empty id, an empty scene or a radius that is not above 0.
	static bool Bind(string eventId, string sceneId, float radius)
	{
		if (eventId.IsEmpty() || sceneId.IsEmpty() || radius <= 0)
		{
			LF_Debug.Error(FEATURE, string.Format("place bind refused: event '%1' scene '%2' radius %3", eventId, sceneId, radius));
			return false;
		}

		Scenes().Set(eventId, sceneId);
		Radii().Set(eventId, radius);
		if (s_aWarned)
			s_aWarned.RemoveItem(eventId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A radius for one phase of a bound event, over the event's. \return false (logged) for a bad argument.
	static bool BindPhaseRadius(string eventId, string phaseName, float radius)
	{
		if (eventId.IsEmpty() || phaseName.IsEmpty() || radius <= 0)
		{
			LF_Debug.Error(FEATURE, string.Format("phase radius refused: event '%1' phase '%2' radius %3", eventId, phaseName, radius));
			return false;
		}

		PhaseRadii().Set(PhaseKey(eventId, phaseName), radius);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops an event's place and its phase radii (tests, an event that shuts down).
	static void Unbind(string eventId)
	{
		Scenes().Remove(eventId);
		Radii().Remove(eventId);

		array<string> keys = {};
		string prefix = eventId + "|";
		foreach (string key, float radius : PhaseRadii())
		{
			if (key.IndexOf(prefix) == 0)
				keys.Insert(key);
		}
		foreach (string stale : keys)
			PhaseRadii().Remove(stale);
	}

	//------------------------------------------------------------------------------------------------
	//! \return the scene id `eventId` is bound to, "" when it has no place.
	static string SceneOf(string eventId)
	{
		string sceneId;
		Scenes().Find(eventId, sceneId);
		return sceneId;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the radius in metres that applies to `eventId` in `phaseName`, 0 when it has no place.
	static float RadiusOf(string eventId, string phaseName)
	{
		float radius;
		if (PhaseRadii().Find(PhaseKey(eventId, phaseName), radius))
			return radius;

		Radii().Find(eventId, radius);
		return radius;
	}

	//------------------------------------------------------------------------------------------------
	//! How many players stand within the radius of the event's place. 0 (one warning per event) when
	//! the event has no place or its marker is not placed.
	static int CountNearby(string eventId, string phaseName)
	{
		string sceneId = SceneOf(eventId);
		if (sceneId.IsEmpty())
		{
			WarnOnce(eventId, string.Format("event %1 has no place: counting 0 players nearby (bind it with LF_EventPlace.Bind)", eventId));
			return 0;
		}

		vector center;
		if (!LF_Places.FindOrigin(LF_EventWaypointComponent.KIND_TARGET, sceneId, center))
		{
			WarnOnce(eventId, string.Format("event %1 place: scene '%2' has no event_target marker placed: counting 0 players nearby", eventId, sceneId));
			return 0;
		}

		float radius = RadiusOf(eventId, phaseName);
		array<vector> positions = {};
		PlayerPositions(positions);

		int nearby;
		foreach (vector position : positions)
		{
			if (vector.Distance(position, center) <= radius)
				nearby++;
		}
		return nearby;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: count these positions instead of the connected players.
	static void UseTestPositions(notnull array<vector> positions)
	{
		s_aTestPositions = {};
		foreach (vector position : positions)
			s_aTestPositions.Insert(position);
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: back to the connected players.
	static void ClearTestPositions()
	{
		s_aTestPositions = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true once CountNearby has warned about `eventId` (and not since a Bind).
	static bool HasWarned(string eventId)
	{
		return s_aWarned && s_aWarned.Contains(eventId);
	}

	//------------------------------------------------------------------------------------------------
	protected static void PlayerPositions(notnull array<vector> into)
	{
		if (s_aTestPositions)
		{
			into.Copy(s_aTestPositions);
			return;
		}

		if (!GetGame())
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity body = playerManager.GetPlayerControlledEntity(playerId);
			if (body)
				into.Insert(body.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void WarnOnce(string eventId, string message)
	{
		if (!s_aWarned)
			s_aWarned = new set<string>();
		if (s_aWarned.Contains(eventId))
			return;

		s_aWarned.Insert(eventId);
		LF_Debug.Warn(FEATURE, message);
	}

	//------------------------------------------------------------------------------------------------
	protected static string PhaseKey(string eventId, string phaseName)
	{
		return eventId + "|" + phaseName;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, string> Scenes()
	{
		if (!s_mScenes)
			s_mScenes = new map<string, string>();
		return s_mScenes;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, float> Radii()
	{
		if (!s_mRadii)
			s_mRadii = new map<string, float>();
		return s_mRadii;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, float> PhaseRadii()
	{
		if (!s_mPhaseRadii)
			s_mPhaseRadii = new map<string, float>();
		return s_mPhaseRadii;
	}

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset) and tests.
	static void ResetStatics()
	{
		s_mScenes = null;
		s_mRadii = null;
		s_mPhaseRadii = null;
		s_aWarned = null;
		s_aTestPositions = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_EventPlace's statics (Core/LF_StaticReset.c).
class LF_EventPlace_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_EventPlace_StaticResetHook());

	override string Owner()
	{
		return "LF_EventPlace";
	}

	override void Apply()
	{
		LF_EventPlace.ResetStatics();
	}
}
