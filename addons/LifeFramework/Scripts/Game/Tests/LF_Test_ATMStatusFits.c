// red-proof: 2026-11-23, `--only LF_Test_ATMStatusFits` on the unfixed strings: "PLEASE TAKE YOUR CASH: $20" is 26 characters and fails the 22-character limit.
//! atm/status-fits: the ATM screen line is about 25 characters wide, and "PLEASE TAKE YOUR CASH: $20" lost
//! its last digit on it (first-2-hours re-review C8). A payout or deposit line stays short enough for a
//! four-digit amount.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ATMStatusFits : LF_Test
{
	protected const int MAX_CHARS = 22;

	override string GetName()
	{
		return "atm/status-fits";
	}

	override void Run(LF_TestContext ctx)
	{
		string amount = LF_MoneyFormat.FormatMoney(1000);
		string dispensed = WidgetManager.Translate("#LF-ATM_Dispensed", amount);
		string deposited = WidgetManager.Translate("#LF-ATM_Deposited", amount);
		ctx.True(dispensed.Length() <= MAX_CHARS, "the payout line fits the screen: " + dispensed + " (" + dispensed.Length() + ")");
		ctx.True(deposited.Length() <= MAX_CHARS, "the deposit line fits the screen: " + deposited + " (" + deposited.Length() + ")");
		ctx.True(dispensed.Contains(amount), "the payout line still names the amount: " + dispensed);
	}
}
