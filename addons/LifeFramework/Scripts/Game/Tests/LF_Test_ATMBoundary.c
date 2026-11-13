// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: commenting out the
// LF_MoneyUtils.AddCash(character, removed) rollback in LF_CharacterATMComponent.ServeDeposit
// failed it with `[LFTEST] FAIL atm/request-cash-boundary`,
// `- over-deposit leaves the 50 cash in the pockets (expected 50, got 0)`. Restored; green.

//------------------------------------------------------------------------------------------------
//! The ATM is the only cash-to-account boundary. Drives the server seams RpcAsk_Deposit and
//! RpcAsk_Withdraw run (ServeDeposit / ServeWithdraw) with a real character, so cash is the
//! MoneyStack in its pockets and the balance is the LF_BankAccount the RPC would use.
// tier: WORLD
class LF_Test_ATMBoundary : LF_Test
{
	override string GetName()
	{
		return "atm/request-cash-boundary";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ATMManager.Reset();
		string uid = "test-atm-boundary";

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		LF_BankAccount account = LF_ATMManager.GetOrCreate(uid);
		ctx.NotNull(account, "bank account exists");
		ctx.Equal(50, LF_MoneyUtils.AddCash(character, 50), "seed cash 50 added");
		if (ctx.FailureCount() > 0)
		{
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		// Deposit more than the pockets hold: nothing moves.
		ctx.False(LF_CharacterATMComponent.ServeDeposit(character, account, 100, uid), "over-deposit of 100 with 50 cash is refused");
		ctx.Equal(50, LF_MoneyUtils.GetCash(character), "over-deposit leaves the 50 cash in the pockets");
		ctx.Equal(0, account.GetBalance(), "over-deposit credits nothing");

		ctx.False(LF_CharacterATMComponent.ServeDeposit(character, account, 0, uid), "a zero deposit is refused");
		ctx.False(LF_CharacterATMComponent.ServeDeposit(character, account, -10, uid), "a negative deposit is refused");
		ctx.False(LF_CharacterATMComponent.ServeDeposit(character, account, LF_LedgerConfig.AtmMaxTransaction() + 1, uid), "an oversized deposit is refused");
		ctx.Equal(50, LF_MoneyUtils.GetCash(character), "refused deposits leave cash at 50");
		ctx.Equal(0, account.GetBalance(), "refused deposits leave the balance at 0");

		// Exact deposit moves every unit once.
		ctx.True(LF_CharacterATMComponent.ServeDeposit(character, account, 50, uid), "a deposit of all 50 cash is accepted");
		ctx.Equal(0, LF_MoneyUtils.GetCash(character), "deposit takes the 50 cash");
		ctx.Equal(50, account.GetBalance(), "deposit credits 50");

		// Withdraw more than the balance: refused, never negative.
		ctx.False(LF_CharacterATMComponent.ServeWithdraw(character, account, 80, uid), "withdrawing 80 from a balance of 50 is refused");
		ctx.Equal(50, account.GetBalance(), "a refused withdraw leaves the balance at 50");
		ctx.Equal(0, LF_MoneyUtils.GetCash(character), "a refused withdraw pays no cash");
		ctx.False(LF_CharacterATMComponent.ServeWithdraw(character, account, -50, uid), "a negative withdraw is refused");
		ctx.Equal(50, account.GetBalance(), "a negative withdraw cannot raise the balance");

		// Exact withdraw empties the account to 0, not below.
		ctx.True(LF_CharacterATMComponent.ServeWithdraw(character, account, 50, uid), "withdrawing the full 50 is accepted");
		ctx.Equal(0, account.GetBalance(), "full withdraw leaves the balance at 0");
		ctx.Equal(50, LF_MoneyUtils.GetCash(character), "full withdraw pays 50 cash");
		ctx.False(LF_CharacterATMComponent.ServeWithdraw(character, account, 1, uid), "withdrawing from an empty account is refused");
		ctx.Equal(0, account.GetBalance(), "an empty account stays at 0");

		LF_TestCharacterFixture.Delete(character);

		// All-or-nothing payout: a payout target with no inventory manager cannot take the cash,
		// so the withdraw is rolled back and the balance restored. A bare Character_Roleplay
		// still holds MoneyStack, so the target is a jacket lying on the ground.
		IEntity noInventory = LF_Utils.SpawnEntityPrefab(LF_TestCharacterFixture.JACKET_PREFAB, vector.Zero);
		ctx.NotNull(noInventory, "fixture: a jacket on the ground spawns");
		if (!noInventory)
			return;

		LF_BankAccount rollbackAccount = LF_ATMManager.GetOrCreate(uid + "-rollback");
		rollbackAccount.Deposit(40);
		ctx.Equal(0, LF_MoneyUtils.AddCash(noInventory, 1), "fixture: an entity with no inventory manager takes no cash");
		ctx.False(LF_CharacterATMComponent.ServeWithdraw(noInventory, rollbackAccount, 40, uid), "a withdraw the target cannot hold is refused");
		ctx.Equal(40, rollbackAccount.GetBalance(), "the refused payout is returned to the account");

		LF_TestCharacterFixture.Delete(noInventory);
	}
};
