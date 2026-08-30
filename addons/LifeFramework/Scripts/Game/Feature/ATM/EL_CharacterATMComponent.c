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
	//! Removes exactly the requested amount; a partial removal is rolled back
	//! so a failed deposit never consumes cash. Always replies so the menu
	//! never hangs.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Deposit(int amount)
	{
		IEntity character = GetOwner();
		bool success = false;
		int newBalance = 0;
		EL_BankAccount account;

		if (character)
		{
			string playerUid = EL_Utils.GetPlayerUID(character);
			account = EL_ATMManager.GetOrCreate(playerUid);
			if (!account)
			{
				EL_Debug.Error("ATM", "deposit rejected: player UID is unavailable");
				Rpc(RpcDo_DepositResult, false, 0);
				return;
			}
			if (EL_ATMManager.IsValidAmount(amount))
			{
				int removed = EL_MoneyUtils.RemoveCash(character, amount);
				if (removed == amount)
				{
					account.Deposit(amount);
					success = true;
					EL_Debug.Log("ATM", string.Format("deposit +%1 -> balance %2 (uid %3)", amount, account.GetBalance(), playerUid));
				}
				else if (removed > 0)
				{
					EL_MoneyUtils.AddCash(character, removed);
					EL_Debug.Warn("ATM", string.Format("deposit rolled back: removed %1 of %2, cash restored (uid %3)", removed, amount, playerUid));
				}
				else
				{
					EL_Debug.Warn("ATM", string.Format("deposit rejected: not enough cash for %1 (uid %2)", amount, playerUid));
				}
			}
			else
			{
				EL_Debug.Warn("ATM", string.Format("deposit rejected: invalid amount %1 (uid %2)", amount, playerUid));
			}
		}
		else
		{
			EL_Debug.Error("ATM", string.Format("deposit rejected: no owning character (amount=%1)", amount));
		}

		if (account)
			newBalance = account.GetBalance();
		Rpc(RpcDo_DepositResult, success, newBalance);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: withdraw from the account and pay out cash. Refuses when the
	//! inventory cannot hold the payout, leaving the account untouched.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Withdraw(int amount)
	{
		IEntity character = GetOwner();
		bool success = false;
		int newBalance = 0;
		EL_BankAccount account;

		if (character)
		{
			string playerUid = EL_Utils.GetPlayerUID(character);
			account = EL_ATMManager.GetOrCreate(playerUid);
			if (!account)
			{
				EL_Debug.Error("ATM", "withdraw rejected: player UID is unavailable");
				Rpc(RpcDo_WithdrawResult, false, 0);
				return;
			}
			if (EL_ATMManager.IsValidAmount(amount))
			{
				if (account.Withdraw(amount))
				{
					int paid = EL_MoneyUtils.AddCash(character, amount);
					if (paid == amount)
					{
						success = true;
						EL_Debug.Log("ATM", string.Format("withdraw -%1 -> balance %2 (uid %3)", amount, account.GetBalance(), playerUid));
					}
					else
					{
						// All-or-nothing: the payout could not be made, return the
						// full amount to the account and take back the partial cash.
						account.Deposit(amount);
						EL_MoneyUtils.RemoveCash(character, paid);
						EL_Debug.Warn("ATM", string.Format("withdraw rolled back: paid %1 of %2, balance restored (uid %3)", paid, amount, playerUid));
					}
				}
				else
				{
					EL_Debug.Warn("ATM", string.Format("withdraw rejected: balance %1 below %2 (uid %3)", account.GetBalance(), amount, playerUid));
				}
			}
			else
			{
				EL_Debug.Warn("ATM", string.Format("withdraw rejected: invalid amount %1 (uid %2)", amount, playerUid));
			}
		}
		else
		{
			EL_Debug.Error("ATM", string.Format("withdraw rejected: no owning character (amount=%1)", amount));
		}

		if (account)
			newBalance = account.GetBalance();
		Rpc(RpcDo_WithdrawResult, success, newBalance);
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
		PlayerController playerController = PlayerController.Cast(GetGame().GetPlayerController());
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
