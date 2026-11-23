//------------------------------------------------------------------------------------------------
//! The export order board on the dock warehouse wall (#4411, chapter #4170, WK-W24): a printed chalk
//! board with one line per order in the book (LF_ExportOrderBook, #4410) and one "Take the order" row
//! in the action list per line. No pop-up: the face is a styled sign whose rows come from live state
//! (LF_SignRowSource), so a taken or closed order changes its line on the next refresh.
//!
//! It is a board kind (LF_BoardProviders): the server builds the answer per read from the book and
//! replies once to that reader; nothing is replicated. Wire shape: per order two texts (goods name,
//! destination) in m_aTexts and NUMBERS_PER_ORDER numbers in m_aNumbers (id, count, pay, deadline
//! minute of the day, taken flag, the taker's player id or 0). The taker's name never crosses: a
//! reader sees a name only for a player they were introduced to (LF_KnownNames), else "taken".
//------------------------------------------------------------------------------------------------
class LF_ExportBoard
{
	static const string FEATURE = "ExportOrder";
	static const string KIND = "exportboard";
	static const string SOURCE_KIND = "exportorders";
	//! The action class a reader must stand at (LF_ActionSite): the take rows on the board prop.
	static const string READ_ACTION = "LF_ExportBoardTakeAction";
	static const int NUMBERS_PER_ORDER = 6;	// lf-fixed: wire stride, not a tunable
	static const int TEXTS_PER_ORDER = 2;	// lf-fixed: wire stride, not a tunable
	//! Most orders one board prints; matches the take rows on the prefab.
	static const int MAX_ROWS = 6;	// lf-fixed: the board's size

