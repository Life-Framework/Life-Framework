// red-proof: 2026-11-17, cli test --no-build --red: RED PROOF OK on main's path (SetCharacter under the current key): "B's first mark is still B's (expected 500, got 900)", "A's saved list is still unchanged (expected 1, got 4)". Green with the generation check.
// tier: LOGIC
//------------------------------------------------------------------------------------------------
//! map/marks-upload-generation (#2546, map review finding 8): the client uploads its mark list 2 s
//! after an edit. A character switch inside that window used to save the old character's list under
//! the new character's key. The server's restore now carries a generation the client echoes on the
//! upload; a list stamped with an older generation, or issued for another character, is dropped.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapMarksUploadGeneration : LF_Test
{
	protected static const int PLAYER = 992546;
	protected static const string CHAR_A = "test-2546-char-a";
	protected static const string CHAR_B = "test-2546-char-b";

	override string GetName()
	{
		return "map/marks-upload-generation";
	}

	//------------------------------------------------------------------------------------------------
	protected array<ref LF_MapMarkRow> Rows(int count, int x)
	{
		array<ref LF_MapMarkRow> rows = new array<ref LF_MapMarkRow>();
		for (int i = 0; i < count; i++)
		{
			rows.Insert(LF_MapMarkRow.Make("", 0, x + i, x + i, 0, 0, 0, 0, 0, "m"));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! The server's set seam. The red proof takes main's path: the list saved under the current key.
	protected int Set(string characterId, int generation, array<ref LF_MapMarkRow> rows)
	{
		if (RedProof())
			return LF_MapMarks.SetCharacter(characterId, rows);

		return LF_MapMarks.SetFromPlayer(PLAYER, characterId, generation, rows);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Own keys and player id; generations are unique per session, so no store reset is needed.
		LF_MapMarks.SetCharacter(CHAR_A, null);
		LF_MapMarks.SetCharacter(CHAR_B, null);

		// Character A's restore, then A saves one mark under its generation.
		int genA = LF_MapMarks.IssueGeneration(PLAYER, CHAR_A);
		ctx.Equal(1, LF_MapMarks.SetFromPlayer(PLAYER, CHAR_A, genA, Rows(1, 100)), "A's own upload is saved");

		// B already has two saved marks; the player switches to B and the server restores B.
		LF_MapMarks.SetCharacter(CHAR_B, Rows(2, 500));
		int genB = LF_MapMarks.IssueGeneration(PLAYER, CHAR_B);

		ctx.True(genB != genA, "the switch's restore is a new generation");

		// A's upload, queued before the switch, lands now with three marks stamped genA.
		ctx.Equal(-1, Set(CHAR_B, genA, Rows(3, 900)), "a list stamped with the old generation is dropped");
		ctx.Equal(2, LF_MapMarks.CountOf(CHAR_B), "B's saved list is unchanged");
		ctx.Equal(500, LF_MapMarks.MarkOf(CHAR_B, 0).m_iPosX, "B's first mark is still B's");
		ctx.Equal(1, LF_MapMarks.CountOf(CHAR_A), "A's saved list is unchanged");

		// No restore yet, or a restore for another character: dropped too.
		ctx.Equal(-1, LF_MapMarks.SetFromPlayer(PLAYER + 1, CHAR_B, 0, Rows(1, 1)), "a player with no restore saves nothing");
		ctx.Equal(-1, Set(CHAR_A, genB, Rows(4, 1)), "the current generation under another character is dropped");
		ctx.Equal(1, LF_MapMarks.CountOf(CHAR_A), "A's saved list is still unchanged");

		// B's own upload under the new generation is saved.
		ctx.Equal(3, Set(CHAR_B, genB, Rows(3, 700)), "B's upload under the new generation is saved");
		ctx.Equal(700, LF_MapMarks.MarkOf(CHAR_B, 0).m_iPosX, "B's list is the new one");

		LF_MapMarks.SetCharacter(CHAR_A, null);
		LF_MapMarks.SetCharacter(CHAR_B, null);
	}
}
