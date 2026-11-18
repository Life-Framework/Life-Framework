// red-proof: [LFTEST] RED-OK townboard/cards (fails with the hook on, 49 assertions) (tools/cli test --no-build --red, 2026-11-16)
// red-proof: #3077 at MAX_CARDS 8 the twelve-card board fails (`twelve config cards are all shown`, and no warning at thirteen).
//! The town board's cards and the waypoint, with no world: which cards show and in which slot, what a
//! card says about a place it cannot find, the compass and distance words, the slot geometry the
//! prefab's contexts follow, and the waypoint's set, replace, toggle, arrive and refuse rules.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_TownBoard : LF_Test
{
	override string GetName()
	{
		return "townboard/cards";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_TownBoardCard MakeCard(string id, string placeId)
	{
		LF_TownBoardCard card = new LF_TownBoardCard();
		card.m_sId = id;
		card.m_sTitle = "Title " + id;
		card.m_sCategory = "Cat";
		card.m_sPlaceKind = "LF_Test_NoSuchKind";
		card.m_sPlaceId = placeId;
		return card;
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Waypoint.ResetStatics();
		// Twelve cards fill the board and nothing is dropped, so nothing is warned about.
		LF_TownBoardConfig config = new LF_TownBoardConfig();
		config.m_sTitle = "Test board";
		config.m_aCards = {};
		for (int i = 0; i < 12; i++)
			config.m_aCards.Insert(MakeCard("c" + i.ToString(), "nowhere"));

		LF_TownBoard.UseConfig(config);
		ctx.Equal(12, LF_TownBoard.MAX_CARDS, "the board has twelve slots");
		ctx.Equal(12, LF_TownBoard.CardCount(), "twelve config cards are all shown");
		ctx.EqualStr("c11", LF_TownBoard.CardAt(11).m_sId, "slot 11 is the twelfth card");
		ctx.Equal(0, LF_TownBoard.DropWarnCount(), "twelve cards drop none, so no warning");

		// A thirteenth card is dropped, with one LF_Debug warning for the config; the first twelve stay.
		config.m_aCards.Insert(MakeCard("c12", "nowhere"));
		LF_TownBoard.UseConfig(config);
		if (!RedProof())
			ctx.Equal(1, LF_TownBoard.DropWarnCount(), "a thirteenth card is dropped with a warning");
		else
			ctx.Equal(1, LF_TownBoard.DropWarnCount() + 1, "a thirteenth card is dropped with a warning");

		ctx.Equal(LF_TownBoard.MAX_CARDS, LF_TownBoard.CardCount(), "thirteen cards fill the twelve slots");
		ctx.EqualStr("c0", LF_TownBoard.CardAt(0).m_sId, "slot 0 is the first card");
		ctx.True(LF_TownBoard.CardAt(12) == null, "a thirteenth card has no slot");
		ctx.True(LF_TownBoard.CardAt(-1) == null, "a negative slot is empty");
		ctx.True(LF_TownBoard.FindCard("c12") == null, "a card past the slots is not on the board");
		ctx.True(LF_TownBoard.FindCard("c11") != null, "the twelfth card is on the board");
		ctx.EqualStr("TEST BOARD", LF_TownBoard.Title(), "the header is printed in capitals");

		// A place that is not on this map: the card says so, and there is nothing to mark.
		vector place;
		ctx.False(LF_TownBoard.FindPlace(LF_TownBoard.CardAt(0), place), "a card whose place is not placed finds nothing");
		ctx.EqualStr(LF_TownBoard.Text(LF_TownBoard.KEY_NO_PLACE), LF_TownBoard.WhereText(LF_TownBoard.CardAt(0), vector.Zero), "it reads as no place");
		ctx.EqualStr("Title c0", LF_TownBoard.PlaceName(LF_TownBoard.CardAt(0)), "an unnamed place falls back to the card title");

		// Compass (+Z north, +X east) and distance words.
		ctx.Equal(0, LF_TownBoard.Direction(vector.Zero, "0 0 100"), "north");
		ctx.Equal(2, LF_TownBoard.Direction(vector.Zero, "100 0 0"), "east");
		ctx.Equal(4, LF_TownBoard.Direction(vector.Zero, "0 0 -100"), "south");
		ctx.Equal(6, LF_TownBoard.Direction(vector.Zero, "-100 0 0"), "west");
		ctx.Equal(1, LF_TownBoard.Direction(vector.Zero, "70 0 70"), "north-east");
		ctx.Equal(7, LF_TownBoard.Direction(vector.Zero, "-70 0 72"), "north-west");
		ctx.Equal(0, LF_TownBoard.Direction(vector.Zero, "-5 0 100"), "just west of north is north");
		ctx.Equal(10, LF_TownBoard.RoundDistance(3), "never under 10 m");
		ctx.Equal(60, LF_TownBoard.RoundDistance(57), "to 10 m under 200");
		ctx.Equal(250, LF_TownBoard.RoundDistance(262), "to 50 m from 200");

		// The slots: four across, three down, inside the face, mirrored about its centre.
		vector first = LF_TownBoardFace.SlotPosition(0);
		vector last = LF_TownBoardFace.SlotPosition(LF_TownBoard.MAX_CARDS - 1);
		ctx.True(first[0] >= 0 && first[1] >= 0, "the first card is on the face");
		ctx.True(last[0] + LF_TownBoardFace.CARD_WIDTH <= LF_WantedPosterFace.FACE_WIDTH && last[1] + LF_TownBoardFace.CARD_HEIGHT <= LF_WantedPosterFace.FACE_HEIGHT, "the last card is on the face");
		vector a = LF_TownBoardFace.SlotCentreMeters(0, 0.965);
		vector b = LF_TownBoardFace.SlotCentreMeters(3, 0.965);
		ctx.EqualFloat(-a[0], b[0], 0.001, "the top row is centred left to right");
		ctx.True(a[0] < 0 && a[1] > 0, "slot 0 is top left as the reader sees it");
		ctx.True(LF_TownBoardFace.SlotCentreMeters(4, 0.965)[1] < 0, "slot 4 starts the lower row");
		ctx.True(LF_TownBoardFace.SlotPosition(8)[1] > LF_TownBoardFace.SlotPosition(4)[1], "slot 8 starts a third row");
		ctx.True(LF_TownBoardFace.SlotPosition(0)[1] >= LF_TownBoardFace.HEADER_TOP + LF_TownBoardFace.HEADER_HEIGHT, "the cards start below the header");
		ctx.True(LF_TownBoardFace.SlotPosition(4)[1] >= LF_TownBoardFace.SlotPosition(0)[1] + LF_TownBoardFace.CARD_HEIGHT, "the rows do not overlap");
		ctx.True(LF_TownBoardFace.SlotPosition(8)[1] >= LF_TownBoardFace.SlotPosition(4)[1] + LF_TownBoardFace.CARD_HEIGHT, "the third row does not overlap the second");
		ctx.True(LF_TownBoardFace.SlotPosition(1)[0] >= LF_TownBoardFace.SlotPosition(0)[0] + LF_TownBoardFace.CARD_WIDTH, "the columns do not overlap");
		// Neighbouring card contexts (0.12 m radius in the prefab) must not reach each other's centre.
		float rowStep = vector.Distance(LF_TownBoardFace.SlotCentreMeters(0, 0.965), LF_TownBoardFace.SlotCentreMeters(4, 0.965));
		ctx.True(rowStep > 0.24, "the card contexts of two rows do not overlap");

		// The waypoint: set, the same card again takes it off, another replaces it, arriving clears it.
		ctx.False(LF_Waypoint.Mark("", "Bank", "10 0 10"), "a mark with no id is refused");
		ctx.False(LF_Waypoint.IsActive(), "a refused mark sets nothing");
		bool marked = false;
		if (!RedProof())
			marked = LF_Waypoint.Mark("bank", "Bank", "100 0 50");

		ctx.True(marked, "marking a card's place sets the waypoint");
		ctx.True(LF_Waypoint.IsMarked("bank"), "the waypoint is that card");
		ctx.EqualStr("Bank", LF_Waypoint.GetLabel(), "the marker names the place");
		ctx.True(LF_Waypoint.Mark("hospital", "Hospital", "200 0 0"), "another card replaces the mark");
		ctx.True(LF_Waypoint.IsMarked("hospital") && !LF_Waypoint.IsMarked("bank"), "only the new card is marked");
		ctx.False(LF_Waypoint.Arrive("150 0 0"), "50 m away is not there yet");
		ctx.True(LF_Waypoint.IsActive(), "the mark stays on the way");
		ctx.True(LF_Waypoint.Arrive("195 0 3"), "within 8 m the player has arrived");
		ctx.False(LF_Waypoint.IsActive(), "arriving clears the mark");
		ctx.True(LF_Waypoint.Mark("bank", "Bank", "100 0 50"), "marking again after arrival sets it");
		ctx.False(LF_Waypoint.Mark("bank", "Bank", "100 0 50"), "the same card again takes the mark off");
		ctx.False(LF_Waypoint.IsActive(), "nothing is marked then");

		// The HUD marker on a 1920 x 1080 screen: on screen as projected, else pinned inside the edge.
		float x;
		float y;
		ctx.True(LF_WaypointHud.Place("960 500 20", 1920, 1080, x, y), "a point ahead is on screen");
		ctx.EqualFloat(960, x, 0.01, "and drawn where it projects");
		ctx.False(LF_WaypointHud.Place("2500 500 20", 1920, 1080, x, y), "a point off to the right is off screen");
		ctx.EqualFloat(1920 - LF_WaypointHud.EDGE, x, 0.01, "and pinned to the right edge");
		ctx.False(LF_WaypointHud.Place("300 500 -20", 1920, 1080, x, y), "a point behind is off screen");
		ctx.EqualFloat(1080 - LF_WaypointHud.EDGE, y, 0.01, "and pinned to the bottom edge");
		ctx.EqualFloat(1620, x, 0.01, "mirrored, on the side the player should turn to");

		LF_Waypoint.ResetStatics();
		LF_TownBoard.UseConfig(null);
	}
}
