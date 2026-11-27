// red-proof: 2026-11-23, `--only LF_Test_ATMStatusFits` on the unfixed strings: "PLEASE TAKE YOUR CASH: $20" is 26 characters and fails the 22-character limit.
// red-proof: 2026-11-27 (UI sweep): on the face the status line holds 17 characters, not 22: the picture of the screen after a $20 withdrawal read "TAKE YOUR CASH: $2". With the old strings the run fails the payout line (19 characters), the deposit line (13 plus the amount) and every "REFUSED: ..." line; `--red` puts a long line through the same check.
//! atm/status-fits: the ATM screen line is about 17 characters wide ("TAKE YOUR CASH: $20" lost its last digit on the
//! machine in the 2026-11-27 sweep; the first fit, 22, was measured on a longer line). A payout, a deposit, a refusal or
//! the bank not answering stays short enough for a four-digit amount.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ATMStatusFits : LF_Test
{
	protected const int MAX_CHARS = 17;

	override string GetName()
	{
		return "atm/status-fits";
	}

	//------------------------------------------------------------------------------------------------
	protected void Fits(LF_TestContext ctx, string label, string line)
	{
		if (RedProof())
			line = "REFUSED: YOUR HANDS ARE NOT FREE";

		ctx.True(line.Length() <= MAX_CHARS, string.Format("%1 fits the screen: '%2' (%3 characters, limit %4)", label, line, line.Length(), MAX_CHARS));
		ctx.False(line.IsEmpty() || line.StartsWith("#"), string.Format("%1 is words: '%2'", label, line));
	}

	override void Run(LF_TestContext ctx)
	{
		string amount = LF_MoneyFormat.FormatMoney(1000);
		string dispensed = WidgetManager.Translate("#LF-ATM_Dispensed", amount);
		string deposited = WidgetManager.Translate("#LF-ATM_Deposited", amount);
		Fits(ctx, "the payout line", dispensed);
		Fits(ctx, "the deposit line", deposited);
		ctx.True(dispensed.Contains(amount), "the payout line still names the amount: " + dispensed);
		ctx.True(deposited.Contains(amount), "the deposit line still names the amount: " + deposited);
		Fits(ctx, "the working line", WidgetManager.Translate("#LF-ATM_Working"));
		Fits(ctx, "the no-reply line", WidgetManager.Translate("#LF-ATM_NoReply"));
		Fits(ctx, "the hello line", WidgetManager.Translate("#LF-ATM_ScreenHello"));

		// Every refusal the server can send prints on the same line (the systems-off key is shared with other screens).
		for (int reason = LF_EATMRefusal.INVALID_AMOUNT; reason <= LF_EATMRefusal.CANNOT_ACT; reason++)
		{
			if (reason == LF_EATMRefusal.SYSTEM_OFF)
				continue;

			string key = LF_ATMRefusal.Key(reason);
			Fits(ctx, "refusal " + reason.ToString(), WidgetManager.Translate(key));
		}
	}
}
