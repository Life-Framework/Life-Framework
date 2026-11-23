// red-proof: new test, red first: with no LF_OutpostBoard the file does not compile (every assert fails); the RedProof() hook builds the answer for a reader at the top tier only, so the hint assertions fail.
//! outposts/board-lines (#4452, O3, N1): the chalk board beside the crate prints the crate's stock names and
//! prices, one line each; a reader below the crate's top tier gets one hint line that names no tier and no
//! number; a reader at the top tier gets none; the answer never carries the reader's tier.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_OutpostBoardLines : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "outposts/board-lines";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasDigit(string text)
	{
		for (int i = 0; i < text.Length(); i++)
		{
			if ("0123456789".Contains(text.Substring(i, 1)))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_SignRow> RowsFor(notnull array<ref LF_OutpostStockItem> stock, int tier)
	{
		return LF_OutpostBoard.RowsFromAnswer(LF_OutpostBoard.BuildAnswerFrom(stock, tier));
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_OutpostStockItem> stock = {};
		LF_OutpostStock.DefaultStock(stock);
		ctx.Equal(3, stock.Count(), "the default crate stocks three rows");

		int top = LF_OutpostBoard.TopTier(stock);
		ctx.Equal(2, top, "the default crate's top tier is the rifle's");

		// A reader below the top tier: every stock line plus one hint.
		int belowTier = 0;
		if (RedProof())
			belowTier = top;

		array<ref LF_SignRow> rows = RowsFor(stock, belowTier);
		ctx.Equal(stock.Count() + 1, rows.Count(), "a reader below the top tier sees every stock line and one hint");
		if (rows.Count() != stock.Count() + 1)
			return;

		for (int i = 0; i < stock.Count(); i++)
		{
			string name = WidgetManager.Translate(stock[i].m_sNameKey);
			ctx.EqualStr(name, rows[i].m_sLabel, "line " + i + " names the crate's row " + stock[i].m_sId);
			ctx.EqualStr(LF_MoneyFormat.FormatMoney(stock[i].m_iPrice), rows[i].m_sValue, "line " + i + " shows the crate's price for " + stock[i].m_sId);
			ctx.True(rows[i].m_sValue.Contains(stock[i].m_iPrice.ToString().Substring(0, 1)), "the price on line " + i + " is a dollar amount: " + rows[i].m_sValue);
		}

		// The hint is words only: no tier, no number, no standing.
		LF_SignRow hint = rows[stock.Count()];
		string hintText = hint.m_sLabel;
		ctx.EqualStr(WidgetManager.Translate(LF_OutpostBoard.HINT_KEY), hintText, "the last line is the hint");
		ctx.True(hint.m_sValue.IsEmpty(), "the hint has no value column");
		ctx.False(hintText.StartsWith("#"), "the hint resolves to words in the string table: " + hintText);
		ctx.False(HasDigit(hintText), "the hint names no number: " + hintText);
		string lowered = hintText;
		lowered.ToLower();
		ctx.False(lowered.Contains("tier") || lowered.Contains("standing") || lowered.Contains("rank") || lowered.Contains("level"), "the hint names no tier or standing: " + hintText);
		ctx.True(lowered.Contains("friends of the camp"), "the hint speaks of the camp's friends: " + hintText);

		// One tier below the top still gets it; the top tier and above get none.
		ctx.Equal(stock.Count() + 1, RowsFor(stock, top - 1).Count(), "a reader one tier below the top gets the hint");
		ctx.Equal(stock.Count(), RowsFor(stock, top).Count(), "a reader at the top tier gets no hint");
		ctx.Equal(stock.Count(), RowsFor(stock, top + 1).Count(), "a reader above the top tier gets no hint");

		// The wire carries only the hint flag and the prices: the reader's tier never crosses.
		LF_BoardAnswer low = LF_OutpostBoard.BuildAnswerFrom(stock, 0);
		LF_BoardAnswer high = LF_OutpostBoard.BuildAnswerFrom(stock, top);
		ctx.Equal(stock.Count(), low.m_aTexts.Count(), "one name per stock row crosses");
		ctx.Equal(stock.Count() + 1, low.m_aNumbers.Count(), "the hint flag and one price per row cross, nothing else");
		ctx.Equal(1, low.m_aNumbers[0], "a low reader's flag asks for the hint");
		ctx.Equal(0, high.m_aNumbers[0], "a top reader's flag asks for none");
		for (int j = 1; j < low.m_aNumbers.Count(); j++)
		{
			ctx.Equal(low.m_aNumbers[j], high.m_aNumbers[j], "the prices do not depend on the reader (price " + j + ")");
		}

		// Nothing on the shelf prints one honest line; a ragged reply is dropped, not painted.
		array<ref LF_OutpostStockItem> empty = {};
		rows = RowsFor(empty, 0);
		ctx.Equal(1, rows.Count(), "an empty crate prints one row and no hint");
		LF_BoardAnswer ragged = new LF_BoardAnswer();
		ragged.m_aTexts.Insert("#LF-OutpostStock_Pistol");
		ragged.m_aNumbers.Insert(0);
		ctx.Equal(0, LF_OutpostBoard.RowCount(ragged), "a ragged reply counts no whole line");

		// The kind is registered and names the Read row the reader must stand at.
		LF_BoardProviders.ResetStatics();
		LF_BoardProvider provider = LF_BoardProviders.Find(LF_OutpostBoard.KIND);
		ctx.True(provider != null, "the outpost board kind is registered");
		if (provider)
			ctx.EqualStr(LF_OutpostBoard.READ_ACTION, provider.GetReadAction(), "a read must stand at the Read row");

		LF_SignRowSource source = LF_SignRowSources.Find(LF_OutpostBoard.SOURCE_KIND);
		ctx.True(source != null, "the outpost stock row source is registered");
		if (source)
			ctx.EqualStr(LF_OutpostBoard.KIND, source.GetBoardKind(), "the row source reads the outpost board kind");
	}
}
