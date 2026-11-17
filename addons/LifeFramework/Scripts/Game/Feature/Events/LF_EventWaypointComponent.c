//------------------------------------------------------------------------------------------------
//! A placed scene marker of a story event (#1954, chapter #1015). A scene is the ground an event
//! plays on: a route the families walk (waypoints, in order), the spots they start from (spawn
//! markers) and the place they walk to (the target). Each marker registers in LF_Places under its
//! scene id, so an event names a scene by id only and core script holds no coordinates, map,
//! entity or layer names; a pack on another map places the markers.
//!
//! Place kinds and ids (LF_Places.OrderedId):
//!   event_waypoint  `<scene>#<order>`  one stop of the route, order from 0
//!   event_spawn     `<scene>#<order>`  one start spot, order from 0
//!   event_target    `<scene>`          the one place the route ends
//------------------------------------------------------------------------------------------------
enum LF_EEventMarkerRole
{
	WAYPOINT,
	SPAWN,
	TARGET
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Events", description: "Scene marker of a story event: a waypoint, a spawn spot or the target.")]
class LF_EventWaypointComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_EventWaypointComponent : ScriptComponent
{
	static const string FEATURE = "Events";
	static const string KIND_WAYPOINT = "event_waypoint";
	static const string KIND_SPAWN = "event_spawn";
	static const string KIND_TARGET = "event_target";

	[Attribute("", UIWidgets.EditBox, desc: "Scene id an event names (arrival_square). Every marker of one scene shares it")]
	protected string m_sSceneId;

	[Attribute("0", UIWidgets.ComboBox, desc: "WAYPOINT is a stop of the route, SPAWN a start spot, TARGET the place the route ends", enums: ParamEnumArray.FromEnum(LF_EEventMarkerRole))]
	protected LF_EEventMarkerRole m_eRole;

	[Attribute("0", UIWidgets.Auto, desc: "Position in the scene's route or spawn list, from 0. Unique per scene and role; the target ignores it")]
	protected int m_iOrder;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_sSceneId.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("scene marker on %1 has no scene id; no event can find it", owner.GetName()));
			return;
		}

		if (m_eRole != LF_EEventMarkerRole.TARGET && m_iOrder < 0)
		{
			LF_Debug.Error(FEATURE, string.Format("scene '%1' marker on %2 has order %3; orders start at 0", m_sSceneId, owner.GetName(), m_iOrder));
			return;
		}

		LF_Places.Register(KindOf(m_eRole), PlaceId(m_sSceneId, m_eRole, m_iOrder), owner);
	}

	//------------------------------------------------------------------------------------------------
	string GetSceneId()
	{
		return m_sSceneId;
	}

	//------------------------------------------------------------------------------------------------
	LF_EEventMarkerRole GetRole()
	{
		return m_eRole;
	}

	//------------------------------------------------------------------------------------------------
	int GetOrder()
	{
		return m_iOrder;
	}

	//------------------------------------------------------------------------------------------------
	//! The LF_Places kind a marker of this role registers under.
	static string KindOf(LF_EEventMarkerRole role)
	{
		if (role == LF_EEventMarkerRole.SPAWN)
			return KIND_SPAWN;

		if (role == LF_EEventMarkerRole.TARGET)
			return KIND_TARGET;

		return KIND_WAYPOINT;
	}

	//------------------------------------------------------------------------------------------------
	//! The LF_Places id of a marker: the scene id, plus `#order` for a route or spawn list; the target
	//! is one per scene and has no order.
	static string PlaceId(string sceneId, LF_EEventMarkerRole role, int order)
	{
		if (role == LF_EEventMarkerRole.TARGET)
			return sceneId;

		return LF_Places.OrderedId(sceneId, order);
	}

	//------------------------------------------------------------------------------------------------
	//! The scene's waypoints, lowest order first. A gap in the orders is kept (0, 2, 3), the list is
	//! still in order. \return how many were found.
	static int FindWaypoints(string sceneId, notnull array<IEntity> into)
	{
		return FindOrdered(KIND_WAYPOINT, sceneId, into);
	}

	//------------------------------------------------------------------------------------------------
	//! The scene's spawn markers, lowest order first. \return how many were found.
	static int FindSpawns(string sceneId, notnull array<IEntity> into)
	{
		return FindOrdered(KIND_SPAWN, sceneId, into);
	}

	//------------------------------------------------------------------------------------------------
	//! The scene's target, or null when the scene has none placed.
	static IEntity FindTarget(string sceneId)
	{
		if (sceneId.IsEmpty())
			return null;

		return LF_Places.Find(KIND_TARGET, sceneId);
	}

	//------------------------------------------------------------------------------------------------
	protected static int FindOrdered(string kind, string sceneId, notnull array<IEntity> into)
	{
		if (sceneId.IsEmpty())
			return 0;

		array<IEntity> all = {};
		LF_Places.FindAll(kind, all);

		// Insertion by order: a scene holds a handful of markers.
		array<int> orders = {};
		int start = into.Count();
		int found = 0;
		foreach (IEntity entity : all)
		{
			LF_EventWaypointComponent marker = LF_EventWaypointComponent.Cast(entity.FindComponent(LF_EventWaypointComponent));
			if (!marker || marker.GetSceneId() != sceneId)
				continue;

			int order = marker.GetOrder();
			int at = orders.Count();
			while (at > 0 && orders[at - 1] > order)
				at--;

			orders.InsertAt(order, at);
			into.InsertAt(entity, start + at);
			found++;
		}

		return found;
	}
}
