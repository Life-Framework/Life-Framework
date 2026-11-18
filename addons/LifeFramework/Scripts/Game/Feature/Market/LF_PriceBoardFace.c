// LF_PriceBoardFace.c: the trader chalk board prints today's market prices on its face (#3299).
//
// The face is a styled sign (chalk, docs/architecture/sign-system.md) whose rows come from the market
// book. The row source "market" reads the same board answer as the Read card (LF_PriceBoard, kind
// "prices"): one row per curved good in config order, label and today's price, with a down arrow after
// the price when it stands below its base; an empty answer prints one "no prices today" row.

//------------------------------------------------------------------------------------------------
class LF_PriceBoardFace
{
	static const string SOURCE_KIND = "market";

	//------------------------------------------------------------------------------------------------
	//! The rows an answer prints. A reply whose numbers do not cover its goods is dropped whole and
	//! prints the empty row (logged), the same rule as the card.
	static array<ref LF_SignRow> RowsFromAnswer(notnull LF_BoardAnswer answer)
	{
		array<ref LF_SignRow> rows = {};
		int goods = answer.m_aTexts.Count();
		if (answer.m_aNumbers.Count() < goods * LF_PriceBoard.NUMBERS_PER_GOOD)
		{
			LF_Debug.Error(LF_PriceBoard.FEATURE, string.Format("price face reply dropped: %1 goods need %2 numbers, got %3", goods, goods * LF_PriceBoard.NUMBERS_PER_GOOD, answer.m_aNumbers.Count()));
			goods = 0;
		}

		if (goods == 0)
		{
			rows.Insert(LF_SignRow.Create("#LF-Market_NoPrices", string.Empty));
			return rows;
		}

		for (int i = 0; i < goods; i++)
		{
			int price = answer.m_aNumbers[i * LF_PriceBoard.NUMBERS_PER_GOOD];
			int belowBase = answer.m_aNumbers[i * LF_PriceBoard.NUMBERS_PER_GOOD + 1];
			rows.Insert(LF_SignRow.Create(LF_WantedBoard.ResolveKey(answer.m_aTexts[i]), PriceValue(price, belowBase)));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! "38", or "32 v" when the price is below its base.
	static string PriceValue(int price, int belowBase)
	{
		if (belowBase != 0)
			return price.ToString() + " " + LF_PriceBoard.ARROW_DOWN;

		return price.ToString();
	}
}

//------------------------------------------------------------------------------------------------
class LF_PriceBoardRowSource : LF_SignRowSource
{
	override string GetKind()
	{
		return LF_PriceBoardFace.SOURCE_KIND;
	}

	override string GetBoardKind()
	{
		return LF_PriceBoard.KIND;
	}

	override array<ref LF_SignRow> Rows(notnull LF_BoardAnswer answer)
	{
		return LF_PriceBoardFace.RowsFromAnswer(answer);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_SignRowSourceSet
{
	override void Collect(notnull array<ref LF_SignRowSource> sources)
	{
		super.Collect(sources);
		sources.Insert(new LF_PriceBoardRowSource());
	}
}
