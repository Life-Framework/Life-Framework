// red-proof: new test, red first: with no LF_PriceBoardFace / LF_SignRowSources the file does not compile (all asserts fail); the RedProof() hook skips the face-ask note, so the reply opens a card instead of feeding the face.
//! The price board's face (#3299): a market answer of three goods becomes three sign rows in config order
//! (label, price, a down arrow below base), an empty answer prints the "no prices today" row, and a reply
//! to a face ask feeds the sign while a reply to a Read ask stays a card.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PriceBoardFace : LF_Test
{
	override string GetName()
	{
		return "market/price-board-face";
	}

	protected static LF_BoardAnswer ThreeGoods()
	{
		LF_BoardAnswer answer = new LF_BoardAnswer();
		answer.m_aTexts.Insert("Copper Bar");
		answer.m_aNumbers.Insert(38);
		answer.m_aNumbers.Insert(0);
		answer.m_aTexts.Insert("Iron Bar");
		answer.m_aNumbers.Insert(24);
		answer.m_aNumbers.Insert(1);
		answer.m_aTexts.Insert("Steel Bar");
		answer.m_aNumbers.Insert(61);
		answer.m_aNumbers.Insert(0);
		return answer;
	}

	override void Run(LF_TestContext ctx)
	{
		LF_SignRowSources.ResetStatics();

		array<ref LF_SignRow> rows = LF_PriceBoardFace.RowsFromAnswer(ThreeGoods());
		ctx.Equal(3, rows.Count(), "three goods make three rows");
		ctx.EqualStr("Copper Bar", rows[0].m_sLabel, "config order: copper first");
		ctx.EqualStr("38", rows[0].m_sValue, "a good at its base prints the plain price");
		ctx.EqualStr("Iron Bar", rows[1].m_sLabel, "iron second");
		ctx.EqualStr("24 " + LF_PriceBoard.ARROW_DOWN, rows[1].m_sValue, "a good below its base prints the down arrow after the price");
		ctx.EqualStr("Steel Bar", rows[2].m_sLabel, "steel third");

		array<ref LF_SignRow> none = LF_PriceBoardFace.RowsFromAnswer(new LF_BoardAnswer());
		ctx.Equal(1, none.Count(), "an empty answer prints one row");
		ctx.EqualStr("#LF-Market_NoPrices", none[0].m_sLabel, "the row says there are no prices today");
		ctx.True(none[0].m_sValue.IsEmpty(), "the empty row has no value, so no leader dots");

		LF_BoardAnswer partial = new LF_BoardAnswer();
		partial.m_aTexts.Insert("Copper Bar");
		partial.m_aNumbers.Insert(38);
		ctx.EqualStr("#LF-Market_NoPrices", LF_PriceBoardFace.RowsFromAnswer(partial)[0].m_sLabel, "a reply whose numbers do not cover its goods is dropped, not half printed");

		// The reply routing: a face ask feeds the sign's rows; a Read ask stays a card.
		ctx.True(LF_SignRowSources.RowsFor(LF_PriceBoardFace.SOURCE_KIND) == null, "no rows before the first reply");
		int now = 1000;
		if (!RedProof())
			LF_SignRowSources.NoteAsk(LF_PriceBoard.KIND, true);

		ctx.True(LF_SignRowSources.TakeFaceReplyAt(LF_PriceBoard.KIND, ThreeGoods(), now), "a reply to a face ask feeds the face and opens no card");
		array<ref LF_SignRow> stored = LF_SignRowSources.RowsFor(LF_PriceBoardFace.SOURCE_KIND);
		ctx.True(stored != null && stored.Count() == 3, "the face holds the three rows");
		ctx.Equal(1, LF_SignRowSources.VersionFor(LF_PriceBoardFace.SOURCE_KIND), "the version moved so the sign repaints");

		LF_SignRowSources.NoteAsk(LF_PriceBoard.KIND, false);
		ctx.False(LF_SignRowSources.TakeFaceReplyAt(LF_PriceBoard.KIND, ThreeGoods(), now), "a reply to a Read ask is not taken: the card opens");
		ctx.Equal(1, LF_SignRowSources.VersionFor(LF_PriceBoardFace.SOURCE_KIND), "a card read leaves the face alone");
		ctx.False(LF_SignRowSources.TakeFaceReplyAt(LF_PriceBoard.KIND, ThreeGoods(), now), "a reply with no ask is a card (the harness read)");

		// An ask nobody answered (refused, out of range) expires and no longer claims a reply.
		LF_SignRowSources.NoteAsk(LF_PriceBoard.KIND, true);
		ctx.False(LF_SignRowSources.TakeFaceReplyAt(LF_PriceBoard.KIND, ThreeGoods(), System.GetTickCount() + LF_SignRowSources.ASK_EXPIRY_MS + 1000), "a stale face ask is dropped");

		// When a sign asks.
		ctx.True(LF_SignRowSources.ShouldAsk(0, 5000), "never asked: ask");
		ctx.False(LF_SignRowSources.ShouldAsk(5000, 5000 + LF_SignRowSources.REFRESH_MS - 1), "asked a moment ago: wait");
		ctx.True(LF_SignRowSources.ShouldAsk(5000, 5000 + LF_SignRowSources.REFRESH_MS), "a refresh window later: ask again");
		ctx.True(LF_SignRowSources.InAskRange(3.5, 4), "inside the server's read distance: ask");
		ctx.False(LF_SignRowSources.InAskRange(5, 4), "beyond it the server would refuse: do not ask");

		LF_SignRowSources.ResetStatics();
	}
}
