// red-proof: Observed 2026-11-10 with `tools/cli test --no-build --local --only LF_Test_BankTeller`: expecting balance 401 after the withdraw failed `atm/bank-teller` with `- the balance falls by 100 (expected 401, got 400)`. Restored; green.

// tier: WORLD
//! The 1989 bank teller (#590): an NPC at the bank counter whose "Talk to the teller" is the ATM
//! action, so deposit and withdraw run the same server seams and ledger reasons. Spawns the real
//! teller prefab, checks the server's range check resolves it as the counter, and drives a
//! withdraw of 100 then a deposit of 40 for a character standing in front of it.
class LF_Test_BankTeller : LF_Test
{
	protected static const ResourceName TELLER = "{8F3DB2237B844542}Prefabs/Characters/Npcs/LF_BankTeller.et";

	override string GetName()
	{
		return "atm/bank-teller";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity teller = LF_Utils.SpawnEntityPrefab(TELLER, "0 -60 0");
		ctx.NotNull(teller, "the teller prefab spawns");
		if (!teller)
			return;

		// The teller carries the ATM action, so the server's range check counts it as a counter.
		LF_ATMAction action = FindAction(teller);
		ctx.NotNull(action, "the teller carries the Talk to the teller action");
		if (action)
			ctx.True(action.HasLocalEffectOnlyScript(), "the teller screen opens only for the player who used it");

		vector tellerOrigin = teller.GetOrigin();
		ctx.True(LF_CharacterATMComponent.IsAtATM(tellerOrigin + "0 0 1.2"), "a player 1.2 m from the teller is served");
		ctx.False(LF_CharacterATMComponent.IsAtATM(tellerOrigin + "0 0 200"), "a player 200 m from the teller is not served");

		LF_ATMManager.Reset();
		array<ref LF_LedgerEntry> before = LF_Ledger.ExportAll();
		LF_Ledger.ApplyAll(null);
		LF_Ledger.EnsureSubscribed();

		string uid = "test-bank-teller";
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (character)
		{
			LF_BankAccount account = LF_ATMManager.GetOrCreate(uid);
			account.SetBalance(500);
			int cash0 = LF_MoneyUtils.GetCash(character);

			ctx.True(LF_CharacterATMComponent.ServeWithdraw(character, account, 100, uid), "withdraw 100 at the teller");
			ctx.Equal(400, account.GetBalance(), "the balance falls by 100");
			ctx.Equal(cash0 + 100, LF_MoneyUtils.GetCash(character), "the cash rises by 100");

			ctx.True(LF_CharacterATMComponent.ServeDeposit(character, account, 40, uid), "deposit 40 at the teller");
			ctx.Equal(440, account.GetBalance(), "the balance rises by 40");
			ctx.Equal(cash0 + 60, LF_MoneyUtils.GetCash(character), "the cash falls by 40");

			array<ref LF_LedgerEntry> last = {};
			LF_Ledger.Last(uid, LF_Ledger.ACCOUNT_BANK, 10, last);
			ctx.Equal(2, last.Count(), "the ledger has the withdraw and the deposit on the bank side");
			if (last.Count() == 2)
			{
				ctx.EqualStr("atm-deposit", last.Get(0).m_sReason, "the deposit keeps the ATM ledger reason");
				ctx.EqualStr("atm-withdraw", last.Get(1).m_sReason, "the withdraw keeps the ATM ledger reason");
				ctx.Equal(-100, last.Get(1).m_iDelta, "the withdraw is a 100 debit");
			}

			LF_TestCharacterFixture.Delete(character);
		}

		LF_Ledger.ApplyAll(before);
		LF_ATMManager.Reset();
		SCR_EntityHelper.DeleteEntityAndChildren(teller);
	}

	protected LF_ATMAction FindAction(IEntity entity)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction candidate : actions)
		{
			LF_ATMAction atm = LF_ATMAction.Cast(candidate);
			if (atm)
				return atm;
		}

		return null;
	}
}
