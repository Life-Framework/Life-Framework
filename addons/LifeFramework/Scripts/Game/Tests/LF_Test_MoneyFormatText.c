// red-proof: with `RedProof()` on the formatter is not asked, so every text stays "?" and the grouping, sign and
// empty-delta checks fail.

//! money/format-text: how an amount reads on every screen (LF_MoneyFormat). Thousands are grouped by hand from the right
//! (12,500, never 12500), a negative amount puts its minus OUTSIDE the symbol (-$150, not $-150), zero is `$0`, and the HUD
//! delta carries an explicit plus for a gain, a minus for a loss and no text at all for no change. The symbol is the
//! "currency" term's, so every check builds its expected text from `Symbol()` and a pack's pound sign passes too.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MoneyFormatText : LF_Test
{
	override string GetName()
	{
		return "money/format-text";
	}

	override void Run(LF_TestContext ctx)
	{
		string symbol = "?";
		string zero = "?";
		string small = "?";
		string thousand = "?";
		string grouped = "?";
		string millions = "?";
		string negative = "?";
		string negativeThousand = "?";
		string maxInt = "?";
		string gain = "?";
		string loss = "?";
		string noChange = "?";
		string bigGain = "?";
		if (!RedProof())
		{
			symbol = LF_MoneyFormat.Symbol();
			zero = LF_MoneyFormat.FormatMoney(0);
			small = LF_MoneyFormat.FormatMoney(999);
			thousand = LF_MoneyFormat.FormatMoney(1000);
			grouped = LF_MoneyFormat.FormatMoney(12500);
			millions = LF_MoneyFormat.FormatMoney(1234567);
			negative = LF_MoneyFormat.FormatMoney(-150);
			negativeThousand = LF_MoneyFormat.FormatMoney(-1000);
			maxInt = LF_MoneyFormat.FormatMoney(2147483647);
			gain = LF_MoneyFormat.FormatDelta(500);
			loss = LF_MoneyFormat.FormatDelta(-150);
			noChange = LF_MoneyFormat.FormatDelta(0);
			bigGain = LF_MoneyFormat.FormatDelta(1000000);
		}

		ctx.True(symbol != "" && symbol != "?", "the currency symbol is never empty (it falls back to $)");
		ctx.EqualStr(symbol + "0", zero, "zero reads as the symbol and 0");
		ctx.EqualStr(symbol + "999", small, "three digits need no separator");
		ctx.EqualStr(symbol + "1,000", thousand, "a thousand gets its separator");
		ctx.EqualStr(symbol + "12,500", grouped, "12500 reads as 12,500");
		ctx.EqualStr(symbol + "1,234,567", millions, "separators count from the right in every group");
		ctx.EqualStr("-" + symbol + "150", negative, "the minus goes outside the symbol");
		ctx.EqualStr("-" + symbol + "1,000", negativeThousand, "a negative amount is grouped too");
		ctx.EqualStr(symbol + "2,147,483,647", maxInt, "the largest int formats without losing a digit");
		ctx.EqualStr("+" + symbol + "500", gain, "a gain carries a plus");
		ctx.EqualStr("-" + symbol + "150", loss, "a loss carries a minus outside the symbol");
		ctx.EqualStr("", noChange, "no change shows nothing");
		ctx.EqualStr("+" + symbol + "1,000,000", bigGain, "a big gain is grouped behind its plus");
	}
}
