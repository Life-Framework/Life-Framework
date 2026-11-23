// red-proof: new test, red first: with no LF_ExportBoard the file does not compile (every assert fails); the RedProof() hook skips the take, so the "taken" and "known name" assertions fail.
//! The export order board (#4411, WK-W24): one printed line per open order with its count, goods,
//! destination, pay and deadline in US English; a taken order says "taken" until the reader knows the
//! hauler, then shows the name; a closed order's line is gone on the next read.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ExportBoard : LF_Test
{
	protected static const string HAULER = "test-export-board-hauler";
	protected static const ResourceName GOODS = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/export-board";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ExportOrderTemplate Template(string id, string goods, string destination, int count, int wholesale, int minutes)
	{
		LF_ExportOrderTemplate template = new LF_ExportOrderTemplate();
		template.m_sId = id;
		template.m_Goods = GOODS;
		template.m_sGoodsName = goods;
		template.m_sDestination = destination;
		template.m_iCount = count;
		template.m_iWholesale = wholesale;
		template.m_iGameMinutes = minutes;
		return template;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ExportOrderConfig MakeConfig()
	{
		LF_ExportOrderConfig config = LF_ExportOrderConfig.CreateDefault();
		config.m_aTemplates.Insert(Template("iron", "iron bars", "the steamer", 20, 40, 180));
		config.m_aTemplates.Insert(Template("coal", "coal", "the coal freighter", 100, 7, 360));
		config.m_aTemplates.Insert(Template("planks", "planks", "the coal freighter", 120, 6, 360));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Line(array<ref LF_SignRow> rows, int index)
	{
		if (index >= rows.Count())
			return "<no row " + index + ">";

		return rows[index].m_sLabel + " | " + rows[index].m_sValue;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ExportOrderBook.ResetStatics();
		LF_ExportOrderConfig.Reload();
		LF_ExportOrderConfig.UseForTest(MakeConfig());

		// Three orders open at minute 600 (10:00): deadlines 13:00, 16:00 and 16:00.
		if (!RedProof())
			LF_ExportOrderBook.Tick(600);

		LF_BoardAnswer answer = LF_ExportBoard.BuildAnswerFrom(LF_ExportOrderBook.List());
		ctx.Equal(3, LF_ExportBoard.OrderCount(answer), "the board prints one line per open order");
		array<ref LF_SignRow> rows = LF_ExportBoard.RowsFromAnswer(answer);
		ctx.Equal(3, rows.Count(), "three rows for three orders");

		string iron = Line(rows, 0);
		ctx.True(iron.Contains("20 x iron bars"), "the iron line names the count and goods: " + iron);
		ctx.True(iron.Contains("the steamer"), "the iron line names where it goes: " + iron);
		ctx.True(iron.Contains("$920"), "the iron line shows the pay, 20 x 40 plus 15 percent: " + iron);
		ctx.True(iron.Contains("13:00"), "the iron line shows the deadline: " + iron);
		ctx.False(iron.Contains("taken"), "an open order does not say taken: " + iron);
		ctx.True(Line(rows, 1).Contains("100 x coal"), "the second line is the coal order: " + Line(rows, 1));
		ctx.True(Line(rows, 1).Contains("16:00"), "the coal line shows 16:00: " + Line(rows, 1));
		ctx.True(Line(rows, 2).Contains("120 x planks"), "the third line is the planks order: " + Line(rows, 2));

		// A hauler takes the iron order. A reader who does not know them sees "taken", one who does sees the name.
		array<ref LF_ExportOrder> orders = LF_ExportOrderBook.List();
		int ironId = -1;
		if (orders.Count() > 0)
			ironId = orders[0].m_iId;

		if (!RedProof())
			ctx.EqualStr(string.Empty, LF_ExportOrderBook.Take(HAULER, 10, ironId), "a level-10 hauler takes the iron order");

		array<int> takerIds = {4, 0, 0};
		array<string> strangers = {"", "", ""};
		array<string> known = {"Sam Hauler", "", ""};
		answer = LF_ExportBoard.BuildAnswerFrom(LF_ExportOrderBook.List(), takerIds);
		if (answer.m_aNumbers.Count() >= LF_ExportBoard.NUMBERS_PER_ORDER)
		{
			ctx.Equal(1, answer.m_aNumbers[4], "the taken flag crosses for the iron order");
			ctx.Equal(4, answer.m_aNumbers[5], "the taker's player id crosses, never their account id");
		}
		else
		{
			ctx.Fail("the board answer carries no order");
		}

		rows = LF_ExportBoard.RowsFromAnswer(answer, strangers);
		ctx.True(Line(rows, 0).Contains("taken"), "a stranger's order says taken: " + Line(rows, 0));
		ctx.False(Line(rows, 1).Contains("taken"), "the other lines stay open: " + Line(rows, 1));
		rows = LF_ExportBoard.RowsFromAnswer(answer, known);
		ctx.True(Line(rows, 0).Contains("Sam Hauler"), "a known hauler shows by name: " + Line(rows, 0));
		ctx.False(Line(rows, 0).Contains("taken"), "a known name replaces the word taken: " + Line(rows, 0));
		ctx.True(Line(rows, 0).Contains("$920") && Line(rows, 0).Contains("13:00"), "a taken line keeps its pay and deadline: " + Line(rows, 0));

		// Its deadline passes: the line closes and the next two shift up.
		if (!RedProof())
			LF_ExportOrderBook.Tick(781);

		answer = LF_ExportBoard.BuildAnswerFrom(LF_ExportOrderBook.List());
		rows = LF_ExportBoard.RowsFromAnswer(answer);
		ctx.Equal(LF_ExportOrderBook.List().Count(), LF_ExportBoard.OrderCount(answer), "the board prints exactly the orders the book holds");
		bool ironStillPrinted;
		for (int i = 0; i < LF_ExportBoard.OrderCount(answer); i++)
		{
			if (answer.m_aNumbers[i * LF_ExportBoard.NUMBERS_PER_ORDER] == ironId)
				ironStillPrinted = true;
		}

		ctx.False(ironStillPrinted, "the closed order's line is gone");
		ctx.False(ironId < 0, "the iron order existed to close");

		// An empty book prints one honest line, not a blank board; a ragged reply is dropped, not painted.
		rows = LF_ExportBoard.RowsFromAnswer(LF_ExportBoard.BuildAnswerFrom(null));
		ctx.Equal(1, rows.Count(), "an empty book prints one row");
		ctx.True(rows[0].m_sLabel.StartsWith("#"), "the empty row is a string key");
		LF_BoardAnswer ragged = new LF_BoardAnswer();
		ragged.m_aTexts.Insert("coal");
		ragged.m_aNumbers.Insert(7);
		ctx.Equal(0, LF_ExportBoard.OrderCount(ragged), "a ragged reply counts no whole order");

		// The kind is registered and names the take row the reader must stand at.
		LF_BoardProviders.ResetStatics();
		LF_BoardProvider provider = LF_BoardProviders.Find(LF_ExportBoard.KIND);
		ctx.True(provider != null, "the export board kind is registered");
		if (provider)
			ctx.EqualStr(LF_ExportBoard.READ_ACTION, provider.GetReadAction(), "a read must stand at the take rows");

		LF_ExportOrderBook.ResetStatics();
		LF_ExportOrderConfig.Reload();
		LF_ExportBoard.ResetStatics();
	}
}
