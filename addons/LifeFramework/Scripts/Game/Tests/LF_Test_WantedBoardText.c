// red-proof: with `RedProof()` on the poster builders are not asked, so every text stays "?" and the star, key, place, headline
// and description checks fail.

//! police/wanted-board-text: the words on a wanted poster (LF_WantedBoard). The star row is always five marks wide and
//! clamps a level outside 0..5, a `#key` is translated while plain text passes through, a place of the form
//! "<grid key> 002-001" is built from the grid term (a bare grid key is only a key), the headline names an unidentified
//! suspect "Unknown person" and carries the offence and the level, and the description lists the clothing (empty pieces
//! dropped), then on foot or driving, then where the suspect was last seen only when a place is known. Every expected
//! text is built from the same terms, so a re-worded string passes and a dropped piece fails.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_WantedBoardText : LF_Test
{
	override string GetName()
	{
		return "police/wanted-board-text";
	}

	override void Run(LF_TestContext ctx)
	{
		string none = "?";
		string some = "?";
		string full = "?";
		string over = "?";
		string under = "?";
		string keyText = "?";
		string plain = "?";
		string emptyKey = "?";
		string gridPlace = "?";
		string signPlace = "?";
		string bareGrid = "?";
		string unknownHead = "?";
		string namedHead = "?";
		string keyOffenceHead = "?";
		string bareDesc = "?";
		string clothingDesc = "?";
		string droppedPiece = "?";
		string drivingDesc = "?";
		string placeDesc = "?";
		string keyClothingDesc = "?";
		if (!RedProof())
		{
			none = LF_WantedBoard.Stars(0);
			some = LF_WantedBoard.Stars(3);
			full = LF_WantedBoard.Stars(5);
			over = LF_WantedBoard.Stars(9);
			under = LF_WantedBoard.Stars(-2);
			keyText = LF_WantedBoard.ResolveKey(LF_WantedReports.PLACE_GRID_KEY);
			plain = LF_WantedBoard.ResolveKey("Harbour Pier");
			emptyKey = LF_WantedBoard.ResolveKey("");
			gridPlace = LF_WantedBoard.ResolvePlace(LF_WantedReports.PLACE_GRID_KEY + " 002-001");
			signPlace = LF_WantedBoard.ResolvePlace("Harbour Pier");
			bareGrid = LF_WantedBoard.ResolvePlace(LF_WantedReports.PLACE_GRID_KEY);
			unknownHead = LF_WantedBoard.PosterHeadline("", "Robbery", 3);
			namedHead = LF_WantedBoard.PosterHeadline("Sam Hauler", "Robbery", 5);
			keyOffenceHead = LF_WantedBoard.PosterHeadline("Sam Hauler", "#LF-Wanted_OnFoot", 1);
			bareDesc = LF_WantedBoard.PosterDescription("", "", "");
			clothingDesc = LF_WantedBoard.PosterDescription("red coat|blue jeans", "", "");
			droppedPiece = LF_WantedBoard.PosterDescription("red coat||blue jeans", "", "");
			drivingDesc = LF_WantedBoard.PosterDescription("red coat", "green truck", "");
			placeDesc = LF_WantedBoard.PosterDescription("red coat", "", "Harbour Pier");
			keyClothingDesc = LF_WantedBoard.PosterDescription("#LF-Wanted_OnFoot", "", "");
		}

		ctx.EqualStr("-----", none, "level 0 shows five empty marks");
		ctx.EqualStr("***--", some, "level 3 shows three filled marks and two empty");
		ctx.EqualStr("*****", full, "level 5 shows five filled marks");
		ctx.EqualStr("*****", over, "a level above 5 never shows more than five marks");
		ctx.EqualStr("-----", under, "a negative level shows five empty marks");

		ctx.EqualStr(WidgetManager.Translate(LF_WantedReports.PLACE_GRID_KEY), keyText, "a #key is translated");
		ctx.EqualStr("Harbour Pier", plain, "plain text passes through");
		ctx.EqualStr("", emptyKey, "empty stays empty");
		ctx.EqualStr(WidgetManager.Translate(LF_WantedReports.PLACE_GRID_KEY, "002-001"), gridPlace, "a grid place is built from the grid term and its square");
		ctx.EqualStr("Harbour Pier", signPlace, "a sign title is shown as it is");
		ctx.EqualStr(WidgetManager.Translate(LF_WantedReports.PLACE_GRID_KEY), bareGrid, "a grid key with no square is just the key's own text");

		string level3 = WidgetManager.Translate("#LF-Wanted_Level", 3);
		string level5 = WidgetManager.Translate("#LF-Wanted_Level", 5);
		string level1 = WidgetManager.Translate("#LF-Wanted_Level", 1);
		ctx.EqualStr(WidgetManager.Translate("#LF-Wanted_Unknown") + " - Robbery (" + level3 + ")", unknownHead, "an unidentified suspect reads as Unknown person, with the offence and the level");
		ctx.EqualStr("Sam Hauler - Robbery (" + level5 + ")", namedHead, "an identified suspect is named");
		ctx.EqualStr("Sam Hauler - " + WidgetManager.Translate("#LF-Wanted_OnFoot") + " (" + level1 + ")", keyOffenceHead, "an offence given as a #key is translated");

		string onFoot = WidgetManager.Translate("#LF-Wanted_OnFoot");
		string noDescription = WidgetManager.Translate("#LF-Wanted_NoDescription");
		ctx.EqualStr(WidgetManager.Translate("#LF-Wanted_Wearing", noDescription) + "; " + onFoot, bareDesc, "nothing seen reads 'no description' and on foot");
		ctx.EqualStr(WidgetManager.Translate("#LF-Wanted_Wearing", "red coat, blue jeans") + "; " + onFoot, clothingDesc, "clothing pieces are joined by commas");
		ctx.EqualStr(clothingDesc, droppedPiece, "an empty clothing piece leaves no stray comma");
		ctx.EqualStr(WidgetManager.Translate("#LF-Wanted_Wearing", "red coat") + "; " + WidgetManager.Translate("#LF-Wanted_Driving", "green truck"), drivingDesc, "a known vehicle replaces 'on foot'");
		ctx.EqualStr(WidgetManager.Translate("#LF-Wanted_Wearing", "red coat") + "; " + onFoot + "; " + WidgetManager.Translate("#LF-Wanted_SeenAt", "Harbour Pier"), placeDesc, "a known place ends the line with where the suspect was seen");
		ctx.EqualStr(WidgetManager.Translate("#LF-Wanted_Wearing", onFoot) + "; " + onFoot, keyClothingDesc, "a clothing piece given as a #key is translated");
	}
}
