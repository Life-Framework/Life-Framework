// LF_ATMKeypad.c — the amount on the cash machine's entry line, one key at a time.
// Pure: no widgets, so the suite drives it directly.

class LF_ATMKeypad
{
	//------------------------------------------------------------------------------------------------
	//! Appends one digit. A digit that would pass the single-transaction limit is refused and the
	//! amount stays as it was; a leading zero adds nothing.
	//! \return The new amount.
	static int Press(int amount, int digit)
	{
		if (digit < 0 || digit > 9 || amount < 0)
			return amount;

		if (amount > (EL_ATMManager.MAX_TRANSACTION_AMOUNT - digit) / 10)
			return amount;

		return amount * 10 + digit;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the last digit.
	static int Back(int amount)
	{
		if (amount <= 0)
			return 0;

		return amount / 10;
	}

	//------------------------------------------------------------------------------------------------
	//! The digit a key name stands for (Key0Button .. Key9Button), or -1.
	static int DigitOf(string keyName)
	{
		if (keyName.Length() != 10 || keyName.Substring(0, 3) != "Key" || keyName.Substring(4, 6) != "Button")
			return -1;

		string digit = keyName.Substring(3, 1);
		int value = digit.ToInt();
		if (value == 0 && digit != "0")
			return -1;

		return value;
	}
}
