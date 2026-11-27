// red-proof: with `RedProof()` on no fee is taken and nothing is refunded, so "the bank pays first", "cash covers the
// rest", "a refund goes back to cash first" and the totals fail.

//! licence/fee-wallet: the licence desk's purse (LF_LicenceFeeWallet). A fee comes out of the bank account first and
//! carried cash covers the rest; a refund goes back where the money came from, cash first; a fee larger than
//! everything held takes only what exists; a non-positive amount moves nothing; a wallet with no body is bank only
//! and one with no account is cash only.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LicenceFeeWallet : LF_Test
{
	protected static const string UID = "test-licence-fee-wallet";

	override string GetName()
	{
		return "licence/fee-wallet";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ATMManager.Reset();
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
		{
			LF_ATMManager.Reset();
			return;
		}

		LF_BankAccount account = LF_ATMManager.GetOrCreate(UID);
		account.SetBalance(200);
		LF_MoneyUtils.RemoveCash(body, LF_MoneyUtils.GetCash(body));
		LF_MoneyUtils.AddCash(body, 100);

		LF_LicenceFeeWallet wallet = new LF_LicenceFeeWallet(body, UID, "test-fee");
		ctx.Equal(300, wallet.Carried(), "the wallet holds the bank balance plus the cash carried");

		int first = 0;
		int second = 0;
		int refundCash = 0;
		int refundMixed = 0;
		int zero = -1;
		int negative = -1;
		int overdraw = 0;
		if (!RedProof())
		{
			first = wallet.Take(150);
		}

		ctx.Equal(150, first, "a fee of 150 is taken in full");
		ctx.Equal(50, account.GetBalance(), "the bank pays first");
		ctx.Equal(100, LF_MoneyUtils.GetCash(body), "a fee the bank covers leaves the cash alone");
		ctx.Equal(150, wallet.FromBank(), "the wallet remembers 150 came from the bank");
		ctx.Equal(0, wallet.FromCash(), "the wallet remembers no cash was used");

		if (!RedProof())
		{
			second = wallet.Take(100);
		}

		ctx.Equal(100, second, "a second fee of 100 is taken in full");
		ctx.Equal(0, account.GetBalance(), "the bank is emptied before the pocket is touched");
		ctx.Equal(50, LF_MoneyUtils.GetCash(body), "carried cash covers the rest of the second fee");
		ctx.Equal(200, wallet.FromBank(), "200 in all came from the bank");
		ctx.Equal(50, wallet.FromCash(), "50 in all came from cash");

		if (!RedProof())
		{
			refundCash = wallet.Put(30);
		}

		ctx.Equal(30, refundCash, "a refund of 30 is handed back");
		ctx.Equal(80, LF_MoneyUtils.GetCash(body), "a refund goes back to cash first");
		ctx.Equal(0, account.GetBalance(), "a refund within the cash used does not touch the bank");
		ctx.Equal(20, wallet.FromCash(), "the cash taken is now 20");

		if (!RedProof())
		{
			refundMixed = wallet.Put(40);
		}

		ctx.Equal(40, refundMixed, "a refund of 40 is handed back");
		ctx.Equal(100, LF_MoneyUtils.GetCash(body), "the refund returns the last 20 of cash");
		ctx.Equal(20, account.GetBalance(), "the rest of the refund goes back to the bank");
		ctx.Equal(0, wallet.FromCash(), "no cash is owed back any more");
		ctx.Equal(180, wallet.FromBank(), "the bank part of the fee shrinks by what was returned");

		if (!RedProof())
		{
			zero = wallet.Take(0);
			negative = wallet.Take(-5);
		}

		ctx.Equal(0, zero, "a zero fee takes nothing");
		ctx.Equal(0, negative, "a negative fee takes nothing");
		ctx.Equal(0, wallet.Put(0), "a zero refund gives nothing back");
		ctx.Equal(100, LF_MoneyUtils.GetCash(body), "non-positive amounts leave the cash alone");
		ctx.Equal(20, account.GetBalance(), "non-positive amounts leave the bank alone");

		if (!RedProof())
		{
			overdraw = wallet.Take(10000);
		}

		ctx.Equal(120, overdraw, "a fee above everything held takes only what exists (20 bank + 100 cash)");
		ctx.Equal(0, account.GetBalance(), "the overdrawn bank is empty, never negative");
		ctx.Equal(0, LF_MoneyUtils.GetCash(body), "the overdrawn pocket is empty");
		ctx.Equal(0, wallet.Carried(), "nothing is left to carry");

		// A wallet with no body is bank only; one with no account is cash only.
		account.SetBalance(75);
		LF_LicenceFeeWallet bankOnly = new LF_LicenceFeeWallet(null, UID, "test-fee");
		ctx.Equal(75, bankOnly.Carried(), "with no body the wallet holds the bank only");
		ctx.Equal(75, bankOnly.Take(500), "with no body a big fee takes the whole bank and no more");
		ctx.Equal(0, account.GetBalance(), "the bank-only wallet emptied the account");

		LF_MoneyUtils.AddCash(body, 40);
		LF_LicenceFeeWallet cashOnly = new LF_LicenceFeeWallet(body, "", "test-fee");
		ctx.Equal(40, cashOnly.Carried(), "with no account id the wallet holds the cash only");
		ctx.Equal(40, cashOnly.Take(500), "with no account id a big fee takes all the cash and no more");
		ctx.Equal(0, LF_MoneyUtils.GetCash(body), "the cash-only wallet emptied the pocket");

		LF_TestCharacterFixture.Delete(body);
		LF_ATMManager.Reset();
	}
}
