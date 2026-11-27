// red-proof: 2026-11-27 `RedProof()` on skips the formatter and the text path, so every text stays "?" and the edge,
// large-int, argument-grouping and crane-toast checks fail.

//! money/format-edges: the edges of one amount's text (#money-format, playtest 2026-11-27 round 4: the crane toast read
//! "$1040" beside the HUD's "+$1,040"). Zero, 999 and 1000, a negative, the lowest int, ints above 2^24 that came out of
//! LF_Utils int-safe math (a float would round 1800000480 to 1800000512), `GroupAmountText` leaving anything that is
//! not plain integer text alone, and `LF_Terms.Text` grouping the slot a row writes after `{currency:sym}` so a caller
//! that passes `paid.ToString()` still reads "$1,040". The crane's short-count toast names the missing units.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MoneyFormatEdges : LF_Test
{
	override string GetName()
	{
		return "money/format-edges";
	}

	override void Run(LF_TestContext ctx)
	{
		string symbol = "?";
		string zero = "?";
		string edgeLow = "?";
		string edgeHigh = "?";
		string aboveFloat = "?";
		string unix = "?";
		string clamped = "?";
		string lowest = "?";
		string negativeBig = "?";
		string deltaBig = "?";
		string textThousand = "?";
		string textNegative = "?";
		string textZero = "?";
		string textEmpty = "?";
		string textWord = "?";
		string textDecimal = "?";
		string textSignOnly = "?";
		string textPlus = "?";
		string textHuge = "?";
		string paidToast = "?";
		string paidSmall = "?";
		string fare = "?";
		string cancel = "?";
		string wordArg = "?";
		string shortToast = "?";
		string keyShort = "?";
		string keyNoCount = "?";
		string keyOther = "?";
		string argShort = "?";
		string argOther = "?";

		int unixSeconds = 1800000480;
		int floatLimit = 16777217;
		int lowestInt = -2147483647;
		lowestInt = lowestInt - 1;

		if (!RedProof())
		{
			symbol = LF_MoneyFormat.Symbol();
			zero = LF_MoneyFormat.FormatMoney(0);
			edgeLow = LF_MoneyFormat.FormatMoney(999);
			edgeHigh = LF_MoneyFormat.FormatMoney(1000);
			aboveFloat = LF_MoneyFormat.FormatMoney(floatLimit);
			unix = LF_MoneyFormat.FormatMoney(LF_Utils.MaxInt(unixSeconds, 0));
			clamped = LF_MoneyFormat.FormatMoney(LF_Utils.ClampInt(unixSeconds + 1, 0, 1800000999));
			lowest = LF_MoneyFormat.FormatMoney(lowestInt);
			negativeBig = LF_MoneyFormat.FormatMoney(LF_Utils.MinInt(-unixSeconds, 0));
			deltaBig = LF_MoneyFormat.FormatDelta(floatLimit);
			textThousand = LF_MoneyFormat.GroupAmountText("1040");
			textNegative = LF_MoneyFormat.GroupAmountText("-1500");
			textZero = LF_MoneyFormat.GroupAmountText("0");
			textEmpty = LF_MoneyFormat.GroupAmountText("");
			textWord = LF_MoneyFormat.GroupAmountText("Quay 1500");
			textDecimal = LF_MoneyFormat.GroupAmountText("12.5");
			textSignOnly = LF_MoneyFormat.GroupAmountText("-");
			textPlus = LF_MoneyFormat.GroupAmountText("+1500");
			textHuge = LF_MoneyFormat.GroupAmountText("123456789012");
			paidToast = LF_Terms.Text("#LF-ExportCrane_Paid", "1040");
			paidSmall = LF_Terms.Text("#LF-ExportCrane_Paid", "999");
			fare = LF_Terms.Text("#LF-BusStop_ActionTo", "Quay", "1500", "1200");
			cancel = LF_Terms.Text("#LF-BusStop_CancelAction", "2500", "30");
			wordArg = LF_Terms.Text("#LF-BusStop_ActionTo", "Quay 1500", "1500", "30");
			shortToast = LF_Terms.Text(LF_ExportCraneHandIn.ToastKey(LF_ExportOrderBook.REFUSE_SHORT, 3), LF_ExportCraneHandIn.ToastArg(LF_ExportOrderBook.REFUSE_SHORT, 3));
			keyShort = LF_ExportCraneHandIn.ToastKey(LF_ExportOrderBook.REFUSE_SHORT, 3);
			keyNoCount = LF_ExportCraneHandIn.ToastKey(LF_ExportOrderBook.REFUSE_SHORT, 0);
			keyOther = LF_ExportCraneHandIn.ToastKey(LF_ExportOrderBook.REFUSE_NONE_HELD, 3);
			argShort = LF_ExportCraneHandIn.ToastArg(LF_ExportOrderBook.REFUSE_SHORT, 12);
			argOther = LF_ExportCraneHandIn.ToastArg(LF_ExportOrderBook.REFUSE_GONE, 12);
		}

		// The edges of the formatter (money/format-text has the usual cases).
		ctx.EqualStr(symbol + "0", zero, "zero reads as the symbol and 0");
		ctx.EqualStr(symbol + "999", edgeLow, "999 has no separator");
		ctx.EqualStr(symbol + "1,000", edgeHigh, "1000 gets one");

		// Ints above 2^24: nothing goes through a float on the way.
		ctx.EqualStr(symbol + "16,777,217", aboveFloat, "2^24 + 1 keeps its last digit");
		ctx.EqualStr(symbol + "1,800,000,480", unix, "a unix-sized amount through LF_Utils.MaxInt keeps its low digits");
		ctx.EqualStr(symbol + "1,800,000,481", clamped, "LF_Utils.ClampInt keeps the exact value");
		ctx.EqualStr("-" + symbol + "2,147,483,648", lowest, "the lowest int formats without overflowing on its own sign");
		ctx.EqualStr("-" + symbol + "1,800,000,480", negativeBig, "a large debt through LF_Utils.MinInt reads with the minus outside");
		ctx.EqualStr("+" + symbol + "16,777,217", deltaBig, "a large gain keeps its plus and its digits");

		// GroupAmountText: plain integer text is grouped, everything else is left as the caller wrote it.
		ctx.EqualStr("1,040", textThousand, "1040 reads 1,040");
		ctx.EqualStr("-1,500", textNegative, "a negative integer text keeps its minus in front");
		ctx.EqualStr("0", textZero, "zero stays 0");
		ctx.EqualStr("", textEmpty, "empty text stays empty");
		ctx.EqualStr("Quay 1500", textWord, "text with a word in it is left alone");
		ctx.EqualStr("12.5", textDecimal, "a decimal is left alone");
		ctx.EqualStr("-", textSignOnly, "a lone minus is left alone");
		ctx.EqualStr("+1500", textPlus, "a leading plus is left alone");
		ctx.EqualStr("123456789012", textHuge, "text longer than an int is left alone");

		// LF_Terms.Text: a slot written after {currency:sym} is money; the others are not.
		ctx.True(paidToast.Contains(symbol + "1,040"), "the crane toast reads the HUD's amount: " + paidToast);
		ctx.False(paidToast.Contains("1040"), "the crane toast has no ungrouped digits: " + paidToast);
		ctx.True(paidSmall.Contains(symbol + "999"), "a small amount is unchanged: " + paidSmall);
		ctx.True(fare.Contains(symbol + "1,500"), "the fare slot is grouped: " + fare);
		ctx.True(fare.Contains("1200"), "a seconds slot beside it is not: " + fare);
		ctx.True(cancel.Contains(symbol + "2,500"), "the refund slot is grouped: " + cancel);
		ctx.True(wordArg.Contains("Quay 1500"), "a label slot is never touched: " + wordArg);

		// The crane's short count names the units still missing.
		ctx.EqualStr("#LF-ExportOrder_RefuseShortCount", keyShort, "a short count with units missing uses the counted row");
		ctx.EqualStr(LF_ExportOrderBook.REFUSE_SHORT, keyNoCount, "a short with no count keeps the plain row");
		ctx.EqualStr(LF_ExportOrderBook.REFUSE_NONE_HELD, keyOther, "another refusal keeps its own row");
		ctx.EqualStr("12", argShort, "the counted row gets the missing units as %1");
		ctx.EqualStr("", argOther, "another refusal passes no argument");
		ctx.True(shortToast.Contains("3 more unit(s)"), "the toast says how many units are missing: " + shortToast);
	}
}
