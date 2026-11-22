// red-proof: 2026-11-22, tools/cli test --no-build --red LF_Test_MapMarkWaypoint: RED-OK map/marker-waypoint (24 assertions fail with the hook on); green without it.
// red-proof: RedProof() skips the placing, so no waypoint is set and the placed, label, replace and removed checks fail.
//------------------------------------------------------------------------------------------------
//! map/marker-waypoint (rc-map-waypoint): a marker the player places on the map sets the one personal waypoint
//! (the HUD marker with distance); a second marker moves it; deleting that marker clears it; deleting any other
//! marker, or one whose waypoint another feature took over, leaves the waypoint alone. Pure script: the hook gives
//! the position and text, so no map and no game session is needed.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapMarkWaypoint : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/marker-waypoint";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_Waypoint.ResetStatics();
		LF_MapMarkWaypoint.ResetStatics();

		ctx.EqualStr(LF_MapMarkWaypoint.LABEL, LF_MapMarkWaypoint.LabelOf(""), "a marker with no text is named by the neutral key");
		ctx.EqualStr("Dock", LF_MapMarkWaypoint.LabelOf("Dock"), "a marker's own text names its waypoint");
		string caption = WidgetManager.Translate(LF_MapMarkWaypoint.LABEL);
		ctx.True(caption != LF_MapMarkWaypoint.LABEL && !caption.IsEmpty(), "the neutral name is in the string table");

		// Placing sets the waypoint.
		bool isSet;
		if (!RedProof())
			isSet = LF_MapMarkWaypoint.Placed(100, 200, "100 5 200", "");

		ctx.True(isSet, "placing a marker sets the waypoint");
		ctx.True(LF_Waypoint.IsMarked(LF_MapMarkWaypoint.ID), "the waypoint carries the marker id");
		ctx.EqualFloat(100, LF_Waypoint.GetTarget()[0], 0.01, "the target is the marker, x");
		ctx.EqualFloat(200, LF_Waypoint.GetTarget()[2], 0.01, "the target is the marker, z");
		ctx.EqualStr(LF_MapMarkWaypoint.LABEL, LF_Waypoint.GetLabel(), "the label is the neutral key");

		// A second marker moves it: still one waypoint, not a toggle.
		LF_MapMarkWaypoint.Placed(300, 400, "300 5 400", "Dock");
		ctx.True(LF_Waypoint.IsMarked(LF_MapMarkWaypoint.ID), "a second marker keeps one waypoint");
		ctx.EqualFloat(300, LF_Waypoint.GetTarget()[0], 0.01, "the waypoint moved to the second marker");
		ctx.EqualStr("Dock", LF_Waypoint.GetLabel(), "the second marker's text names it");

		// Deleting the first marker leaves the waypoint on the second.
		ctx.False(LF_MapMarkWaypoint.Removed(100, 200), "deleting another marker leaves the waypoint");
		ctx.True(LF_Waypoint.IsMarked(LF_MapMarkWaypoint.ID), "the waypoint is still set");

		// Deleting the marker that owns it clears it.
		ctx.True(LF_MapMarkWaypoint.Removed(300, 400), "deleting the marker clears the waypoint");
		ctx.False(LF_Waypoint.IsActive(), "no waypoint is left");

		// A waypoint another feature took over is not cleared by a later delete.
		LF_MapMarkWaypoint.Placed(500, 600, "500 5 600", "");
		LF_Waypoint.Mark("bank", "Bank", "10 0 10");
		ctx.False(LF_MapMarkWaypoint.Removed(500, 600), "deleting the marker does not clear a waypoint the board set");
		ctx.True(LF_Waypoint.IsMarked("bank"), "the board waypoint stays");

		// Arriving clears the waypoint (the shared rule); the marker's delete after that is a no-op.
		LF_Waypoint.ResetStatics();
		LF_MapMarkWaypoint.Placed(700, 800, "700 5 800", "");
		ctx.True(LF_Waypoint.Arrive("700 5 800"), "walking to the marker clears the waypoint");
		ctx.False(LF_MapMarkWaypoint.Removed(700, 800), "deleting a reached marker has nothing to clear");

		LF_Waypoint.ResetStatics();
		LF_MapMarkWaypoint.ResetStatics();
	}
}
