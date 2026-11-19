// red-proof: observed 2026-11-19 `tools\cli test --no-build --red LF_Test_BankTransferRules` printed `RED-OK bank/transfer-rules (fails with the hook on, 32 assertions)` (the hook points the config load at a missing file); `--only` then passed=1 failed=0.
//! Bank transfer rules (#3318, slice of #638): pure logic, no world. A known account that is not the
//! sender's own, an amount above zero within balance plus fee, the daily limit across two transfers,
//! and the fee. Every refusal has a localization key.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_BankTransferRules : LF_Test
{
	protected static const string FROM = "1001";
	protected static const string TO = "2002";
	protected static const ResourceName MISSING = "Configs/Money/LF_NoSuchBankTransfer.conf";

	override string GetName()
	{
		return "bank/transfer-rules";
	}

	protected LF_BankTransferConfig MakeConfig(int fee, int percent, int dailyLimit)
	{
		LF_BankTransferConfig config();
		config.m_bEnabled = true;
		config.m_iFee = fee;
		config.m_iFeePercent = percent;
		config.m_iDailyLimit = dailyLimit;
		return config;
	}

	override void Run(LF_TestContext ctx)
	{
		LF_BankTransferConfig config = MakeConfig(5, 0, 1000);

		// A good transfer returns the amount and the flat fee.
		LF_BankTransferVerdict good = LF_BankTransferRules.Evaluate(config, FROM, TO, true, 200, 500, 0);
		ctx.True(good.IsAllowed(), "a known other account, a positive amount and enough balance goes");
		ctx.Equal(200, good.m_iAmount, "a good transfer returns the amount");
		ctx.Equal(5, good.m_iFee, "a good transfer returns the flat fee");
		ctx.Equal(205, good.TotalCharge(), "the sender is charged amount plus fee");

		// Refusals, one reason each.
		LF_BankTransferVerdict own = LF_BankTransferRules.Evaluate(config, FROM, FROM, true, 200, 500, 0);
		ctx.Equal(LF_EBankTransferRefusal.OWN_ACCOUNT, own.m_eRefusal, "your own account number is refused");

		LF_BankTransferVerdict unknown = LF_BankTransferRules.Evaluate(config, FROM, "9999", false, 200, 500, 0);
		ctx.Equal(LF_EBankTransferRefusal.UNKNOWN_ACCOUNT, unknown.m_eRefusal, "an unknown number is refused");
		LF_BankTransferVerdict blank = LF_BankTransferRules.Evaluate(config, FROM, "", true, 200, 500, 0);
		ctx.Equal(LF_EBankTransferRefusal.UNKNOWN_ACCOUNT, blank.m_eRefusal, "an empty number is refused");

		LF_BankTransferVerdict zero = LF_BankTransferRules.Evaluate(config, FROM, TO, true, 0, 500, 0);
		ctx.Equal(LF_EBankTransferRefusal.INVALID_AMOUNT, zero.m_eRefusal, "zero is refused");
		LF_BankTransferVerdict negative = LF_BankTransferRules.Evaluate(config, FROM, TO, true, -50, 500, 0);
		ctx.Equal(LF_EBankTransferRefusal.INVALID_AMOUNT, negative.m_eRefusal, "a negative amount is refused");

		// Balance must cover the amount plus the fee: 200 + 5 needs 205.
		LF_BankTransferVerdict exact = LF_BankTransferRules.Evaluate(config, FROM, TO, true, 200, 205, 0);
		ctx.True(exact.IsAllowed(), "a balance of exactly amount plus fee goes");
		LF_BankTransferVerdict shortByFee = LF_BankTransferRules.Evaluate(config, FROM, TO, true, 200, 204, 0);
		ctx.Equal(LF_EBankTransferRefusal.NOT_ENOUGH_BALANCE, shortByFee.m_eRefusal, "a balance short of amount plus fee is refused");
		LF_BankTransferVerdict shortAmount = LF_BankTransferRules.Evaluate(config, FROM, TO, true, 200, 100, 0);
		ctx.Equal(LF_EBankTransferRefusal.NOT_ENOUGH_BALANCE, shortAmount.m_eRefusal, "a balance short of the amount is refused");

		// The daily limit holds across two transfers: 600 + 400 = 1000 fits, one more does not.
		LF_BankTransferVerdict first = LF_BankTransferRules.Evaluate(config, FROM, TO, true, 600, 5000, 0);
		ctx.True(first.IsAllowed(), "the first transfer is under the daily limit");
		int sent = first.m_iAmount;
		LF_BankTransferVerdict second = LF_BankTransferRules.Evaluate(config, FROM, TO, true, 400, 5000, sent);
		ctx.True(second.IsAllowed(), "a second transfer up to the limit goes");
		sent += second.m_iAmount;
		LF_BankTransferVerdict third = LF_BankTransferRules.Evaluate(config, FROM, TO, true, 1, 5000, sent);
		ctx.Equal(LF_EBankTransferRefusal.DAILY_LIMIT, third.m_eRefusal, "past the daily limit is refused");
		LF_BankTransferVerdict over = LF_BankTransferRules.Evaluate(config, FROM, TO, true, 401, 5000, 600);
		ctx.Equal(LF_EBankTransferRefusal.DAILY_LIMIT, over.m_eRefusal, "an amount that would cross the limit is refused");
		LF_BankTransferConfig unlimited = MakeConfig(0, 0, 0);
		LF_BankTransferVerdict uncapped = LF_BankTransferRules.Evaluate(unlimited, FROM, TO, true, 90000, 100000, 80000);
		ctx.True(uncapped.IsAllowed(), "a daily limit of 0 means no limit");

		// Fee: flat plus whole percent rounded down; ints only.
		LF_BankTransferConfig mixed = MakeConfig(2, 3, 0);
		ctx.Equal(2 + 30, LF_BankTransferRules.Fee(mixed, 1000), "flat fee plus 3 percent of 1000");
		ctx.Equal(2 + 1, LF_BankTransferRules.Fee(mixed, 50), "3 percent of 50 rounds down to 1");
		ctx.Equal(0, LF_BankTransferRules.Fee(mixed, 0), "no amount, no fee");
		ctx.Equal(2 + 64424509, LF_BankTransferRules.Fee(mixed, 2147483647), "a huge amount does not overflow the percentage");

		// A switched-off system refuses everything, and a null config reads as the defaults.
		LF_BankTransferConfig off = MakeConfig(0, 0, 0);
		off.m_bEnabled = false;
		LF_BankTransferVerdict disabled = LF_BankTransferRules.Evaluate(off, FROM, TO, true, 10, 500, 0);
		ctx.Equal(LF_EBankTransferRefusal.DISABLED, disabled.m_eRefusal, "transfers off refuses every transfer");
		LF_BankTransferVerdict defaults = LF_BankTransferRules.Evaluate(null, FROM, TO, true, 10, 500, 0);
		ctx.True(defaults.IsAllowed(), "a null config reads as the defaults");

		// Every refusal says why in words; a good transfer has no key.
		ctx.EqualStr("", LF_BankTransferRules.Key(LF_EBankTransferRefusal.NONE), "NONE has no key");
		ctx.False(LF_BankTransferRules.Key(LF_EBankTransferRefusal.OWN_ACCOUNT).IsEmpty(), "own account has a key");
		ctx.False(LF_BankTransferRules.Key(LF_EBankTransferRefusal.UNKNOWN_ACCOUNT).IsEmpty(), "unknown account has a key");
		ctx.False(LF_BankTransferRules.Key(LF_EBankTransferRefusal.INVALID_AMOUNT).IsEmpty(), "invalid amount has a key");
		ctx.False(LF_BankTransferRules.Key(LF_EBankTransferRefusal.NOT_ENOUGH_BALANCE).IsEmpty(), "short balance has a key");
		ctx.False(LF_BankTransferRules.Key(LF_EBankTransferRefusal.DAILY_LIMIT).IsEmpty(), "daily limit has a key");
		ctx.False(LF_BankTransferRules.Key(LF_EBankTransferRefusal.DISABLED).IsEmpty(), "disabled has a key");

		// The shipped config resolves and holds sane values.
		ResourceName path = LF_BankTransferConfig.CONFIG_PATH;
		if (RedProof())
			path = MISSING;
		LF_BankTransferConfig shipped = LF_BankTransferConfig.Load(path);
		ctx.NotNull(shipped, "the shipped Configs/Money/LF_BankTransfer.conf resolves");
		if (shipped)
		{
			ctx.True(shipped.m_bEnabled, "transfers are on in the shipped config");
			ctx.True(shipped.m_iFee >= 0, "the shipped flat fee is not negative");
			ctx.True(shipped.m_iDailyLimit >= 0, "the shipped daily limit is not negative");
		}
	}
}
