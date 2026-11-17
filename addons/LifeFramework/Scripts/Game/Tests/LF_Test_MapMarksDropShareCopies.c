// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapMarksDropShareCopies: RED PROOF OK with the share Put skipped, "the restore sends only the mark that is not a share copy (expected 1, got 3)". Green with DropShareCopies in the restore.
// tier: WORLD
//------------------------------------------------------------------------------------------------
//! map/marks-drop-share-copies (#2623, #2495 finding 9): before the review fix shared marks were uploaded
//! as personal marks, so saved "mapmarks" rows hold copies of org and dispatch marks. The spawn restore
//! (LF_MapMarks.ExportCharacter) drops each saved mark that matches a live share the character sees
//! (same text, within 1 m) and keeps the cleaned list in the store; with no live share every mark stays.
//! A fixture org member and a recording sink stand in for the real players.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_MapMarksDropShareCopies : LF_Test
{
	protected static const string CHARACTER = "mapmarks-share-copies-character";
	protected static const string ORG = "mapmarks-share-copies-org";
	protected static const string SHARE_KEY = "org:mapmarks-share-copies-org:1";
	protected static const string TEXT = "Meet here";
	protected static const int PLAYER = 26230;
	protected static const int X = 1000;
	protected static const int Z = 2000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/marks-drop-share-copies";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_MapMarkRow Mark(int x, int z, string text)
	{
		return LF_MapMarkRow.Make("", SCR_EMapMarkerType.PLACED_CUSTOM, x, z, 0, 0, 0, -1, 0, text);
	}

	//------------------------------------------------------------------------------------------------
	//! The saved list: a copy at the share, a copy 1 m off, and the player's own mark at the same spot.
	protected static void SaveList()
	{
		array<ref LF_MapMarkRow> rows = {};
		rows.Insert(Mark(X, Z, TEXT));
		rows.Insert(Mark(X + 1, Z, TEXT));
		rows.Insert(Mark(X, Z, "Home"));
		LF_MapMarks.SetCharacter(CHARACTER, rows);
	}

	//------------------------------------------------------------------------------------------------
	protected static int Restore()
	{
		array<int> ints = {};
		array<string> texts = {};
		return LF_MapMarks.ExportCharacter(CHARACTER, ints, texts);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapShares.ResetStatics();
		LF_Test_MapShareFacts facts = new LF_Test_MapShareFacts();
		facts.Set(PLAYER, CHARACTER, "", ORG);
		LF_MapShares.SetFacts(facts);
		LF_MapShares.SetSink(new LF_Test_MapShareSink());

		// A live org share at the spot: both copies go, the player's own mark stays.
		LF_MapShareRow share = LF_MapShareRow.Make(SHARE_KEY, LF_EMapShareAudience.ORG, ORG, X, Z, 0, 0, TEXT, 0);
		if (!RedProof())
			ctx.True(LF_MapShares.Put(share), "the org share is live");

		SaveList();
		ctx.Equal(1, Restore(), "the restore sends only the mark that is not a share copy");
		ctx.Equal(1, LF_MapMarks.CountOf(CHARACTER), "the store keeps the cleaned list for the next save");
		LF_MapMarkRow kept = LF_MapMarks.MarkOf(CHARACTER, 0);
		if (kept)
			ctx.EqualStr("Home", kept.m_sText, "the player's own mark is the one kept");

		// The same text 3 m away is the player's own mark, not a copy.
		array<ref LF_MapMarkRow> far = {};
		far.Insert(Mark(X + 3, Z, TEXT));
		LF_MapMarks.SetCharacter(CHARACTER, far);
		ctx.Equal(1, Restore(), "a mark with the share's text 3 m away stays");

		// No live share (expired or removed): nothing can be matched, all three stay.
		LF_MapShares.Remove(SHARE_KEY);
		SaveList();
		ctx.Equal(3, Restore(), "with no live share every saved mark stays");
		ctx.Equal(3, LF_MapMarks.CountOf(CHARACTER), "with no live share the store is unchanged");

		LF_MapMarks.SetCharacter(CHARACTER, null);
		LF_MapShares.ResetStatics();
		LF_MapShares.EnsureStarted();
	}
}
