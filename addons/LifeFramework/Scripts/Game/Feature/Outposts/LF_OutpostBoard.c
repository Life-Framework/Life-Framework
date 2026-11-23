//------------------------------------------------------------------------------------------------
//! The chalk board beside the quartermaster's crate (0.3.0 gangs O3, #4452, decision N1). A board in the
//! world, no pop-up: its face is a styled chalk sign whose rows come from live state (LF_SignRowSource),
//! one line per stock row of the crate with its name and price, and, for a reader below the crate's top
//! tier, one hint line about what a friend of the camp is sold. The hint names no tier and the board
//! carries no standing number (N1).
//!
//! It is a board kind (LF_BoardProviders): the server builds the answer per read from the crate nearest
//! the reader and that reader's underworld tier, and replies once to that reader; nothing is replicated.
//! Wire shape: m_aTexts holds one name key per stock row; m_aNumbers holds the hint flag (1 or 0) first,
//! then one price per stock row. The reader's tier never crosses, only whether to print the hint.
//! The board prop's Read row asks the face for a fresh answer (no card opens).
//------------------------------------------------------------------------------------------------
class LF_OutpostBoard
{
	static const string FEATURE = "Outposts";
	static const string KIND = "outpostboard";
	static const string SOURCE_KIND = "outpoststock";
	//! The action class a reader must stand at (LF_ActionSite): the Read row on the board prop.
	static const string READ_ACTION = "LF_OutpostBoardReadAction";
	//! The crate's Buy row: the board finds the crate by it.
	static const string CRATE_ACTION = "LF_OutpostStockBuyAction";
	//! How far from the reader the server looks for the crate the board stands beside.
	static const float CRATE_RANGE = 25;	// lf-fixed: the board stands beside its crate; the read range is smaller
	//! Most stock lines one board prints.
	static const int MAX_ROWS = 6;	// lf-fixed: the board's size
	static const string HINT_KEY = "#LF-OutpostBoard_Hint";
	static const string NONE_KEY = "#LF-OutpostBoard_None";

