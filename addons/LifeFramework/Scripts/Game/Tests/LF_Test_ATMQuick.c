// red-proof: 2026-11-22, cli test --no-build --red LF_Test_ATMQuick,LF_Test_AtmPrefab: RED PROOF OK (both fail with the hook on), then --only passed.
//! atm/quick-lines: the cash machine's in-world lines (LF_ATMQuick). Fixed amounts are themselves,
//! "all" is the balance or the cash carried (capped at one transaction), a line the player cannot
//! afford is greyed with the right reason, an unknown figure never greys a line, labels carry the
//! amount, and a dead, downed or cuffed character is refused with CANNOT_ACT, which survives the wire.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ATMQuick : LF_Test
{
	override string GetName()
	{
		return "atm/quick-lines";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EATMQuickKind W = LF_EATMQuickKind.WITHDRAW;
		LF_EATMQuickKind D = LF_EATMQuickKind.DEPOSIT;
		int max = LF_LedgerConfig.AtmMaxTransaction();

		int fixedAmount = 0;
		int allBalance = 0;
		int allCapped = 0;
		int allCash = 0;
		int noCash = -5;
		if (!RedProof())
		{
			fixedAmount = LF_ATMQuick.Resolve(W, 100, 0, 0);
			allBalance = LF_ATMQuick.Resolve(W, LF_ATMQuick.ALL, 1900, 40);
			allCapped = LF_ATMQuick.Resolve(W, LF_ATMQuick.ALL, max + 500, 0);
			allCash = LF_ATMQuick.Resolve(D, LF_ATMQuick.ALL, 1900, 240);
			noCash = LF_ATMQuick.Resolve(D, LF_ATMQuick.ALL, 1900, 0);
		}

		ctx.Equal(100, fixedAmount, "Withdraw $100 moves 100");
		ctx.Equal(1900, allBalance, "Withdraw all moves the whole balance");
		ctx.Equal(max, allCapped, "Withdraw all is capped at one transaction");
		ctx.Equal(240, allCash, "Deposit all cash moves every note carried");
		ctx.Equal(0, noCash, "Deposit all with empty pockets moves nothing");

		LF_EATMRefusal overBalance = LF_EATMRefusal.NONE;
		LF_EATMRefusal overCash = LF_EATMRefusal.NONE;
		LF_EATMRefusal emptyAll = LF_EATMRefusal.NONE;
		LF_EATMRefusal emptyBank = LF_EATMRefusal.NONE;
		LF_EATMRefusal cuffed = LF_EATMRefusal.NONE;
		if (!RedProof())
		{
			overBalance = LF_ATMQuick.Check(W, 500, 100, 1000);
			overCash = LF_ATMQuick.Check(D, 100, 5000, 50);
			emptyAll = LF_ATMQuick.Check(D, LF_ATMQuick.ALL, 5000, 0);
			emptyBank = LF_ATMQuick.Check(W, LF_ATMQuick.ALL, 0, 300);
			cuffed = LF_ATMQuick.ActorRefusal(false, false, true);
		}

		ctx.Equal(LF_EATMRefusal.NOT_ENOUGH_BALANCE, overBalance, "Withdraw $500 with $100 in the bank is greyed: not enough in the bank");
		ctx.Equal(LF_EATMRefusal.NOT_ENOUGH_CASH, overCash, "Deposit $100 with $50 on you is greyed: not enough cash");
		ctx.Equal(LF_EATMRefusal.NOT_ENOUGH_CASH, emptyAll, "Deposit all cash with empty pockets is greyed");
		ctx.Equal(LF_EATMRefusal.NOT_ENOUGH_BALANCE, emptyBank, "Withdraw all from an empty account is greyed");
		ctx.Equal(LF_EATMRefusal.CANNOT_ACT, cuffed, "a cuffed character is refused");
		ctx.Equal(LF_EATMRefusal.CANNOT_ACT, LF_ATMQuick.ActorRefusal(false, true, false), "a downed character is refused");
		ctx.Equal(LF_EATMRefusal.CANNOT_ACT, LF_ATMQuick.ActorRefusal(true, false, false), "a dead character is refused");
		ctx.Equal(LF_EATMRefusal.NONE, LF_ATMQuick.ActorRefusal(false, false, false), "a free character may use the machine");
		ctx.Equal(LF_EATMRefusal.CANNOT_ACT, LF_ATMRefusal.FromWire(LF_EATMRefusal.CANNOT_ACT), "CANNOT_ACT survives the wire");
		ctx.False(LF_ATMRefusal.Key(LF_EATMRefusal.CANNOT_ACT).IsEmpty(), "CANNOT_ACT has a screen line");

		ctx.Equal(LF_EATMRefusal.NONE, LF_ATMQuick.Check(W, 20, 20, 0), "Withdraw $20 with exactly $20 is allowed");
		ctx.Equal(LF_EATMRefusal.NONE, LF_ATMQuick.Check(D, 500, 0, 600), "Deposit $500 with $600 on you is allowed");
		ctx.Equal(LF_EATMRefusal.NONE, LF_ATMQuick.Check(W, 500, LF_ATMQuick.UNKNOWN, LF_ATMQuick.UNKNOWN), "an unknown balance never greys a fixed line");

		string arg;
		ctx.EqualStr("#LF-ATM_QuickWithdraw", LF_ATMQuick.LabelKey(W, 100, 0, 0, arg), "a fixed withdraw line reads Withdraw %1");
		ctx.EqualStr(LF_MoneyFormat.FormatMoney(100), arg, "and shows its amount");
		ctx.EqualStr("#LF-ATM_QuickDepositAll", LF_ATMQuick.LabelKey(D, LF_ATMQuick.ALL, 0, 240, arg), "Deposit all cash shows the amount once cash is known");
		ctx.EqualStr(LF_MoneyFormat.FormatMoney(240), arg, "the cash carried");
		ctx.EqualStr("#LF-ATM_QuickWithdrawAllPlain", LF_ATMQuick.LabelKey(W, LF_ATMQuick.ALL, LF_ATMQuick.UNKNOWN, 0, arg), "Withdraw all reads plain before the balance is known");
		ctx.EqualStr("#LF-ATM_QuickNoBalance", LF_ATMQuick.ReasonKey(LF_EATMRefusal.NOT_ENOUGH_BALANCE), "the greyed reason for a withdrawal");
		ctx.EqualStr("#LF-ATM_ToastWithdrew", LF_ATMQuick.ToastKey(false, LF_EATMRefusal.NONE), "a withdrawal toasts Withdrew %1. Bank %2");
	}
}
