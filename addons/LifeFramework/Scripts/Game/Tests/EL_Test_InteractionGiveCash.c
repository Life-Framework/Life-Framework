// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only` (all four broken in one run, 0/4 passed), then restored; green. Replacing `amount > from.Carried()` with `false` in LF_CashHandover.Move failed
// `[ELTEST] FAIL interaction/give-cash`, `- more than carried is refused (expected 2, got 3)`.
// tier: LOGIC
class EL_Test_InteractionGiveCash : EL_Test
{
	override string GetName()
	{
		return "interaction/give-cash";
	}

	override void Run(EL_TestContext ctx)
	{
		int moved;

		// Conservation: what leaves the giver arrives at the receiver.
		EL_TestCashWallet giver = EL_TestCashWallet.Create(500, 100000);
		EL_TestCashWallet receiver = EL_TestCashWallet.Create(500, 100000);
		ctx.Equal(LF_EGiveCash.GIVEN, LF_CashHandover.Move(giver, receiver, 50, moved), "50 of 500 is handed over");
		ctx.Equal(50, moved, "50 moved");
		ctx.Equal(450, giver.Carried(), "the giver keeps 450");
		ctx.Equal(550, receiver.Carried(), "the receiver holds 550");
		ctx.Equal(1000, giver.Carried() + receiver.Carried(), "no cash is created or lost");

		// Refusals move nothing.
		ctx.Equal(LF_EGiveCash.REFUSED_NOT_CARRIED, LF_CashHandover.Move(giver, receiver, 451, moved), "more than carried is refused");
		ctx.Equal(LF_EGiveCash.REFUSED_AMOUNT, LF_CashHandover.Move(giver, receiver, 0, moved), "zero is refused");
		ctx.Equal(LF_EGiveCash.REFUSED_AMOUNT, LF_CashHandover.Move(giver, receiver, -20, moved), "a negative amount is refused");
		ctx.Equal(0, moved, "a refusal moves nothing");
		ctx.Equal(450, giver.Carried(), "the giver still keeps 450 after the refusals");
		ctx.Equal(550, receiver.Carried(), "the receiver still holds 550 after the refusals");

		// A full receiver: everything goes back to the giver.
		EL_TestCashWallet full = EL_TestCashWallet.Create(990, 1000);
		ctx.Equal(LF_EGiveCash.FAILED_PUT, LF_CashHandover.Move(giver, full, 100, moved), "a receiver with room for 10 refuses 100");
		ctx.Equal(450, giver.Carried(), "the giver gets all 100 back");
		ctx.Equal(990, full.Carried(), "the full receiver keeps what it had");

		// Giving everything you carry is allowed.
		ctx.Equal(LF_EGiveCash.GIVEN, LF_CashHandover.Move(giver, receiver, 450, moved), "the whole pocket can be handed over");
		ctx.Equal(0, giver.Carried(), "the giver is empty");
		ctx.Equal(1000, receiver.Carried(), "the receiver holds 1000");
	}
}

//------------------------------------------------------------------------------------------------
//! In-memory cash with a capacity, for the hand-over rule.
class EL_TestCashWallet : LF_CashWallet
{
	protected int m_iCash;
	protected int m_iCapacity;

	static EL_TestCashWallet Create(int cash, int capacity)
	{
		EL_TestCashWallet wallet = new EL_TestCashWallet();
		wallet.m_iCash = cash;
		wallet.m_iCapacity = capacity;
		return wallet;
	}

	override int Carried()
	{
		return m_iCash;
	}

	override int Take(int amount)
	{
		int taken = amount;
		if (taken > m_iCash)
			taken = m_iCash;

		if (taken < 0)
			taken = 0;

		m_iCash -= taken;
		return taken;
	}

	override int Put(int amount)
	{
		int room = m_iCapacity - m_iCash;
		int put = amount;
		if (put > room)
			put = room;

		if (put < 0)
			put = 0;

		m_iCash += put;
		return put;
	}
}
