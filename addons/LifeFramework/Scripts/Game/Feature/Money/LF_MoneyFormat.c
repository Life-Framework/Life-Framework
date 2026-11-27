// Derived from Overthrow (https://github.com/ArmaOverthrow/Overthrow.Arma4), used with the author's permission, relicensed MIT.
// Source: Scripts/Game/Data/OVT_MoneyFormat.c
//------------------------------------------------------------------------------------------------
//! One place that turns an amount of money into text.
//!
//! Every menu in the mod currently concatenates "$" onto a raw ToString(), so 12500 reads as $12500
//! and a player has to count digits to tell it from $1250. Thousands grouping is done by hand rather
//! than by a format string because EnforceScript has no locale-aware number formatter.
//------------------------------------------------------------------------------------------------
class LF_MoneyFormat
{
	//------------------------------------------------------------------------------------------------
	//! Formats an amount as currency with thousands separators.
	//!
	//! The minus sign goes OUTSIDE the currency symbol (-$150, not $-150), which is what every other
	//! readout in the game does.
	//! \param[in] amount Amount in game currency.
	//! \return "$12,500", "$0" or "-$150".
	static string FormatMoney(int amount)
	{
		// The digits are cut from the text, not negated: -amount of the lowest int overflows to itself.
		if (amount < 0)
			return "-" + Symbol() + GroupAmountText(amount.ToString().Substring(1, amount.ToString().Length() - 1));

		return Symbol() + GroupThousands(amount);
	}

	//------------------------------------------------------------------------------------------------
	//! The currency symbol that goes before an amount: the "currency" term's sym form (#958), so a
	//! pack or the owner can set $, a pound sign or a short name. "$" when the term has no words.
	static string Symbol()
	{
		string symbol = LF_Terms.Form("currency", "sym");
		if (symbol.IsEmpty())
			return "$";

		return symbol;
	}

	//------------------------------------------------------------------------------------------------
	//! Formats a change in money as a signed string, for the HUD ticker.
	//! \param[in] delta Change in game currency.
	//! \return "+$500", "-$150", or an empty string when there is no change to show.
	static string FormatDelta(int delta)
	{
		if (delta == 0)
			return "";

		if (delta > 0)
			return "+" + FormatMoney(delta);

		return FormatMoney(delta);
	}

	//------------------------------------------------------------------------------------------------
	//! The amount of a money argument already turned into text, grouped the way FormatMoney groups it
	//! but with no symbol: LF_Terms.Text fills a `{currency:sym}%N` slot with this, so a caller that
	//! passes `paid.ToString()` still shows "1,040" (the crane toast read "$1040" before).
	//! Only plain integer text is touched ("1040", "-150"); anything else comes back unchanged.
	//! \param[in] text The argument as the caller wrote it.
	//! \return "1,040" for "1040", "-1,500" for "-1500", the input for "abc", "12.5" or "".
	static string GroupAmountText(string text)
	{
		if (text.IsEmpty())
			return text;

		string digits = text;
		string sign = "";
		if (text.Get(0) == "-")
		{
			sign = "-";
			digits = text.Substring(1, text.Length() - 1);
		}

		if (digits.IsEmpty() || digits.Length() > 11)
			return text;

		for (int i = 0; i < digits.Length(); i++)
		{
			if (!"0123456789".Contains(digits.Get(i)))
				return text;
		}

		return sign + GroupDigits(digits);
	}

	//------------------------------------------------------------------------------------------------
	//! Inserts a comma every three digits, counting from the right.
	//! \param[in] value A non-negative amount.
	//! \return The digits with separators, e.g. "12,500". "0" for zero.
	protected static string GroupThousands(int value)
	{
		return GroupDigits(value.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! The comma grouping on a string of digits, so an int above 2^24 loses nothing (it is never
	//! turned into a float on the way).
	protected static string GroupDigits(string digits)
	{
		string result = "";
		int length = digits.Length();
		int placed = 0;

		for (int i = length - 1; i >= 0; i--)
		{
			result = digits.Substring(i, 1) + result;
			placed++;

			if (placed % 3 == 0 && i > 0)
				result = "," + result;
		}

		if (result == "")
			return "0";

		return result;
	}
}