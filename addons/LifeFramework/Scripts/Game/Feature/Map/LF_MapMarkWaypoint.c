// LF_MapMarkWaypoint.c: a marker the player places on the map shows in the world (rc-map-waypoint).
//
// The vanilla placed-marker dialog stays (D-M7: every mark is personal and local). Placing one now also sets the
// one personal waypoint, LF_Waypoint: the HUD marker over the spot with its distance, the same marker the arrival
// guide and the town board use. It is client only, so nobody else sees it. It clears when the player walks
// within LF_Waypoint.ARRIVE_METERS of it, when they place another marker or set another waypoint, and when
// they delete the marker on the map. The marker itself stays on the map after arrival: it is the player's mark.
// The hooks are in LF_MapMarksSync.c (InsertStaticMarker / RemoveStaticMarker, the one choke point of every
// placing, quick-menu add and drag).

//------------------------------------------------------------------------------------------------
class LF_MapMarkWaypoint
{
	//! The LF_Waypoint id of a waypoint set by a placed marker.
	static const string ID = "marker";
	//! The name under the HUD marker when the marker carries no text of its own. A string key the widget translates.
	static const string LABEL = "#LF-MapMarkWaypoint_Label";

	//! The map position (whole metres) of the marker that owns the waypoint now.
	protected static bool s_bOwned;
	protected static int s_iX;
	protected static int s_iZ;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_bOwned = false;
		s_iX = 0;
		s_iZ = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! The label a marker's waypoint carries: its own text, else the neutral key.
	static string LabelOf(string markerText)
	{
		if (markerText.IsEmpty())
			return LABEL;

		return markerText;
	}

	//------------------------------------------------------------------------------------------------
	//! A marker was placed at the world position `position` (y is the ground there). Sets the waypoint,
	//! replacing whatever waypoint stood (there is one).
	//! \return true when the waypoint is set now.
	static bool Placed(int worldX, int worldZ, vector position, string markerText)
	{
		if (LF_Waypoint.IsActive())
			LF_Waypoint.Clear("a marker was placed");

		s_bOwned = LF_Waypoint.Mark(ID, LabelOf(markerText), position);
		if (s_bOwned)
		{
			s_iX = worldX;
			s_iZ = worldZ;
		}

		return s_bOwned;
	}

	//------------------------------------------------------------------------------------------------
	//! A personal marker was deleted at the map position (worldX, worldZ). Clears the waypoint when that marker
	//! owns it; another marker or another waypoint is left alone.
	//! \return true when the waypoint was cleared.
	static bool Removed(int worldX, int worldZ)
	{
		if (!s_bOwned || s_iX != worldX || s_iZ != worldZ)
			return false;

		s_bOwned = false;
		if (!LF_Waypoint.IsMarked(ID))
			return false;

		return LF_Waypoint.Clear("the marker was removed");
	}

	//------------------------------------------------------------------------------------------------
	//! The client hook: a personal marker the local player placed. A restore from the server (spawn,
	//! respawn, character switch) is not a placing and sets nothing.
	static void OnMarkerPlaced(notnull SCR_MapMarkerBase marker)
	{
		if (!GetGame() || RplSession.Mode() == RplMode.Dedicated)
			return;

		int pos[2];
		marker.GetWorldPos(pos);
		vector world = Vector(pos[0], 0, pos[1]);
		BaseWorld baseWorld = GetGame().GetWorld();
		if (baseWorld)
			world[1] = baseWorld.GetSurfaceY(pos[0], pos[1]);

		Placed(pos[0], pos[1], world, marker.GetCustomText());
	}

	//------------------------------------------------------------------------------------------------
	static void OnMarkerRemoved(notnull SCR_MapMarkerBase marker)
	{
		int pos[2];
		marker.GetWorldPos(pos);
		Removed(pos[0], pos[1]);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MapMarkWaypoint's statics (LF_StaticReset finds it by reflection).
class LF_MapMarkWaypoint_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MapMarkWaypoint";
	}

	override void Apply()
	{
		LF_MapMarkWaypoint.ResetStatics();
	}
}
