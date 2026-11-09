[ComponentEditorProps(category: "EveronLife/ATM", description: "Component for character ATM interaction")]
class LF_CharacterATMComponentClass : ScriptComponentClass
{
}

class LF_CharacterATMComponent : ScriptComponent
{
	//! How far the player may stand from an ATM's origin when the server takes a deposit or
	//! withdraw. The ATM action shows at 3 m; the extra 2 m absorbs movement and latency.
	static const float ATM_RANGE = 5.0;

	//! World cue when cash changes hands at the machine: the character handling notes, played on
	//! the character for everyone nearby (vanilla inventory sound, same as the counter pay-outs).
	static const string CASH_SOUND = "SOUND_PICK_UP";

	protected ref LF_BankAccount m_BankAccount;

	//------------------------------------------------------------------------------------------------
	void Init(string playerUid)
	{
		LF_ATMManager atmManager = LF_ATMManager.GetInstance();
		if (atmManager)
			m_BankAccount = atmManager.GetOrCreate(playerUid);
	}

	//------------------------------------------------------------------------------------------------
	LF_BankAccount GetBankAccount()
	{
		return m_BankAccount;
	}

	// Public setter to allow external callbacks to set the bank account safely
	void SetBankAccount(LF_BankAccount account)
	{
		m_BankAccount = account;
	}

