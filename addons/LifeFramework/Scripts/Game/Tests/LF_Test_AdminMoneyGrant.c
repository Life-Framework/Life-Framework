// red-proof: observed 2026-11-22 `tools\cli test --no-build --red LF_Test_AdminMoneyGrant` printed `RED PROOF OK (all 1 listed test(s) fail with the hook on)`: RedProof() skips the bank grant, so `a grant of 500 lands in the bank` and the take assertions fail (`expected -200, got 0`); `--only` then passed=1 failed=0. The authority, amount-cap, parse and name rules are pure and pinned in the same run.

//! admin/money-grant: `#lfmoney <player id or name> <amount> [cash|bank]` (administrators only, chat or RCON).
//! Pins the authority rule (non-admin chat refused, RCON gated by its own permission), the amount rules
//! (whole number, not 0, at most 1,000,000 either way), the command parse (target with spaces, optional
//! account word), the target lookup (id or name, ambiguous refused) and the bank move through
//! LF_BankAccount, whose ledger reason is "admin-grant". Taking money never takes more than the account holds.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_AdminMoneyGrant : LF_Test
{
	override string GetName()
	{
		return "admin/money-grant";
	}

	override void Run(LF_TestContext ctx)
	{
		// Authority.
		ctx.False(LF_AdminMoney.MayGrant(true, false), "a chat caller who is not an administrator is refused");
		ctx.True(LF_AdminMoney.MayGrant(true, true), "a chat caller who is an administrator may grant");
		ctx.True(LF_AdminMoney.MayGrant(false, false), "RCON is gated by its admin permission before the handler runs");

		// Amount rules.
		ctx.EqualStr("", LF_AdminMoney.AmountError(500), "500 is allowed");
		ctx.EqualStr("", LF_AdminMoney.AmountError(-500), "-500 (take) is allowed");
		ctx.EqualStr("", LF_AdminMoney.AmountError(1000000), "the cap itself is allowed");
		ctx.EqualStr("", LF_AdminMoney.AmountError(-1000000), "the cap itself is allowed when taking");
		ctx.False(LF_AdminMoney.AmountError(0).IsEmpty(), "0 is refused");
		ctx.False(LF_AdminMoney.AmountError(1000001).IsEmpty(), "one over the cap is refused");
		ctx.False(LF_AdminMoney.AmountError(-1000001).IsEmpty(), "one over the cap is refused when taking");

		int amount;
		ctx.True(LF_AdminMoney.TryParseAmount("250", amount) && amount == 250, "250 parses");
		ctx.True(LF_AdminMoney.TryParseAmount("-40", amount) && amount == -40, "-40 parses");
		ctx.True(LF_AdminMoney.TryParseAmount("+7", amount) && amount == 7, "+7 parses");
		ctx.False(LF_AdminMoney.TryParseAmount("", amount), "empty does not parse");
		ctx.False(LF_AdminMoney.TryParseAmount("12.5", amount), "a decimal does not parse");
		ctx.False(LF_AdminMoney.TryParseAmount("lots", amount), "text does not parse");
		ctx.False(LF_AdminMoney.TryParseAmount("-", amount), "a bare sign does not parse");
		ctx.False(LF_AdminMoney.TryParseAmount("99999999999", amount), "a number too long for an int does not parse");

		// Command parse: argv[0] is the keyword.
		LF_AdminMoneyRequest request;
		array<string> argv = {"lfmoney", "3", "500"};
		ctx.EqualStr("", LF_AdminMoney.TryParse(argv, request), "id and amount parse");
		ctx.True(request != null && request.m_sTarget == "3" && request.m_iAmount == 500 && !request.m_bBank, "default account is cash");

		argv = {"lfmoney", "Jean", "Luc", "-250", "BANK"};
		ctx.EqualStr("", LF_AdminMoney.TryParse(argv, request), "a name with a space and the bank word parse");
		ctx.True(request != null && request.m_sTarget == "Jean Luc" && request.m_iAmount == -250 && request.m_bBank, "the name keeps its space, the account is bank");

		argv = {"lfmoney", "3"};
		ctx.False(LF_AdminMoney.TryParse(argv, request).IsEmpty(), "too few words answers the usage");
		ctx.True(request == null, "no request on a usage answer");
		argv = {"lfmoney", "3", "bank"};
		ctx.False(LF_AdminMoney.TryParse(argv, request).IsEmpty(), "an account word with no amount answers the usage");
		argv = {"lfmoney", "3", "5000000"};
		ctx.False(LF_AdminMoney.TryParse(argv, request).IsEmpty(), "an amount over the cap is refused at parse");
		argv = {"lfmoney", "3", "0"};
		ctx.False(LF_AdminMoney.TryParse(argv, request).IsEmpty(), "an amount of 0 is refused at parse");

		// Target lookup.
		array<int> ids = {2, 5, 6};
		array<string> names = {"Ana", "Bo", "bo"};
		ctx.Equal(2, LF_AdminMoney.ResolveTarget("2", ids, names), "a connected id resolves");
		ctx.Equal(-1, LF_AdminMoney.ResolveTarget("9", ids, names), "an id nobody has does not resolve");
		ctx.Equal(2, LF_AdminMoney.ResolveTarget("ana", ids, names), "a name matches ignoring case");
		ctx.Equal(-2, LF_AdminMoney.ResolveTarget("BO", ids, names), "two players with that name is ambiguous");
		ctx.Equal(-1, LF_AdminMoney.ResolveTarget("Zed", ids, names), "an unknown name does not resolve");

		// Bank move: a grant, a partial take, and the cap on the balance.
		LF_BankAccount account = LF_BankAccount.Create("admin-money-grant-test");
		int moved;
		if (!RedProof())
			moved = LF_AdminMoney.ApplyBank(account, 500);

		ctx.Equal(500, moved, "a grant of 500 moves 500");
		ctx.Equal(500, account.GetBalance(), "a grant of 500 lands in the bank");
		ctx.Equal(-200, LF_AdminMoney.ApplyBank(account, -200), "taking 200 moves 200 out");
		ctx.Equal(300, account.GetBalance(), "the bank holds the rest");
		ctx.Equal(-300, LF_AdminMoney.ApplyBank(account, -900), "taking more than the account holds takes only what it holds");
		ctx.Equal(0, account.GetBalance(), "the bank is not pushed into debt");
		account.SetBalance(1999999900);
		ctx.Equal(0, LF_AdminMoney.ApplyBank(account, 1000), "a grant past the balance cap moves nothing");
		ctx.Equal(1999999900, account.GetBalance(), "a refused grant leaves the balance alone");

		// The answer text.
		ctx.True(LF_AdminMoney.Describe(500, 500, true, "Ana").Contains("Gave $500"), "the answer says what was given");
		ctx.True(LF_AdminMoney.Describe(-500, -300, false, "Ana").Contains("Took $300"), "the answer says what was taken");
		ctx.True(LF_AdminMoney.Describe(-500, -300, false, "Ana").Contains("asked for -$500"), "a partial move names the request");
	}
}
