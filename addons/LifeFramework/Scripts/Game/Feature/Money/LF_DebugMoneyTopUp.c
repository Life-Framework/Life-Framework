//! Playtest money top-up (LF_SpawnLogic.m_bDebugMoneyTopUp, on in GameMode_Roleplay_Debug only).
//! Rules are pure; Apply goes through LF_BankAccount.Deposit and LF_MoneyUtils.AddCash so the
//! ledger records reason debug-topup.
class LF_DebugMoneyTopUp
{
	static const string REASON = "debug-topup";

	//------------------------------------------------------------------------------------------------
	//! Amount to add so a value under `low` reaches `target`; 0 when off or not under `low`.
	static int Missing(bool enabled, int current, int low, int target)
	{
		if (!enabled || current >= low || target <= current)
			return 0;
		return target - current;
	}

	//------------------------------------------------------------------------------------------------
	static int StartingBank(bool enabled, int normal, int target)
	{
		if (enabled)
			return Math.Max(normal, target);
		return normal;
	}

	//------------------------------------------------------------------------------------------------
	static void Apply(string uid, IEntity character, int bankLow, int bankTarget, int cashLow, int cashTarget)
	{
		if (uid.IsEmpty())
			return;

		LF_BankAccount bank = LF_ATMManager.GetOrCreate(uid);
		if (bank)
		{
			int add = Missing(true, bank.GetBalance(), bankLow, bankTarget);
			if (add > 0)
			{
				bank.Deposit(add, REASON);
				LF_Debug.Info("Money", string.Format("debug-topup bank +%1 to %2 (%3)", add, bank.GetBalance(), uid));
			}
		}

		if (!character)
			return;

		int addCash = Missing(true, LF_MoneyUtils.GetCash(character), cashLow, cashTarget);
		if (addCash > 0)
		{
			int given = LF_MoneyUtils.AddCash(character, addCash, REASON);
			LF_Debug.Info("Money", string.Format("debug-topup cash +%1 (%2)", given, uid));
		}
	}
}
