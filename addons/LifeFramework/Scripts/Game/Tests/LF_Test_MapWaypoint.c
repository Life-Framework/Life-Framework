// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapWaypoint: RED-OK (RedProof() skips the first click, the board draw and the move, so the click, board-draw and move checks fail). Green without it.

//------------------------------------------------------------------------------------------------
//! map/waypoint (#2235, epic #2223): a map click marks `LF_Waypoint` with id `map`, a board waypoint
//! is drawn on the map as the pencil circle, and a second click moves it. Pure script: the click
//! arrives as a world position, the draw is a segment list, so no map and no game session is needed.
// tier: LOGIC
class LF_Test_MapWaypoint : LF_Test
{
	override string GetName()
	{
		return "map/waypoint";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Waypoint.ResetStatics();

		vector drawn;
		string drawnLabel;
		ctx.False(LF_MapWaypoint.Drawn(drawn, drawnLabel), "no waypoint: nothing is drawn");

		RunClick(ctx);
		RunBoard(ctx);
		RunMove(ctx);
		RunMark(ctx);

		LF_Waypoint.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunClick(LF_TestContext ctx)
	{
		bool isSet;
		if (!RedProof())
			isSet = LF_MapWaypoint.SetFromMap("100 0 200");

		ctx.True(isSet, "a map click sets the waypoint");
		ctx.True(LF_Waypoint.IsMarked(LF_MapWaypoint.ID), "the waypoint has the map id");
		ctx.EqualStr("map", LF_Waypoint.GetId(), "the id is 'map'");
		ctx.EqualFloat(100, LF_Waypoint.GetTarget()[0], 0.01, "the target is the click, x");
		ctx.EqualFloat(200, LF_Waypoint.GetTarget()[2], 0.01, "the target is the click, z");
		ctx.False(LF_Waypoint.GetLabel().IsEmpty(), "the marker has a label");

		vector pos;
		string label;
		ctx.True(LF_MapWaypoint.Drawn(pos, label), "a map waypoint is drawn");
		ctx.EqualFloat(100, pos[0], 0.01, "the drawn position is the target, x");
		ctx.EqualStr(LF_Waypoint.GetLabel(), label, "the drawn label is the marker's label");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBoard(LF_TestContext ctx)
	{
		// A card on the town board marks the same single waypoint under its own id.
		ctx.True(LF_Waypoint.Mark("bank", "Bank", "300 0 40"), "a board card marks a waypoint");
		vector pos;
		string label;
		bool shown;
		if (!RedProof())
			shown = LF_MapWaypoint.Drawn(pos, label);

		ctx.True(shown, "a board waypoint is drawn on the map");
		ctx.EqualStr("Bank", label, "the board waypoint keeps its label");
		ctx.EqualFloat(300, pos[0], 0.01, "the board waypoint is drawn at its place");

		// A map click replaces the board waypoint (one waypoint only).
		ctx.True(LF_MapWaypoint.SetFromMap("10 0 20"), "a map click replaces the board waypoint");
		ctx.True(LF_Waypoint.IsMarked(LF_MapWaypoint.ID) && !LF_Waypoint.IsMarked("bank"), "only the map waypoint is set");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunMove(LF_TestContext ctx)
	{
		int before = LF_Waypoint.MarkCount();
		bool moved;
		if (!RedProof())
			moved = LF_MapWaypoint.SetFromMap("500 0 600");

		// LF_Waypoint.Mark with the same id would take the mark away; the map moves it instead.
		ctx.True(moved, "a second click sets a waypoint, it does not clear it");
		ctx.True(LF_Waypoint.IsMarked(LF_MapWaypoint.ID), "the map waypoint is still set");
		ctx.EqualFloat(500, LF_Waypoint.GetTarget()[0], 0.01, "the waypoint moved, x");
		ctx.EqualFloat(600, LF_Waypoint.GetTarget()[2], 0.01, "the waypoint moved, z");
		ctx.Equal(before + 1, LF_Waypoint.MarkCount(), "the move is one new mark");

		vector pos;
		string label;
		ctx.True(LF_MapWaypoint.Drawn(pos, label), "the moved waypoint is drawn");
		ctx.EqualFloat(500, pos[0], 0.01, "the drawn mark is at the new place");
	}

	//------------------------------------------------------------------------------------------------
	//! The pencil circle with a cross: a ring of segments and two crossing strokes, all within the radius.
	protected void RunMark(LF_TestContext ctx)
	{
		array<float> segments = {};
		LF_MapWaypoint.BuildMark(100, 50, 20, segments);
		ctx.Equal((LF_MapWaypoint.CIRCLE_SEGMENTS + 2) * 4, segments.Count(), "a ring of segments and two cross strokes");

		bool inside = true;
		float widest;
		for (int i = 0; i + 1 < segments.Count(); i += 2)
		{
			float d = Math.Sqrt(Math.Pow(segments[i] - 100, 2) + Math.Pow(segments[i + 1] - 50, 2));
			if (d > 20.01)
				inside = false;

			if (d > widest)
				widest = d;
		}

		ctx.True(inside, "every point is within the radius");
		ctx.EqualFloat(20, widest, 0.01, "the ring reaches the radius");

		// The cross strokes are the last two segments and pass through the centre.
		int cross = (LF_MapWaypoint.CIRCLE_SEGMENTS) * 4;
		ctx.EqualFloat(100, (segments[cross] + segments[cross + 2]) * 0.5, 0.01, "the first stroke crosses the centre, x");
		ctx.EqualFloat(50, (segments[cross + 1] + segments[cross + 3]) * 0.5, 0.01, "the first stroke crosses the centre, y");

		LF_Waypoint.Clear("test over");
		vector pos;
		string label;
		ctx.False(LF_MapWaypoint.Drawn(pos, label), "a cleared waypoint is not drawn");
		ctx.True(LF_MapWaypoint.SetFromMap("1 0 1"), "a click after a clear sets it again");
	}
}