	//------------------------------------------------------------------------------------------------
	//! Terminal entry points: forward a request to the server through the RPC bridge. A
	//! listen-server host calls the handler directly, the same as the shop bridge, so the host's
	//! own requests never depend on a loopback.
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
	//! deposit and withdraw, so a screen kept open (or a forged RPC) cannot bank cash away from an ATM.
	static bool IsAtATM(vector origin)
	{
		return LF_ActionSite.IsNear(origin, ATM_RANGE, "LF_ATMAction");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: deposit cash from the player's inventory into their account. Always replies, with
	//! the reason when refused, so the terminal never hangs and always says why.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Deposit(int amount)
	{
		ServeRequest(true, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: withdraw from the account and pay out cash.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Withdraw(int amount)
	{
		ServeRequest(false, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one deposit or withdraw, from the range check to the reply.
	protected void ServeRequest(bool deposit, int amount)
	{
		string request = "withdraw";
		if (deposit)
			request = "deposit";

		IEntity character = GetOwner();
		if (!character)
		{
			LF_Debug.Error("ATM", string.Format("%1 rejected: no owning character (amount=%2)", request, amount));
			return;
		}

		string playerUid = LF_Utils.GetPlayerUID(character);
		LF_BankAccount account = LF_ATMManager.GetOrCreate(playerUid);
		if (!account)
		{
			LF_Debug.Error("ATM", request + " rejected: player UID is unavailable");
			ReplyResult(deposit, LF_EATMRefusal.NO_ACCOUNT, amount, 0, LF_MoneyUtils.GetCash(character));
			return;
		}

		LF_EATMRefusal reason;
		if (!IsAtATM(character.GetOrigin()))
		{
			LF_Debug.Warn("ATM", string.Format("%1 refused: not at an ATM (amount=%2 uid %3)", request, amount, playerUid));
			reason = LF_EATMRefusal.TOO_FAR;
		}
		else if (deposit)
		{
			reason = ServeDepositReason(character, account, amount, playerUid);
		}
		else
		{
			reason = ServeWithdrawReason(character, account, amount, playerUid);
		}

		if (reason == LF_EATMRefusal.NONE)
			PlayCashSound(character);

		ReplyResult(deposit, reason, amount, account.GetBalance(), LF_MoneyUtils.GetCash(character));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the world hears the notes change hands.
	protected static void PlayCashSound(IEntity character)
	{
		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(character);
		if (!inventory)
		{
			LF_Debug.Warn("ATM", "no inventory to play the cash sound on");
			return;
		}

		inventory.RpcAsk_PlaySound(LF_NetworkUtils.GetRplId(character), CASH_SOUND);
	}

	//------------------------------------------------------------------------------------------------
	//! Server seam of RpcAsk_Deposit. \see ServeDepositReason.
	//! \return True when the full amount moved from cash to the account.
	static bool ServeDeposit(IEntity character, notnull LF_BankAccount account, int amount, string playerUid)
	{
		return ServeDepositReason(character, account, amount, playerUid) == LF_EATMRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server seam of RpcAsk_Deposit, the cash-to-account boundary. Removes exactly the requested
	//! amount of MoneyStack and credits it. A partial removal is rolled back so a failed deposit
	//! never consumes cash and never credits the account. The caller has already checked range.
	//! \return NONE when the full amount moved, else why it did not.
	static LF_EATMRefusal ServeDepositReason(IEntity character, notnull LF_BankAccount account, int amount, string playerUid)
	{
		if (!LF_ATMManager.IsValidAmount(amount))
		{
			LF_Debug.Warn("ATM", string.Format("deposit rejected: invalid amount %1 (uid %2)", amount, playerUid));
			return LF_EATMRefusal.INVALID_AMOUNT;
		}

		int removed = LF_MoneyUtils.RemoveCash(character, amount, "atm-deposit");
		if (removed == amount)
		{
			account.Deposit(amount, "atm-deposit");
			LF_Debug.Info("ATM", string.Format("deposit +%1 -> balance %2 (uid %3)", amount, account.GetBalance(), playerUid));
			return LF_EATMRefusal.NONE;
		}

		if (removed > 0)
		{
			LF_MoneyUtils.AddCash(character, removed, "atm-deposit-rollback");
			LF_Debug.Warn("ATM", string.Format("deposit rolled back: removed %1 of %2, cash restored (uid %3)", removed, amount, playerUid));
		}
		else
		{
			LF_Debug.Warn("ATM", string.Format("deposit rejected: not enough cash for %1 (uid %2)", amount, playerUid));
		}

		return LF_EATMRefusal.NOT_ENOUGH_CASH;
	}

	//------------------------------------------------------------------------------------------------
	//! Server seam of RpcAsk_Withdraw. \see ServeWithdrawReason.
	//! \return True when the full amount moved from the account to cash.
	static bool ServeWithdraw(IEntity character, notnull LF_BankAccount account, int amount, string playerUid)
	{
		return ServeWithdrawReason(character, account, amount, playerUid) == LF_EATMRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server seam of RpcAsk_Withdraw. All-or-nothing: when the payout cannot be made in full, the
	//! amount goes back to the account and any partial cash is taken back. The caller has already
	//! checked range.
	//! \return NONE when the full amount moved, else why it did not.
	static LF_EATMRefusal ServeWithdrawReason(IEntity character, notnull LF_BankAccount account, int amount, string playerUid)
	{
		if (!LF_ATMManager.IsValidAmount(amount))
		{
			LF_Debug.Warn("ATM", string.Format("withdraw rejected: invalid amount %1 (uid %2)", amount, playerUid));
			return LF_EATMRefusal.INVALID_AMOUNT;
		}

		if (!account.Withdraw(amount, "atm-withdraw"))
		{
			LF_Debug.Warn("ATM", string.Format("withdraw rejected: balance %1 below %2 (uid %3)", account.GetBalance(), amount, playerUid));
			return LF_EATMRefusal.NOT_ENOUGH_BALANCE;
		}

		int paid = LF_MoneyUtils.AddCash(character, amount, "atm-withdraw");
		if (paid == amount)
		{
			LF_Debug.Info("ATM", string.Format("withdraw -%1 -> balance %2 (uid %3)", amount, account.GetBalance(), playerUid));
			return LF_EATMRefusal.NONE;
		}

		account.Deposit(amount, "atm-withdraw-rollback");
		LF_MoneyUtils.RemoveCash(character, paid, "atm-withdraw-rollback");
		LF_Debug.Warn("ATM", string.Format("withdraw rolled back: paid %1 of %2, balance restored (uid %3)", paid, amount, playerUid));
		return LF_EATMRefusal.POCKETS_FULL;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: report the balance and carried cash so the terminal opens with real numbers.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_GetBalance()
	{
		IEntity character = GetOwner();
		if (!character)
			return;

		LF_BankAccount account = LF_ATMManager.GetOrCreate(LF_Utils.GetPlayerUID(character));
		if (!account)
		{
			LF_Debug.Error("ATM", "balance request rejected: player UID is unavailable");
			return;
		}

		int balance = account.GetBalance();
		int cash = LF_MoneyUtils.GetCash(character);
		if (IsLocalPlayer())
			RpcDo_Balance(balance, cash);
		else
			Rpc(RpcDo_Balance, balance, cash);
	}

	//------------------------------------------------------------------------------------------------
	//! Delivers an outcome to the owning client. A listen-server host has no wire hop for an
	//! Owner-targeted RPC to itself, so the handler runs directly there (same as the shop bridge).
	protected void ReplyResult(bool deposit, LF_EATMRefusal reason, int amount, int balance, int cash)
	{
		if (IsLocalPlayer())
			RpcDo_Result(deposit, reason, amount, balance, cash);
		else
			Rpc(RpcDo_Result, deposit, reason, amount, balance, cash);
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
	protected void RpcDo_Result(bool deposit, int reason, int amount, int balance, int cash)
	{
		LF_ATMTerminal.OnResult(deposit, LF_ATMRefusal.FromWire(reason), amount, balance, cash);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Balance(int balance, int cash)
	{
		LF_ATMTerminal.OnBalance(balance, cash);
	}
};