	//------------------------------------------------------------------------------------------------
	//! Pure: the highest tier any stock row needs; the lowest int for an empty crate.
	static int TopTier(array<ref LF_OutpostStockItem> stock)
	{
		int top = -1000;	// lf-fixed: below any tier of the ladder
		if (!stock)
			return top;

		foreach (LF_OutpostStockItem item : stock)
		{
			if (item && item.m_iMinTier > top)
				top = item.m_iMinTier;
		}

		return top;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: true when the board prints the hint line: a reader below the crate's top tier. A reader at
	//! the top tier, or a crate with nothing on it, gets none.
	static bool ShowsHint(array<ref LF_OutpostStockItem> stock, int tier)
	{
		if (!stock || stock.IsEmpty())
			return false;

		return tier < TopTier(stock);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the answer for a crate's rows and a reader at `tier`, capped at MAX_ROWS.
	static LF_BoardAnswer BuildAnswerFrom(array<ref LF_OutpostStockItem> stock, int tier)
	{
		LF_BoardAnswer answer = new LF_BoardAnswer();
		if (ShowsHint(stock, tier))
			answer.m_aNumbers.Insert(1);
		else
			answer.m_aNumbers.Insert(0);

		if (!stock)
			return answer;

		foreach (LF_OutpostStockItem item : stock)
		{
			if (!item)
				continue;

			if (answer.m_aTexts.Count() >= MAX_ROWS)
				break;

			answer.m_aTexts.Insert(item.m_sNameKey);
			answer.m_aNumbers.Insert(item.m_iPrice);
		}

		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the crate nearest `reader`, or null when none stands within CRATE_RANGE.
	static LF_OutpostStockComponent FindCrate(IEntity reader)
	{
		if (!reader)
			return null;

		IEntity site = LF_ActionSite.FindNear(reader.GetOrigin(), CRATE_RANGE, CRATE_ACTION);
		if (!site)
			return null;

		return LF_Component<LF_OutpostStockComponent>.Find(site);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: what the board says to this reader now. With no crate in range it prints the default
	//! stock (and logs why), so the board is never blank.
	static LF_BoardAnswer BuildAnswer(int playerId)
	{
		IEntity reader;
		if (GetGame() && GetGame().GetPlayerManager())
			reader = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);

		array<ref LF_OutpostStockItem> stock;
		LF_OutpostStockComponent crate = FindCrate(reader);
		if (crate)
		{
			stock = crate.GetStock();
		}
		else
		{
			stock = {};
			LF_OutpostStock.DefaultStock(stock);
			LF_Debug.Warn(FEATURE, string.Format("outpost board read by player %1: no crate within %2 m, printing the default stock", playerId, CRATE_RANGE));
		}

		int tier = LF_OutpostStock.TierOf(reader);
		LF_BoardAnswer answer = BuildAnswerFrom(stock, tier);
		LF_Debug.Info(FEATURE, string.Format("outpost board read by player %1: rows=%2 hint=%3", playerId, answer.m_aTexts.Count(), answer.m_aNumbers[0]));
		return answer;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many whole stock lines an answer carries (a ragged reply counts only the full ones).
	static int RowCount(notnull LF_BoardAnswer answer)
	{
		return Math.Min(answer.m_aTexts.Count(), answer.m_aNumbers.Count() - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! "$500", the price as the crate's Buy row writes it.
	static string PriceText(int price)
	{
		return LF_MoneyFormat.FormatMoney(price);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the rows an answer prints: one per stock line (name, price), then the hint line when the
	//! reply asks for it. A crate with nothing on it prints one "nothing today" row.
	static array<ref LF_SignRow> RowsFromAnswer(notnull LF_BoardAnswer answer)
	{
		array<ref LF_SignRow> rows = {};
		int count = RowCount(answer);
		if (count < 0 || (count == 0 && answer.m_aTexts.Count() > 0))
		{
			LF_Debug.Error(FEATURE, string.Format("outpost board reply dropped: %1 texts and %2 numbers do not make a board", answer.m_aTexts.Count(), answer.m_aNumbers.Count()));
			rows.Insert(LF_SignRow.Create(WidgetManager.Translate(NONE_KEY), string.Empty));
			return rows;
		}

		for (int i = 0; i < count; i++)
		{
			rows.Insert(LF_SignRow.Create(WidgetManager.Translate(answer.m_aTexts[i]), PriceText(answer.m_aNumbers[i + 1])));
		}

		if (count == 0)
			rows.Insert(LF_SignRow.Create(WidgetManager.Translate(NONE_KEY), string.Empty));

		if (answer.m_aNumbers[0] != 0)
			rows.Insert(LF_SignRow.Create(WidgetManager.Translate(HINT_KEY), string.Empty));

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for the board face's rows again (a plain class, so it sees the board
	//! bridge in the Signs folder, which loads after this folder's modded classes).
	static void Refresh(SCR_PlayerController controller)
	{
		if (controller)
			controller.LF_AskBoard(KIND, true);
	}
}

//------------------------------------------------------------------------------------------------
class LF_OutpostBoardProvider : LF_BoardProvider
{
	override string GetKind()
	{
		return LF_OutpostBoard.KIND;
	}

	override string GetReadAction()
	{
		return LF_OutpostBoard.READ_ACTION;
	}

	override LF_BoardAnswer Answer(int playerId)
	{
		return LF_OutpostBoard.BuildAnswer(playerId);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_BoardProviderSet
{
	override void Collect(notnull array<ref LF_BoardProvider> providers)
	{
		super.Collect(providers);
		providers.Insert(new LF_OutpostBoardProvider());
	}
}

//------------------------------------------------------------------------------------------------
class LF_OutpostBoardRowSource : LF_SignRowSource
{
	override string GetKind()
	{
		return LF_OutpostBoard.SOURCE_KIND;
	}

	override string GetBoardKind()
	{
		return LF_OutpostBoard.KIND;
	}

	override array<ref LF_SignRow> Rows(notnull LF_BoardAnswer answer)
	{
		return LF_OutpostBoard.RowsFromAnswer(answer);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_SignRowSourceSet
{
	override void Collect(notnull array<ref LF_SignRowSource> sources)
	{
		super.Collect(sources);
		sources.Insert(new LF_OutpostBoardRowSource());
	}
}

//------------------------------------------------------------------------------------------------
//! "Read the board" on the chalk board prop. It runs on the reader's machine and only asks the
//! board's face to refresh; the lines are painted on the board in the world, never in a window.
class LF_OutpostBoardReadAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = WidgetManager.Translate("#LF-OutpostBoard_Read");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_OutpostBoard.FEATURE, "outpost board read not sent: no SCR_PlayerController");
			return;
		}

		LF_OutpostBoard.Refresh(controller);
	}
}
