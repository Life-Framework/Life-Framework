[ComponentEditorProps(category: "EveronLife/ATM", description: "Component for character ATM interaction")]
class EL_CharacterATMComponentClass : ScriptComponentClass
{
}

class EL_CharacterATMComponent : ScriptComponent
{
	//! How far the player may stand from an ATM's origin when the server takes a deposit or
	//! withdraw. The ATM action shows at 3 m; the extra 2 m absorbs movement and latency.
	static const float ATM_RANGE = 5.0;

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
	//! bridge. A listen-server host calls the handler directly, the same as
	//! the shop bridge, so the host's own requests never depend on a loopback.
	void RequestDeposit(int amount)
	{
		if (Replication.IsServer())
			RpcAsk_Deposit(amount);
		else
			Rpc(RpcAsk_Deposit, amount);
	}

	//------------------------------------------------------------------------------------------------
	void RequestWithdraw(int amount)
	{
		if (Replication.IsServer())
			RpcAsk_Withdraw(amount);
		else
			Rpc(RpcAsk_Withdraw, amount);
	}

	//------------------------------------------------------------------------------------------------
	void RequestBalance()
	{
		if (Replication.IsServer())
			RpcAsk_GetBalance();
		else
			Rpc(RpcAsk_GetBalance);
	}

	//------------------------------------------------------------------------------------------------
	//! \param origin World position of the player.
	//! \return true when an ATM stands within ATM_RANGE of origin. The server checks this on every
	//! deposit and withdraw, so a menu kept open (or a forged RPC) cannot bank cash away from an ATM.
	static bool IsAtATM(vector origin)
	{
		return EL_ActionSite.IsNear(origin, ATM_RANGE, "EL_ATMAction");
	}

	//------------------------------------------------------------------------------------------------
	//! Refuses a cash request made away from an ATM. Logs the refusal and tells the player.
	//! \return true when the request was refused.
	protected bool RefuseAwayFromATM(IEntity character, string request, int amount, string playerUid)
	{
		if (IsAtATM(character.GetOrigin()))
			return false;

		EL_Debug.Warn("ATM", string.Format("%1 refused: not at an ATM (amount=%2 uid %3)", request, amount, playerUid));
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
		{
			int playerId = playerManager.GetPlayerIdFromControlledEntity(character);
			if (playerId > 0)
				EL_NotificationManagerComponent.NotifyPlayer(playerId, "#EL-ATM_Title", "#EL-ATM_TooFar", 3.0, EL_ENotificationType.WARNING);
		}

		return true;
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
				ReplyDeposit(false, 0);
				return;
			}
			if (RefuseAwayFromATM(character, "deposit", amount, playerUid))
			{
				ReplyDeposit(false, account.GetBalance());
				return;
			}
			success = ServeDeposit(character, account, amount, playerUid);
		}
		else
		{
			EL_Debug.Error("ATM", string.Format("deposit rejected: no owning character (amount=%1)", amount));
		}

		if (account)
			newBalance = account.GetBalance();
		ReplyDeposit(success, newBalance);
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
				ReplyWithdraw(false, 0);
				return;
			}
			if (RefuseAwayFromATM(character, "withdraw", amount, playerUid))
			{
				ReplyWithdraw(false, account.GetBalance());
				return;
			}
			success = ServeWithdraw(character, account, amount, playerUid);
		}
		else
		{
			EL_Debug.Error("ATM", string.Format("withdraw rejected: no owning character (amount=%1)", amount));
		}

		if (account)
			newBalance = account.GetBalance();
		ReplyWithdraw(success, newBalance);
	}

	//------------------------------------------------------------------------------------------------
	//! Server seam of RpcAsk_Deposit, the cash-to-account boundary. Removes exactly the requested
	//! amount of MoneyStack and credits it. A partial removal is rolled back so a failed deposit
	//! never consumes cash and never credits the account. The caller has already checked range.
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
	//! Server seam of RpcAsk_Withdraw. All-or-nothing: when the payout cannot be made in full, the
	//! amount goes back to the account and any partial cash is taken back. The caller has already
	//! checked range.
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
		int balance = account.GetBalance();
		if (IsLocalPlayer())
			RpcDo_Balance(balance);
		else
			Rpc(RpcDo_Balance, balance);
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers a deposit outcome to the owning client. A listen-server host has no wire hop for an
	//! Owner-targeted RPC to itself, so the handler runs directly there (same as the shop bridge).
	protected void ReplyDeposit(bool success, int newBalance)
	{
		if (IsLocalPlayer())
			RpcDo_DepositResult(success, newBalance);
		else
			Rpc(RpcDo_DepositResult, success, newBalance);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReplyWithdraw(bool success, int newBalance)
	{
		if (IsLocalPlayer())
			RpcDo_WithdrawResult(success, newBalance);
		else
			Rpc(RpcDo_WithdrawResult, success, newBalance);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLocalPlayer()
	{
		IEntity player = GetOwner();
		if (!player)
			return false;

		RplComponent rpl = RplComponent.Cast(player.FindComponent(RplComponent));
		if (!rpl)
			return false;

		return rpl.IsOwner();
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
