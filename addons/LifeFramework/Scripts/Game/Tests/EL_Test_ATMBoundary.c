// red-proof: PENDING - not yet observed. The Arma Reforger Server install was being restored, so
// `tools\cli test --no-build --tier all` could not boot. Planned perturbation: delete the
// `EL_MoneyUtils.AddCash(character, removed)` rollback in EL_CharacterATMComponent.ServeDeposit;
// "over-deposit leaves the 50 cash in the pockets" goes red (expected 50, got 0).

//------------------------------------------------------------------------------------------------
//! The ATM is the only cash-to-account boundary. Drives the server seams RpcAsk_Deposit and
//! RpcAsk_Withdraw run (ServeDeposit / ServeWithdraw) with a real character, so cash is the
//! MoneyStack in its pockets and the balance is the EL_BankAccount the RPC would use.
// tier: WORLD
class EL_Test_ATMBoundary : EL_Test
{
	override string GetName()
	{
		return "atm/request-cash-boundary";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ATMManager.Reset();
		string uid = "test-atm-boundary";

		IEntity character = EL_TestCharacterFixture.Spawn(ctx, true);
		if (!character)
			return;

		EL_BankAccount account = EL_ATMManager.GetOrCreate(uid);
		ctx.NotNull(account, "bank account exists");
		ctx.Equal(50, EL_MoneyUtils.AddCash(character, 50), "seed cash 50 added");
		if (ctx.FailureCount() > 0)
		{
			EL_TestCharacterFixture.Delete(character);
			return;
		}

		// Deposit more than the pockets hold: nothing moves.
		ctx.False(EL_CharacterATMComponent.ServeDeposit(character, account, 100, uid), "over-deposit of 100 with 50 cash is refused");
		ctx.Equal(50, EL_MoneyUtils.GetCash(character), "over-deposit leaves the 50 cash in the pockets");
		ctx.Equal(0, account.GetBalance(), "over-deposit credits nothing");

		ctx.False(EL_CharacterATMComponent.ServeDeposit(character, account, 0, uid), "a zero deposit is refused");
		ctx.False(EL_CharacterATMComponent.ServeDeposit(character, account, -10, uid), "a negative deposit is refused");
		ctx.False(EL_CharacterATMComponent.ServeDeposit(character, account, EL_ATMManager.MAX_TRANSACTION_AMOUNT + 1, uid), "an oversized deposit is refused");
		ctx.Equal(50, EL_MoneyUtils.GetCash(character), "refused deposits leave cash at 50");
		ctx.Equal(0, account.GetBalance(), "refused deposits leave the balance at 0");

		// Exact deposit moves every unit once.
		ctx.True(EL_CharacterATMComponent.ServeDeposit(character, account, 50, uid), "a deposit of all 50 cash is accepted");
		ctx.Equal(0, EL_MoneyUtils.GetCash(character), "deposit takes the 50 cash");
		ctx.Equal(50, account.GetBalance(), "deposit credits 50");

		// Withdraw more than the balance: refused, never negative.
		ctx.False(EL_CharacterATMComponent.ServeWithdraw(character, account, 80, uid), "withdrawing 80 from a balance of 50 is refused");
		ctx.Equal(50, account.GetBalance(), "a refused withdraw leaves the balance at 50");
		ctx.Equal(0, EL_MoneyUtils.GetCash(character), "a refused withdraw pays no cash");
		ctx.False(EL_CharacterATMComponent.ServeWithdraw(character, account, -50, uid), "a negative withdraw is refused");
		ctx.Equal(50, account.GetBalance(), "a negative withdraw cannot raise the balance");

		// Exact withdraw empties the account to 0, not below.
		ctx.True(EL_CharacterATMComponent.ServeWithdraw(character, account, 50, uid), "withdrawing the full 50 is accepted");
		ctx.Equal(0, account.GetBalance(), "full withdraw leaves the balance at 0");
		ctx.Equal(50, EL_MoneyUtils.GetCash(character), "full withdraw pays 50 cash");
		ctx.False(EL_CharacterATMComponent.ServeWithdraw(character, account, 1, uid), "withdrawing from an empty account is refused");
		ctx.Equal(0, account.GetBalance(), "an empty account stays at 0");

		EL_TestCharacterFixture.Delete(character);

		// All-or-nothing payout: a character with no pockets cannot take the cash, so the
		// withdraw is rolled back and the balance restored.
		IEntity naked = EL_TestCharacterFixture.Spawn(ctx, false);
		if (!naked)
			return;

		EL_BankAccount rollbackAccount = EL_ATMManager.GetOrCreate(uid + "-rollback");
		rollbackAccount.Deposit(40);
		ctx.Equal(0, EL_MoneyUtils.AddCash(naked, 1), "fixture: a character with no clothing has no pocket for cash");
		ctx.False(EL_CharacterATMComponent.ServeWithdraw(naked, rollbackAccount, 40, uid), "a withdraw the inventory cannot hold is refused");
		ctx.Equal(40, rollbackAccount.GetBalance(), "the refused payout is returned to the account");
		ctx.True(EL_MoneyUtils.GetCash(naked) <= 0, "the refused payout leaves no cash behind");

		EL_TestCharacterFixture.Delete(naked);
	}
};
