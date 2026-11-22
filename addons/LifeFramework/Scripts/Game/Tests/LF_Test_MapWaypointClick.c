// red-proof: 2026-11-22, cli test --no-build --red LF_Test_MapDeclutter,LF_Test_MapWaypointClick: RED PROOF OK (RedProof() skips the Shift-click set, the clear and the place click, so those checks fail). Green without it.

//------------------------------------------------------------------------------------------------
//! map/waypoint-click (0.2.0 playtest: "setting a waypoint is not intuitive"): Shift + left click (gamepad
//! Y + A) sets the one waypoint at the cursor or on the place under it, the same on the waypoint clears it;
//! a plain click on a place sets it and clears it the second time; the hint and the card say so for the
//! device in use. Pure script: the click arrives as a world position, so no map is needed.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapWaypointClick : LF_Test
{
	override string GetName()
	{
		return "map/waypoint-click";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Waypoint.ResetStatics();
		RunShiftClick(ctx);
		LF_Waypoint.ResetStatics();
		RunPlaceClick(ctx);
		LF_Waypoint.ResetStatics();
		RunWords(ctx);
		LF_Waypoint.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunShiftClick(LF_TestContext ctx)
	{
		string outcome;
		if (!RedProof())
			outcome = LF_MapWaypoint.ModifiedClick("100 0 200", 30, false, "", vector.Zero);

		ctx.EqualStr("point", outcome, "Shift + click on open ground sets the waypoint there");
		ctx.True(LF_Waypoint.IsMarked(LF_MapWaypoint.ID), "the waypoint is the map's");
		ctx.EqualFloat(100, LF_Waypoint.GetTarget()[0], 0.01, "at the click, x");
		ctx.EqualFloat(200, LF_Waypoint.GetTarget()[2], 0.01, "at the click, z");

		ctx.EqualStr("point", LF_MapWaypoint.ModifiedClick("160 0 200", 30, false, "", vector.Zero), "Shift + click elsewhere moves it");
		ctx.EqualFloat(160, LF_Waypoint.GetTarget()[0], 0.01, "it moved");

		string cleared;
		if (!RedProof())
			cleared = LF_MapWaypoint.ModifiedClick("165 0 203", 30, false, "", vector.Zero);

		ctx.EqualStr("cleared", cleared, "Shift + click on the waypoint clears it");
		ctx.False(LF_Waypoint.IsActive(), "no waypoint after the clear");

		ctx.EqualStr("place", LF_MapWaypoint.ModifiedClick("51 0 52", 30, true, "Bank, Testville", "50 0 50"), "Shift + click over a place sets it on the place");
		ctx.EqualStr("Bank, Testville", LF_Waypoint.GetLabel(), "with the place's name");
		ctx.EqualFloat(50, LF_Waypoint.GetTarget()[0], 0.01, "at the place, not the click");

		// A waypoint the town board set is the one waypoint too: Shift + click on it clears it.
		LF_Waypoint.Clear("test");
		LF_Waypoint.Mark("board-card", "Clinic", "700 0 700");
		ctx.EqualStr("cleared", LF_MapWaypoint.ModifiedClick("702 0 698", 30, false, "", vector.Zero), "Shift + click on a board waypoint clears it");

		LF_Waypoint.Mark("board-card", "Clinic", "700 0 700");
		ctx.True(LF_MapWaypoint.ClearFromMap(), "the clear key clears a waypoint");
		ctx.False(LF_MapWaypoint.ClearFromMap(), "the clear key with no waypoint does nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPlaceClick(LF_TestContext ctx)
	{
		string first;
		if (!RedProof())
			first = LF_MapPlaceCard.ClickPlace("Bakery, Testville", "10 0 10");

		ctx.EqualStr("set", first, "a click on a place sets the waypoint there");
		ctx.True(LF_MapPlaceCard.IsWaypoint("10 0 10"), "the place is the waypoint");
		ctx.EqualStr("set", LF_MapPlaceCard.ClickPlace("Bank, Testville", "90 0 10"), "a click on another place moves it");
		ctx.False(LF_MapPlaceCard.IsWaypoint("10 0 10"), "the first place is no longer the waypoint");

		string again;
		if (!RedProof())
			again = LF_MapPlaceCard.ClickPlace("Bank, Testville", "90 0 10");

		ctx.EqualStr("cleared", again, "a second click on the waypoint's place clears it");
		ctx.False(LF_Waypoint.IsActive(), "no waypoint after the second click");
		ctx.EqualStr("refused", LF_MapPlaceCard.ClickPlace("", "1 0 1"), "a place with no name is refused");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWords(LF_TestContext ctx)
	{
		ctx.EqualStr("#LF-MapPlaces_CardSet", LF_MapPlaceCard.NoteKey(false, false), "the card says a click sets it");
		ctx.EqualStr("#LF-MapPlaces_CardClearPad", LF_MapPlaceCard.NoteKey(true, true), "on the waypoint's place, the card says how to clear it");
		ctx.Equal(340, LF_MapWaypoint.DistanceMeters("0 0 0", "300 50 160"), "the distance is flat metres, as the HUD prints it");
		array<string> names = {"ATM", "Cafe", "Bakery", "Barber", "Bank"};
		ctx.EqualStr("ATM, Cafe, Bakery +2", LF_MapPlaceCard.JoinNames(names), "a crowded mark's card names three and counts the rest");
	}
}
