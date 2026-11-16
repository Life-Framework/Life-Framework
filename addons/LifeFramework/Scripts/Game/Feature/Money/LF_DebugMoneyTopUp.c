//! Playtest money top-up (LF_SpawnLogic.m_bDebugMoneyTopUp, on in GameMode_Roleplay_Debug only).
//! Rules are pure; Apply goes through LF_BankAccount.Deposit and LF_MoneyUtils.AddCash so the
//! ledger records reason debug-topup.
//!
//! A cash amount set on purpose holds (#1855): the harness `resetplayer <cash>` calls HoldCash, and
//! the 60 s top-up tick leaves that player's carried cash alone until a reset without an amount
//! (ReleaseCash). Before, `resetplayer 1000` and a $250 buy left $750, the tick filled it to 20,000
//! between the buy and the sell, and the fence sale read as a jump to $20,100.
class LF_DebugMoneyTopUp
{
	static const string REASON = "debug-topup";

	//! Player uids whose carried cash the top-up leaves alone (server, session only).
	protected static ref set<string> s_HeldCash;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_HeldCash = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player's carried cash was set on purpose; the top-up leaves it alone.
	static void HoldCash(string uid)
	{
		if (uid.IsEmpty())
			return;

		if (!s_HeldCash)
			s_HeldCash = new set<string>();

		s_HeldCash.Insert(uid);
		LF_Debug.Info("Money", string.Format("debug-topup cash held for %1", uid));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the top-up fills the player's carried cash again.
	static void ReleaseCash(string uid)
	{
		if (s_HeldCash && s_HeldCash.Contains(uid))
		{
			s_HeldCash.RemoveItem(uid);
			LF_Debug.Info("Money", string.Format("debug-topup cash released for %1", uid));
		}
	}

	//------------------------------------------------------------------------------------------------
	static bool IsCashHeld(string uid)
	{
		return s_HeldCash && s_HeldCash.Contains(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Amount to add so a value under `low` reaches `target`; 0 when off or not under `low`.
	static int Missing(bool enabled, int current, int low, int target)
	{
		if (!enabled || current >= low || target <= current)
			return 0;
		return target - current;
	}

	//------------------------------------------------------------------------------------------------
	//! Carried cash the top-up adds for one player: Missing, or 0 while that player's cash is held.
	static int CashToAdd(string uid, int current, int low, int target)
	{
		if (IsCashHeld(uid))
			return 0;

		return Missing(true, current, low, target);
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

		int addCash = CashToAdd(uid, LF_MoneyUtils.GetCash(character), cashLow, cashTarget);
		if (addCash > 0)
		{
			int given = LF_MoneyUtils.AddCash(character, addCash, REASON);
			LF_Debug.Info("Money", string.Format("debug-topup cash +%1 (%2)", given, uid));
		}
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of this file's statics (Core/LF_StaticReset.c runs every feature's hook).
modded class LF_StaticResetHooks1
{
	override void Run(notnull array<string> done, bool apply)
	{
		super.Run(done, apply);
		done.Insert("LF_DebugMoneyTopUp");
		if (apply)
			LF_DebugMoneyTopUp.ResetStatics();
	}
}
