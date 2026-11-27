// red-proof: with `RedProof()` on the action name is not asked, so every text stays "?" and the fare and seconds checks fail.

//! travel/bus-ticket-cancel-name: the "Cancel trip (refund $X): bus in N s" action a waiting ticket shows
//! (LF_BusTicket.CancelName). It names the refund and the seconds left, and a bus that is already late (negative seconds)
//! never shows a minus sign: the count stops at 0.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_BusTicketCancelName : LF_Test
{
	override string GetName()
	{
		return "travel/bus-ticket-cancel-name";
	}

	override void Run(LF_TestContext ctx)
	{
		string waiting = "?";
		string zero = "?";
		string late = "?";
		string free = "?";
		if (!RedProof())
		{
			waiting = LF_BusTicket.CancelName(250, 30);
			zero = LF_BusTicket.CancelName(250, 0);
			late = LF_BusTicket.CancelName(250, -5);
			free = LF_BusTicket.CancelName(0, 12);
		}

		ctx.EqualStr(LF_Terms.Text("#LF-BusStop_CancelAction", "250", "30"), waiting, "the action names the fare and the seconds left");
		ctx.True(waiting.Contains("250") && waiting.Contains("30"), "both numbers are in the text");
		ctx.EqualStr(LF_Terms.Text("#LF-BusStop_CancelAction", "250", "0"), zero, "no seconds left reads 0");
		ctx.EqualStr(zero, late, "a late bus reads 0 seconds, never a negative count");
		ctx.False(late.Contains("-5"), "no minus sign reaches the screen");
		ctx.EqualStr(LF_Terms.Text("#LF-BusStop_CancelAction", "0", "12"), free, "a free fare shows a zero refund");
	}
}
