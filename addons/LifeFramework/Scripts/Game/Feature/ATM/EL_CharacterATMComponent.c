[ComponentEditorProps(category: "EveronLife/ATM", description: "Component for character ATM interaction")]
class EL_CharacterATMComponentClass : ScriptComponentClass
{
}

class EL_CharacterATMComponent : ScriptComponent
{
	protected ref EL_BankAccount m_BankAccount;

	//------------------------------------------------------------------------------------------------
	void Init(string playerUid)
	{
		EL_ATMManager atmManager = EL_ATMManager.GetInstance();
		if (atmManager)
			m_BankAccount = atmManager.GetOrCreate(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	EL_BankAccount GetBankAccount()
	{
		return m_BankAccount;
	}

	// Public setter to allow external callbacks to set the bank account safely
	void SetBankAccount(EL_BankAccount account)
	{
		m_BankAccount = account;
	}

	//------------------------------------------------------------------------------------------------
	//! Menu entry points: forward a request to the server through the RPC
	//! bridge. Safe to call from host and client alike; the engine routes
	//! server-direction RPCs locally on a host.
	void RequestDeposit(int amount)
	{
		Rpc(RpcAsk_Deposit, amount);
	}

	//------------------------------------------------------------------------------------------------
	void RequestWithdraw(int amount)
	{
		Rpc(RpcAsk_Withdraw, amount);
	}

	//------------------------------------------------------------------------------------------------
	void RequestBalance()
	{
		Rpc(RpcAsk_GetBalance);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: deposit cash from the player's inventory into their account.
	//! Always replies so the menu never hangs.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Deposit(int amount)
	{
		IEntity character = GetOwner();
		if (!character)
		{
			EL_Debug.Error("ATM", string.Format("deposit rejected: no owning character (amount=%1)", amount));
			Rpc(RpcDo_DepositResult, false, 0);
			return;
		}

		string playerUid = EL_Utils.GetPlayerUID(character);
		EL_BankAccount account = EL_ATMManager.GetOrCreate(playerUid);
		if (!account)
		{
			EL_Debug.Error("ATM", "deposit rejected: player UID is unavailable");
			Rpc(RpcDo_DepositResult, false, 0);
			return;
		}

		bool success = ServeDeposit(character, account, amount, playerUid);
		Rpc(RpcDo_DepositResult, success, account.GetBalance());
	}

	//------------------------------------------------------------------------------------------------
	//! Server seam of RpcAsk_Deposit, the cash-to-account boundary. Removes exactly the requested
	//! amount of MoneyStack and credits it. A partial removal is rolled back so a failed deposit
	//! never consumes cash and never credits the account.
	//! \return True when the full amount moved from cash to the account.
	static bool ServeDeposit(IEntity character, notnull EL_BankAccount account, int amount, string playerUid)
	{
		if (!EL_ATMManager.IsValidAmount(amount))
		{
			EL_Debug.Warn("ATM", string.Format("deposit rejected: invalid amount %1 (uid %2)", amount, playerUid));
			return false;
		}

		int removed = EL_MoneyUtils.RemoveCash(character, amount);
		if (removed == amount)
		{
			account.Deposit(amount);
			EL_Debug.Log("ATM", string.Format("deposit +%1 -> balance %2 (uid %3)", amount, account.GetBalance(), playerUid));
			return true;
		}

		if (removed > 0)
		{
			EL_MoneyUtils.AddCash(character, removed);
			EL_Debug.Warn("ATM", string.Format("deposit rolled back: removed %1 of %2, cash restored (uid %3)", removed, amount, playerUid));
		}
		else
		{
			EL_Debug.Warn("ATM", string.Format("deposit rejected: not enough cash for %1 (uid %2)", amount, playerUid));
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: withdraw from the account and pay out cash. Always replies so the menu never hangs.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Withdraw(int amount)
	{
		IEntity character = GetOwner();
		if (!character)
		{
			EL_Debug.Error("ATM", string.Format("withdraw rejected: no owning character (amount=%1)", amount));
			Rpc(RpcDo_WithdrawResult, false, 0);
			return;
		}

		string playerUid = EL_Utils.GetPlayerUID(character);
		EL_BankAccount account = EL_ATMManager.GetOrCreate(playerUid);
		if (!account)
		{
			EL_Debug.Error("ATM", "withdraw rejected: player UID is unavailable");
			Rpc(RpcDo_WithdrawResult, false, 0);
			return;
		}

		bool success = ServeWithdraw(character, account, amount, playerUid);
		Rpc(RpcDo_WithdrawResult, success, account.GetBalance());
	}

	//------------------------------------------------------------------------------------------------
	//! Server seam of RpcAsk_Withdraw, the account-to-cash boundary. All-or-nothing: refuses when
	//! the balance is short (the balance never goes negative), and when the inventory cannot hold
	//! the full payout it returns the amount to the account and takes back the partial cash.
	//! \return True when the full amount moved from the account to cash.
	static bool ServeWithdraw(IEntity character, notnull EL_BankAccount account, int amount, string playerUid)
	{
		if (!EL_ATMManager.IsValidAmount(amount))
		{
			EL_Debug.Warn("ATM", string.Format("withdraw rejected: invalid amount %1 (uid %2)", amount, playerUid));
			return false;
		}

		if (!account.Withdraw(amount))
		{
			EL_Debug.Warn("ATM", string.Format("withdraw rejected: balance %1 below %2 (uid %3)", account.GetBalance(), amount, playerUid));
			return false;
		}

		int paid = EL_MoneyUtils.AddCash(character, amount);
		if (paid == amount)
		{
			EL_Debug.Log("ATM", string.Format("withdraw -%1 -> balance %2 (uid %3)", amount, account.GetBalance(), playerUid));
			return true;
		}

		account.Deposit(amount);
		EL_MoneyUtils.RemoveCash(character, paid);
		EL_Debug.Warn("ATM", string.Format("withdraw rolled back: paid %1 of %2, balance restored (uid %3)", paid, amount, playerUid));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: report the current balance so the menu opens with real numbers.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_GetBalance()
	{
		IEntity character = GetOwner();
		if (!character)
			return;

		EL_BankAccount account = EL_ATMManager.GetOrCreate(EL_Utils.GetPlayerUID(character));
		if (!account)
		{
			EL_Debug.Error("ATM", "balance request rejected: player UID is unavailable");
			return;
		}
		Rpc(RpcDo_Balance, account.GetBalance());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_DepositResult(bool success, int newBalance)
	{
		EL_ATMMenu menu = EL_ATMMenu.Cast(GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.ATM));
		if (menu)
			menu.OnDepositResult(success, newBalance);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_WithdrawResult(bool success, int newBalance)
	{
		EL_ATMMenu menu = EL_ATMMenu.Cast(GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.ATM));
		if (menu)
			menu.OnWithdrawResult(success, newBalance);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Balance(int balance)
	{
		EL_ATMMenu menu = EL_ATMMenu.Cast(GetGame().GetMenuManager().FindMenuByPreset(ChimeraMenuPreset.ATM));
		if (menu)
			menu.OnBalanceReceived(balance);
	}

	//------------------------------------------------------------------------------------------------
	void OpenATMMenu()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (playerController)
		{
			EL_ATMMenu menu = EL_ATMMenu.Cast(GetGame().GetMenuManager().OpenMenu(ChimeraMenuPreset.ATM));
			if (menu)
			{
				menu.SetPlayerController(playerController);
			}
		}
	}
};
