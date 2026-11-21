// red-proof: observed 2026-11-19 `tools/cli test --no-build --red LF_Test_ATMTransferPage` printed `RED PROOF OK (all 1 listed test(s) fail with the hook on)`: RedProof() keys in a number with a stray character, so the transfer is refused and the balances do not move; `--only` then passed=1 failed=0. Review fix 2026-11-19: the SenderReply asserts failed on a stub that always allowed (`a cuffed sender is refused (expected 10, got 0)`, downed and dead the same), then passed with the guard. Double-send fix 2026-11-21: `--red LF_Test_ATMTransferPage` RED PROOF OK ("the same transfer 6 s after a sent one is a duplicate" fails with NoteSent skipped), `--only` passed=1 failed=0.
//! The ATM transfer page's server side (#3320, slice of #638). A transfer request names an account number
//! and an amount; the server trims the number, makes its own transfer id, runs the #3318 rules again and
//! moves the money once. A bad number, your own number, a zero amount and an over-long number are refused
//! in words; one player's requests are rate limited; the toast names the last four characters.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ATMTransferPage : LF_Test
{
	protected static const string SENDER = "test-atp-sender";
	protected static const string RECIPIENT = "test-atp-1234";

	override string GetName()
	{
		return "atm/transfer-page";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_BankAccountRecord> bankBefore = LF_ATMManager.ExportAll();
		LF_ATMManager.Reset();
		LF_BankTransfer.ResetStatics();
		LF_ATMTransferService.ResetStatics();

		LF_BankAccount sender = LF_ATMManager.GetOrCreate(SENDER);
		LF_BankAccount recipient = LF_ATMManager.GetOrCreate(RECIPIENT);
		sender.SetBalance(1000);
		recipient.SetBalance(10);

		// The number is trimmed on the server before the rules see it.
		string padded = "  " + RECIPIENT + " ";
		if (RedProof())
			padded = "  " + RECIPIENT + "x ";

		int sent;
		int fee;
		LF_EATMTransferReply reply = LF_ATMTransferService.Serve(SENDER, padded, 50, sent, fee);
		ctx.Equal(LF_EATMTransferReply.SENT, reply, "a padded number still reaches the account");
		ctx.Equal(50, sent, "the reply carries the amount that moved");
		ctx.Equal(60, recipient.GetBalance(), "the recipient's balance rises by the amount");
		ctx.Equal(1000 - 50 - fee, sender.GetBalance(), "the sender pays the amount and the fee");
		ctx.EqualStr("1234", LF_ATMTransferService.LastFour(LF_ATMTransferService.CleanNumber(padded)), "the toast names the last four characters");

		// The id is the server's: the same request twice is two transfers, not a replay.
		reply = LF_ATMTransferService.Serve(SENDER, RECIPIENT, 50, sent, fee);
		ctx.Equal(LF_EATMTransferReply.SENT, reply, "a second identical request is a new transfer");
		ctx.Equal(110, recipient.GetBalance(), "both transfers landed");

		// Refusals, each in words, and nothing moves.
		int balanceBefore = sender.GetBalance();
		ctx.Equal(LF_EATMTransferReply.UNKNOWN_ACCOUNT, LF_ATMTransferService.Serve(SENDER, "no-such-number", 50, sent, fee), "an unknown number is refused");
		ctx.Equal(LF_EATMTransferReply.UNKNOWN_ACCOUNT, LF_ATMTransferService.Serve(SENDER, "   ", 50, sent, fee), "a blank number is refused");
		string longNumber = "0123456789012345678901234567890123456789012345678901234567890123456789";
		ctx.Equal(LF_EATMTransferReply.UNKNOWN_ACCOUNT, LF_ATMTransferService.Serve(SENDER, longNumber, 50, sent, fee), "an over-long number is refused");
		ctx.Equal(LF_EATMTransferReply.OWN_ACCOUNT, LF_ATMTransferService.Serve(SENDER, " " + SENDER, 50, sent, fee), "your own number is refused, padded or not");
		ctx.Equal(LF_EATMTransferReply.INVALID_AMOUNT, LF_ATMTransferService.Serve(SENDER, RECIPIENT, 0, sent, fee), "a zero amount is refused");
		ctx.Equal(LF_EATMTransferReply.INVALID_AMOUNT, LF_ATMTransferService.Serve(SENDER, RECIPIENT, -5, sent, fee), "a negative amount is refused");
		ctx.Equal(LF_EATMTransferReply.NOT_ENOUGH_BALANCE, LF_ATMTransferService.Serve(SENDER, RECIPIENT, 5000, sent, fee), "an amount past the balance is refused");
		ctx.Equal(LF_EATMTransferReply.NO_ACCOUNT, LF_ATMTransferService.Serve("", RECIPIENT, 50, sent, fee), "no sender account is refused");
		ctx.Equal(balanceBefore, sender.GetBalance(), "no refusal moved money");
		ctx.Equal(110, recipient.GetBalance(), "no refusal paid the recipient");

		// Every reply but SENT has words; SENT has none; a wire value out of range reads as failed.
		for (int r = LF_EATMTransferReply.DISABLED; r <= LF_EATMTransferReply.FAILED; r++)
		{
			ctx.False(LF_ATMTransferService.Key(r).IsEmpty(), "reply " + r.ToString() + " has a screen line");
		}

		ctx.True(LF_ATMTransferService.Key(LF_EATMTransferReply.SENT).IsEmpty(), "a sent transfer has no refusal line");
		ctx.Equal(LF_EATMTransferReply.FAILED, LF_ATMTransferService.FromWire(99), "an unknown wire value reads as failed");
		ctx.EqualStr("ab", LF_ATMTransferService.LastFour("ab"), "a number shorter than four is shown whole");

		// A sender who cannot act is refused before the rules: a cuffed player cannot empty the account
		// to a friend before the custody fine, a downed or dead one cannot send from the ground.
		ctx.Equal(LF_EATMTransferReply.SENT, LF_ATMTransferService.SenderReply(true, false, false), "a free, standing sender may send");
		ctx.Equal(LF_EATMTransferReply.RESTRAINED, LF_ATMTransferService.SenderReply(true, false, true), "a cuffed sender is refused");
		ctx.Equal(LF_EATMTransferReply.DOWNED, LF_ATMTransferService.SenderReply(true, true, false), "a downed sender is refused");
		ctx.Equal(LF_EATMTransferReply.DOWNED, LF_ATMTransferService.SenderReply(false, false, false), "a dead sender is refused");

		// Rate limit: a burst passes, the next request is refused, a new window opens again.
		LF_ATMTransferService.ResetStatics();
		int now = 100000;
		for (int i = 0; i < LF_ATMTransferService.RATE_BURST; i++)
		{
			ctx.True(LF_ATMTransferService.TakeSlot(7, now + i), "request " + i.ToString() + " inside the burst passes");
		}

		ctx.False(LF_ATMTransferService.TakeSlot(7, now + 10), "the request past the burst is refused");
		ctx.True(LF_ATMTransferService.TakeSlot(8, now + 10), "another player has their own bucket");
		ctx.True(LF_ATMTransferService.TakeSlot(7, now + LF_ATMTransferService.RATE_WINDOW_MS + 1), "a new window admits the player again");

		// A second SEND of a slow reply (PR #3860 review): the server refuses a repeat of the last sent transfer
		// inside the window, only for the same player, number and amount, and only after a sent one.
		LF_ATMTransferService.ResetStatics();
		int t0 = 500000;
		ctx.False(LF_ATMTransferService.IsDuplicate(7, RECIPIENT, 50, t0), "a first request is never a duplicate");
		if (!RedProof())
			LF_ATMTransferService.NoteSent(7, RECIPIENT, 50, t0);

		ctx.True(LF_ATMTransferService.IsDuplicate(7, RECIPIENT, 50, t0 + 6000), "the same transfer 6 s after a sent one is a duplicate");
		ctx.False(LF_ATMTransferService.IsDuplicate(7, RECIPIENT, 60, t0 + 6000), "another amount is a new transfer");
		ctx.False(LF_ATMTransferService.IsDuplicate(7, SENDER, 50, t0 + 6000), "another number is a new transfer");
		ctx.False(LF_ATMTransferService.IsDuplicate(8, RECIPIENT, 50, t0 + 6000), "another player is not blocked");
		ctx.False(LF_ATMTransferService.IsDuplicate(7, RECIPIENT, 50, t0 + LF_ATMTransferService.DUPLICATE_WINDOW_MS + 1), "past the window the same transfer is allowed again");

		// The client holds a pending request until the answer or the long timeout, past the page.
		ctx.False(LF_ATMTransferService.IsPending(t0), "no request in flight at first");
		LF_ATMTransferService.BeginPending(t0);
		ctx.True(LF_ATMTransferService.IsPending(t0 + 6000), "still pending after the old 5 s timeout");
		ctx.True(LF_ATMTransferService.IsPending(t0 + LF_ATMTransferService.PENDING_TIMEOUT_MS - 1), "pending until the long timeout");
		ctx.False(LF_ATMTransferService.IsPending(t0 + LF_ATMTransferService.PENDING_TIMEOUT_MS + 1), "the long timeout frees the page");
		ctx.True(LF_ATMTransferService.PENDING_TIMEOUT_MS > LF_ATMTransferService.DUPLICATE_WINDOW_MS, "the client holds longer than the server's duplicate window");
		LF_ATMTransferService.EndPending();
		ctx.False(LF_ATMTransferService.IsPending(t0 + 1), "the answer frees the page");

		// The fee line reads the server's settings once they arrive, not the local defaults.
		ctx.Equal(LF_BankTransferRules.Fee(LF_BankTransferConfig.GetOrDefaults(), 1000), LF_ATMTransferService.FeeFor(1000), "before the server answers the line shows the local settings");
		LF_ATMTransferService.SetServerFee(5, 10);
		ctx.Equal(105, LF_ATMTransferService.FeeFor(1000), "the line shows the server's flat fee plus percent");
		ctx.Equal(0, LF_ATMTransferService.FeeFor(0), "no fee on no amount");

		// The typed-text colour in the transfer page's edit box is not the phosphor green of the tube.
		LF_UISkin terminal = LF_UISkin.Terminal();
		ctx.True(terminal.m_FieldInk != null, "the terminal skin names a field ink");
		if (terminal.m_FieldInk)
			ctx.True(terminal.m_FieldInk.GetLuminance() < terminal.m_Ink.GetLuminance() * 0.5, "the field ink is darker than the tube's green, for the cream well");

		LF_ATMManager.ApplyAll(bankBefore);
		LF_BankTransfer.ResetStatics();
		LF_ATMTransferService.ResetStatics();
	}
}
