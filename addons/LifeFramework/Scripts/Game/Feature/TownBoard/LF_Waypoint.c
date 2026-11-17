// LF_Waypoint.c — one place the local player asked to be shown (the town board's Mark on map).
//
// Client only, nothing on the wire: the target is a world position the board already knows. While
// set, a small HUD marker (UI/Layouts/HUD/LF_Waypoint.layout) stands over the place with its name and
// the distance; at the screen edge when the place is off screen or behind. Walking within
// ARRIVE_METERS clears it, and so does marking another place or the same card again. It never moves
// the player: no teleport, no fast travel.

class LF_Waypoint
{
	static const string FEATURE = "Waypoint";
	//! Arrived: the mark goes away this close to the place (flat distance).
	static const float ARRIVE_METERS = 8;	// lf-fixed: a doorway's reach, not tuning

	protected static bool s_bActive;
	protected static string s_sId;
	protected static string s_sLabel;
	protected static vector s_vTarget;
	protected static int s_iMarks;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		LF_WaypointHud.Remove();
		s_bActive = false;
		s_sId = string.Empty;
		s_sLabel = string.Empty;
		s_vTarget = vector.Zero;
		s_iMarks = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Marks the place `id` (a card id) at `target` named `label`. The same id again takes the mark
	//! away; another id replaces it. An empty id or label is refused (logged).
	//! \return true when a mark is set now, false when none is (refused or taken away).
	static bool Mark(string id, string label, vector target)
	{
		if (id.IsEmpty() || label.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("mark refused: id '%1' label '%2'", id, label));
			return false;
		}

		if (s_bActive && s_sId == id)
		{
			Clear("marked again");
			return false;
		}

