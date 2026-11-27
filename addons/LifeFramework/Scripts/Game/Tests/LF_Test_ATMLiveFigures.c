// red-proof: with `RedProof()` on no figure or answer is fed to the screen, so the balance and cash stay unknown and the
// refused sends are never tried: the figure, answer and reset checks fail.

//! atm/live-figures: what the cash machine's screen keeps for the local player (LF_ATMLive). Figures are unknown until the
//! server answers; the server's figures replace them each time; a withdrawal or deposit answer carries the new balance and
//! cash whether it was done or refused (the screen shows the server's truth, not a guess); a request the machine would never
//! accept (the balance line, zero, a negative amount, more than one transaction) is not sent and leaves the screen free for
//! the next one instead of stuck "working"; the world-end reset forgets it all.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ATMLiveFigures : LF_Test
{
	override string GetName()
	{
		return "atm/live-figures";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ATMLive.ResetStatics();
		int startBalance = 5;
		int startCash = 5;
		int balanceAfterFigures = -2;
		int cashAfterFigures = -2;
		int balanceAfterDeposit = -2;
		int cashAfterDeposit = -2;
		int balanceAfterRefusal = -2;
		int cashAfterRefusal = -2;
		bool sentBalanceLine = true;
		bool sentZero = true;
		bool sentNegative = true;
		bool sentOverMax = true;
		bool waitingAfterRefusedSends = true;
		bool waitingAfterAnswer = true;
		int balanceAfterReset = 5;
		int cashAfterReset = 5;
		if (!RedProof())
		{
			startBalance = LF_ATMLive.Balance();
			startCash = LF_ATMLive.Cash();
			LF_ATMLive.OnFigures(1200, 80);
			balanceAfterFigures = LF_ATMLive.Balance();
			cashAfterFigures = LF_ATMLive.Cash();

			LF_ATMLive.OnResult(true, LF_EATMRefusal.NONE, 50, 1250, 30);
			balanceAfterDeposit = LF_ATMLive.Balance();
			cashAfterDeposit = LF_ATMLive.Cash();
			waitingAfterAnswer = LF_ATMLive.IsWaiting();

			LF_ATMLive.OnResult(false, LF_EATMRefusal.NOT_ENOUGH_BALANCE, 99999, 1250, 30);
			balanceAfterRefusal = LF_ATMLive.Balance();
			cashAfterRefusal = LF_ATMLive.Cash();

			sentBalanceLine = LF_ATMLive.Send(LF_EATMQuickKind.BALANCE, 100, null);
			sentZero = LF_ATMLive.Send(LF_EATMQuickKind.WITHDRAW, 0, null);
			sentNegative = LF_ATMLive.Send(LF_EATMQuickKind.DEPOSIT, -20, null);
			sentOverMax = LF_ATMLive.Send(LF_EATMQuickKind.WITHDRAW, LF_LedgerConfig.AtmMaxTransaction() + 1, null);
			waitingAfterRefusedSends = LF_ATMLive.IsWaiting();

			LF_ATMLive.ResetStatics();
			balanceAfterReset = LF_ATMLive.Balance();
			cashAfterReset = LF_ATMLive.Cash();
		}

		ctx.Equal(LF_ATMQuick.UNKNOWN, startBalance, "the balance is unknown before the server answers");
		ctx.Equal(LF_ATMQuick.UNKNOWN, startCash, "the cash is unknown before the server answers");
		ctx.Equal(1200, balanceAfterFigures, "the server's balance is kept");
		ctx.Equal(80, cashAfterFigures, "the server's cash is kept");
		ctx.Equal(1250, balanceAfterDeposit, "a deposit answer carries the new balance");
		ctx.Equal(30, cashAfterDeposit, "a deposit answer carries the new cash");
		ctx.False(waitingAfterAnswer, "an answer leaves the machine free");
		ctx.Equal(1250, balanceAfterRefusal, "a refused withdrawal still shows the server's balance");
		ctx.Equal(30, cashAfterRefusal, "a refused withdrawal still shows the server's cash");
		ctx.False(sentBalanceLine, "the balance line is never sent as a transaction");
		ctx.False(sentZero, "a zero amount is not sent");
		ctx.False(sentNegative, "a negative amount is not sent");
		ctx.False(sentOverMax, "more than one transaction is not sent");
		ctx.False(waitingAfterRefusedSends, "a refused send does not leave the screen waiting for a reply that will never come");
		ctx.Equal(LF_ATMQuick.UNKNOWN, balanceAfterReset, "the world-end reset forgets the balance");
		ctx.Equal(LF_ATMQuick.UNKNOWN, cashAfterReset, "the world-end reset forgets the cash");
	}
}