	// Client: what the last reply said, per row, so a take row shows only for an open order.
	protected static ref array<int> s_aClientOrderIds;
	protected static ref array<bool> s_aClientTaken;
	protected static ref array<string> s_aClientLabels;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aClientOrderIds = null;
		s_aClientTaken = null;
		s_aClientLabels = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the answer for a list of orders, in book order, capped at MAX_ROWS. `takerIds` holds, per
	//! order, the taker's player id (0 when nobody took it or the taker is offline); it may be null.
	static LF_BoardAnswer BuildAnswerFrom(array<ref LF_ExportOrder> orders, array<int> takerIds = null)
	{
		LF_BoardAnswer answer = new LF_BoardAnswer();
		if (!orders)
			return answer;

		for (int i = 0; i < orders.Count() && i < MAX_ROWS; i++)
		{
			LF_ExportOrder order = orders[i];
			answer.m_aTexts.Insert(order.m_sGoodsName);
			answer.m_aTexts.Insert(order.m_sDestination);
			answer.m_aNumbers.Insert(order.m_iId);
			answer.m_aNumbers.Insert(order.m_iCount);
			answer.m_aNumbers.Insert(order.m_iPay);
			answer.m_aNumbers.Insert(order.m_iDeadlineMinute % LF_ExportOrderBook.DAY_MINUTES);
			if (order.IsTaken())
				answer.m_aNumbers.Insert(1);
			else
				answer.m_aNumbers.Insert(0);

			int takerId = 0;
			if (takerIds && i < takerIds.Count())
				takerId = takerIds[i];
			answer.m_aNumbers.Insert(takerId);
		}

		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the book as it is now. The book is ticked first, so an order past its deadline is gone.
	static LF_BoardAnswer BuildAnswer(int playerId)
	{
		LF_ExportOrderBook.Tick(LF_ExportOrderBook.CurrentMinute());
		array<ref LF_ExportOrder> orders = LF_ExportOrderBook.List();
		array<int> takerIds = {};
		foreach (LF_ExportOrder order : orders)
		{
			takerIds.Insert(LF_HandsUp.PlayerIdOfUid(order.m_sTakerUid));
		}

		LF_BoardAnswer answer = BuildAnswerFrom(orders, takerIds);
		LF_Debug.Info(FEATURE, string.Format("export board read by player %1: orders=%2", playerId, answer.m_aTexts.Count() / TEXTS_PER_ORDER));
		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many whole orders an answer carries (a ragged reply counts only the full ones).
	static int OrderCount(notnull LF_BoardAnswer answer)
	{
		int byNumbers = answer.m_aNumbers.Count() / NUMBERS_PER_ORDER;
		int byTexts = answer.m_aTexts.Count() / TEXTS_PER_ORDER;
		return Math.Min(byNumbers, byTexts);
	}

	//------------------------------------------------------------------------------------------------
	//! "20 x iron bars to the steamer".
	static string LineLabel(int count, string goods, string destination)
	{
		return WidgetManager.Translate("#LF-ExportBoard_Label", count.ToString(), LF_WantedBoard.ResolveKey(goods), LF_WantedBoard.ResolveKey(destination));
	}

	//------------------------------------------------------------------------------------------------
	//! "$920 by 13:00", and for a taken order " - Sam Hauler" or " - taken" when the reader has not
	//! been told the hauler's name.
	static string LineValue(int pay, string deadline, bool taken, string knownName)
	{
		if (!taken)
			return LF_Terms.Text("#LF-ExportBoard_Value", pay.ToString(), deadline);

		string who = knownName;
		if (who.IsEmpty())
			who = WidgetManager.Translate("#LF-ExportBoard_Taken");

		return LF_Terms.Text("#LF-ExportBoard_ValueTaken", pay.ToString(), deadline, who);
	}

	//------------------------------------------------------------------------------------------------
	//! "13:00" from a minute of the day.
	static string DeadlineText(int minuteOfDay)
	{
		int ofDay = minuteOfDay % LF_ExportOrderBook.DAY_MINUTES;
		int hour = ofDay / 60;
		int minute = ofDay % 60;
		return string.Format("%1:%2", hour.ToString(2), minute.ToString(2));
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the rows an answer prints, one per order. `takerNames[i]` is the name the reader knows the
	//! taker of order i by, or empty. An empty book prints one "no orders" row.
	static array<ref LF_SignRow> RowsFromAnswer(notnull LF_BoardAnswer answer, array<string> takerNames = null)
	{
		array<ref LF_SignRow> rows = {};
		int count = OrderCount(answer);
		if (count == 0)
		{
			if (answer.m_aNumbers.Count() > 0 || answer.m_aTexts.Count() > 0)
				LF_Debug.Error(FEATURE, string.Format("export board reply dropped: %1 texts and %2 numbers do not make one order", answer.m_aTexts.Count(), answer.m_aNumbers.Count()));

			rows.Insert(LF_SignRow.Create("#LF-ExportBoard_None", string.Empty));
			return rows;
		}

		for (int i = 0; i < count; i++)
		{
			int at = i * NUMBERS_PER_ORDER;
			string name = string.Empty;
			if (takerNames && i < takerNames.Count())
				name = takerNames[i];

			string label = LineLabel(answer.m_aNumbers[at + 1], answer.m_aTexts[i * TEXTS_PER_ORDER], answer.m_aTexts[i * TEXTS_PER_ORDER + 1]);
			string value = LineValue(answer.m_aNumbers[at + 2], DeadlineText(answer.m_aNumbers[at + 3]), answer.m_aNumbers[at + 4] != 0, name);
			rows.Insert(LF_SignRow.Create(label, value));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the names this reader knows the takers by, from the introductions they were told.
	static array<string> TakerNames(notnull LF_BoardAnswer answer)
	{
		array<string> names = {};
		int count = OrderCount(answer);
		for (int i = 0; i < count; i++)
		{
			names.Insert(LF_KnownNames.NameOf(answer.m_aNumbers[i * NUMBERS_PER_ORDER + 5]));
		}

		return names;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: remember each row's order id, taken flag and label, for the take rows.
	static void RememberRows(notnull LF_BoardAnswer answer)
	{
		s_aClientOrderIds = {};
		s_aClientTaken = {};
		s_aClientLabels = {};
		int count = OrderCount(answer);
		for (int i = 0; i < count; i++)
		{
			s_aClientOrderIds.Insert(answer.m_aNumbers[i * NUMBERS_PER_ORDER]);
			s_aClientTaken.Insert(answer.m_aNumbers[i * NUMBERS_PER_ORDER + 4] != 0);
			s_aClientLabels.Insert(LineLabel(answer.m_aNumbers[i * NUMBERS_PER_ORDER + 1], answer.m_aTexts[i * TEXTS_PER_ORDER], answer.m_aTexts[i * TEXTS_PER_ORDER + 1]));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the order id the painted row `slot` shows, or -1 when the row is empty or taken.
	static int OpenOrderIdAt(int slot)
	{
		if (!s_aClientOrderIds || slot < 0 || slot >= s_aClientOrderIds.Count() || s_aClientTaken[slot])
			return -1;

		return s_aClientOrderIds[slot];
	}

	//------------------------------------------------------------------------------------------------
	static string LabelAt(int slot)
	{
		if (!s_aClientLabels || slot < 0 || slot >= s_aClientLabels.Count())
			return string.Empty;

		return s_aClientLabels[slot];
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ExportBoard's statics (LF_StaticReset finds it by reflection).
class LF_ExportBoard_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ExportBoard";
	}

	override void Apply()
	{
		LF_ExportBoard.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
class LF_ExportBoardProvider : LF_BoardProvider
{
	override string GetKind()
	{
		return LF_ExportBoard.KIND;
	}

	override string GetReadAction()
	{
		return LF_ExportBoard.READ_ACTION;
	}

	override LF_BoardAnswer Answer(int playerId)
	{
		return LF_ExportBoard.BuildAnswer(playerId);
	}

	override void OnOpened(notnull LF_BoardAnswer answer)
	{
		LF_Debug.Info(LF_ExportBoard.FEATURE, string.Format("export board card: %1 order(s)", LF_ExportBoard.OrderCount(answer)));
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_BoardProviderSet
{
	override void Collect(notnull array<ref LF_BoardProvider> providers)
	{
		super.Collect(providers);
		providers.Insert(new LF_ExportBoardProvider());
	}
}

//------------------------------------------------------------------------------------------------
class LF_ExportBoardRowSource : LF_SignRowSource
{
	override string GetKind()
	{
		return LF_ExportBoard.SOURCE_KIND;
	}

	override string GetBoardKind()
	{
		return LF_ExportBoard.KIND;
	}

	override array<ref LF_SignRow> Rows(notnull LF_BoardAnswer answer)
	{
		LF_ExportBoard.RememberRows(answer);
		return LF_ExportBoard.RowsFromAnswer(answer, LF_ExportBoard.TakerNames(answer));
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_SignRowSourceSet
{
	override void Collect(notnull array<ref LF_SignRowSource> sources)
	{
		super.Collect(sources);
		sources.Insert(new LF_ExportBoardRowSource());
	}
}