		s_bActive = true;
		s_sId = id;
		s_sLabel = label;
		s_vTarget = target;
		s_iMarks++;
		LF_Debug.Info(FEATURE, string.Format("marked %1 '%2' at %3", id, label, target));
		LF_WaypointHud.Show();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the mark away. \return true when there was one.
	static bool Clear(string why)
	{
		if (!s_bActive)
			return false;

		LF_Debug.Info(FEATURE, string.Format("cleared %1 (%2)", s_sId, why));
		s_bActive = false;
		s_sId = string.Empty;
		s_sLabel = string.Empty;
		LF_WaypointHud.Remove();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The player stands at `position`: within ARRIVE_METERS of the mark clears it.
	//! \return true when this arrival cleared the mark.
	static bool Arrive(vector position)
	{
		if (!s_bActive || vector.DistanceXZ(position, s_vTarget) > ARRIVE_METERS)
			return false;

		return Clear("arrived");
	}

	//------------------------------------------------------------------------------------------------
	static bool IsActive()
	{
		return s_bActive;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsMarked(string id)
	{
		return s_bActive && s_sId == id;
	}

	//------------------------------------------------------------------------------------------------
	static string GetId()
	{
		return s_sId;
	}

	//------------------------------------------------------------------------------------------------
	static string GetLabel()
	{
		return s_sLabel;
	}

	//------------------------------------------------------------------------------------------------
	static vector GetTarget()
	{
		return s_vTarget;
	}

	//------------------------------------------------------------------------------------------------
	//! Marks set since the world started. Proofs read it.
	static int MarkCount()
	{
		return s_iMarks;
	}

	//------------------------------------------------------------------------------------------------
	//! The harness's `seen`: "<id>@<metres from origin>/<hud: shown|hidden|none>", or "-" with no mark.
	static string Describe(vector origin)
	{
		if (!s_bActive)
			return "-";

		int meters = Math.Round(vector.DistanceXZ(origin, s_vTarget));
		return string.Format("%1@%2/%3", s_sId, meters, LF_WaypointHud.State());
	}
}

//------------------------------------------------------------------------------------------------
//! The marker on the HUD. Built when a mark is set on a client with a workspace, removed when it
//! clears. One timer while it shows: project the place to the screen, print the distance, and check
//! for arrival.
class LF_WaypointHud
{
	static const ResourceName LAYOUT = "{5B2E9D714C8A3F06}UI/Layouts/HUD/LF_Waypoint.layout";
	static const int TICK_MS = 33;	// lf-fixed: follows the camera smoothly
	static const float EDGE = 48;	// lf-fixed: how far inside the screen edge a marker off screen sits
	//! The marker's anchor sits this far above the place (a sign plate is at head height).
	static const float LIFT_METERS = 1.5;	// lf-fixed: reads over the door, not in the gravel

	protected static Widget s_wRoot;
	protected static Widget s_wMarker;
	protected static TextWidget s_wLabel;
	protected static TextWidget s_wDistance;
	protected static bool s_bTicking;
	protected static bool s_bOnScreen;

	//------------------------------------------------------------------------------------------------
	//! Builds the marker for the current mark, where something renders. A dedicated server, a test
	//! world without a workspace or a missing layout keeps the mark without a marker (logged once).
	static void Show()
	{
		Remove();
		if (!GetGame() || RplSession.Mode() == RplMode.Dedicated)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		s_wRoot = workspace.CreateWidgets(LAYOUT);
		if (!s_wRoot)
		{
			LF_Debug.Error(LF_Waypoint.FEATURE, "the waypoint marker layout did not load; the mark has no marker: " + LAYOUT);
			return;
		}

		s_wMarker = s_wRoot.FindAnyWidget("Marker");
		s_wLabel = TextWidget.Cast(s_wRoot.FindAnyWidget("Label"));
		s_wDistance = TextWidget.Cast(s_wRoot.FindAnyWidget("Distance"));
		if (s_wLabel)
			s_wLabel.SetText(LF_Waypoint.GetLabel());

		if (GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
			s_bTicking = true;
		}

		Tick();
	}

	//------------------------------------------------------------------------------------------------
	static void Remove()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();

		s_wRoot = null;
		s_wMarker = null;
		s_wLabel = null;
		s_wDistance = null;
		s_bOnScreen = false;
	}

	//------------------------------------------------------------------------------------------------
	//! "shown" (on screen), "edge" (pinned to the edge, the place is off screen), "none" (no marker).
	static string State()
	{
		if (!s_wRoot)
			return "none";

		if (s_bOnScreen)
			return "shown";

		return "edge";
	}

	//------------------------------------------------------------------------------------------------
	//! Where the marker goes on a screen `width` x `height` (reference units) for a projected point
	//! (x, y, depth). Behind the camera the point is mirrored and pinned to the bottom edge; off screen
	//! it is pinned inside the edge. \return true when the point itself is on screen.
	static bool Place(vector projected, float width, float height, out float x, out float y)
	{
		x = projected[0];
		y = projected[1];
		bool behind = projected[2] < 0;
		if (behind)
		{
			x = width - x;
			y = height - EDGE;
		}

		bool onScreen = !behind && x >= 0 && x <= width && y >= 0 && y <= height;
		x = Math.Clamp(x, EDGE, width - EDGE);
		y = Math.Clamp(y, EDGE, height - EDGE);
		return onScreen;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!LF_Waypoint.IsActive() || !s_wRoot)
		{
			Remove();
			return;
		}

		IEntity me = LF_Utils.GetLocalCharacter();
		if (me && LF_Waypoint.Arrive(me.GetOrigin()))
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();
		if (!workspace || !world || !s_wMarker)
			return;

		vector target = LF_Waypoint.GetTarget() + Vector(0, LIFT_METERS, 0);
		vector projected = workspace.ProjWorldToScreen(target, world);
		float width = workspace.DPIUnscale(workspace.GetWidth());
		float height = workspace.DPIUnscale(workspace.GetHeight());
		float x;
		float y;
		s_bOnScreen = Place(projected, width, height, x, y);
		FrameSlot.SetPos(s_wMarker, x, y);
		if (s_wDistance && me)
		{
			int meters = Math.Round(vector.DistanceXZ(me.GetOrigin(), LF_Waypoint.GetTarget()));
			s_wDistance.SetText(WidgetManager.Translate("#LF-Waypoint_Distance", meters));
		}
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_Waypoint's statics (registered at script load; Core/LF_StaticReset.c).
class LF_Waypoint_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_Waypoint_StaticResetHook());

	override string Owner()
	{
		return "LF_Waypoint";
	}

	override void Apply()
	{
		LF_Waypoint.ResetStatics();
	}
}
