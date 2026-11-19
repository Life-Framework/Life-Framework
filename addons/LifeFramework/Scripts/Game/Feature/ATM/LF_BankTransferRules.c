// LF_BankTransferRules.c: the rules for a bank-to-bank transfer, as pure logic (#3318, #638).
// The server hands in what it knows (the config, both account numbers, whether the target exists,
// the amount, the sender's balance, what the sender already sent today) and gets back a verdict:
// the amount and fee to move, or one refusal reason the terminal or a toast can say in words.
// No world, no RPC, no save, no statics. Money is int and never goes through Math.Clamp.
// Nothing calls these rules yet; a later card wires them to the transfer RPC (money review there).

enum LF_EBankTransferRefusal
{
	NONE,
	DISABLED,
	OWN_ACCOUNT,
	UNKNOWN_ACCOUNT,
	INVALID_AMOUNT,
	NOT_ENOUGH_BALANCE,
	DAILY_LIMIT
}

//------------------------------------------------------------------------------------------------
//! The outcome of one transfer check. Refusal NONE means the transfer may go: take amount plus fee
//! from the sender, credit amount to the target.
class LF_BankTransferVerdict : Managed
{
	LF_EBankTransferRefusal m_eRefusal = LF_EBankTransferRefusal.NONE;
	int m_iAmount;
	int m_iFee;

	//------------------------------------------------------------------------------------------------
	bool IsAllowed()
	{
		return m_eRefusal == LF_EBankTransferRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! What leaves the sender's account: the amount and the fee.
	int TotalCharge()
	{
		return m_iAmount + m_iFee;
	}

	//------------------------------------------------------------------------------------------------
	static LF_BankTransferVerdict Refuse(LF_EBankTransferRefusal reason)
	{
		LF_BankTransferVerdict verdict();
		verdict.m_eRefusal = reason;
		return verdict;
	}
}

class LF_BankTransferRules
{
	//------------------------------------------------------------------------------------------------
	//! The fee on an amount: the flat fee plus the percentage (whole percent, rounded down). Split
	//! so a large amount cannot overflow an int. Never negative.
	static int Fee(LF_BankTransferConfig config, int amount)
	{
		if (!config || amount <= 0)
			return 0;

		int flat = LF_Utils.MaxInt(config.m_iFee, 0);
		int percent = LF_Utils.ClampInt(config.m_iFeePercent, 0, 100);
		int byPercent = (amount / 100) * percent + ((amount % 100) * percent) / 100;
		return flat + byPercent;
	}

	//------------------------------------------------------------------------------------------------
	//! Decides one transfer.
	//! \param config The transfer settings; null reads as the defaults.
	//! \param fromAccount The sender's account number.
	//! \param toAccount The account number the sender keyed in.
	//! \param toAccountKnown True when the bank holds an account with that number.
	//! \param amount What the sender wants to move.
	//! \param balance The sender's balance now.
	//! \param sentToday What the sender has already sent in the current day, fees not counted.
	static LF_BankTransferVerdict Evaluate(LF_BankTransferConfig config, string fromAccount, string toAccount, bool toAccountKnown, int amount, int balance, int sentToday)
	{
		if (!config)
			return Evaluate(new LF_BankTransferConfig(), fromAccount, toAccount, toAccountKnown, amount, balance, sentToday);

		if (!config.m_bEnabled)
			return LF_BankTransferVerdict.Refuse(LF_EBankTransferRefusal.DISABLED);

		if (!toAccount.IsEmpty() && toAccount == fromAccount)
			return LF_BankTransferVerdict.Refuse(LF_EBankTransferRefusal.OWN_ACCOUNT);

		if (toAccount.IsEmpty() || !toAccountKnown)
			return LF_BankTransferVerdict.Refuse(LF_EBankTransferRefusal.UNKNOWN_ACCOUNT);

		if (amount <= 0)
			return LF_BankTransferVerdict.Refuse(LF_EBankTransferRefusal.INVALID_AMOUNT);

		int fee = Fee(config, amount);
		// Compared as a difference so amount plus fee cannot overflow.
		if (balance < amount || balance - amount < fee)
			return LF_BankTransferVerdict.Refuse(LF_EBankTransferRefusal.NOT_ENOUGH_BALANCE);

		int limit = config.m_iDailyLimit;
		if (limit > 0)
		{
			int left = LF_Utils.MaxInt(limit - LF_Utils.MaxInt(sentToday, 0), 0);
			if (amount > left)
				return LF_BankTransferVerdict.Refuse(LF_EBankTransferRefusal.DAILY_LIMIT);
		}

		LF_BankTransferVerdict verdict();
		verdict.m_iAmount = amount;
		verdict.m_iFee = fee;
		return verdict;
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key for a refusal; empty for NONE.
	static string Key(LF_EBankTransferRefusal reason)
	{
		if (reason == LF_EBankTransferRefusal.DISABLED)
			return "#LF-Systems_Off";

		if (reason == LF_EBankTransferRefusal.OWN_ACCOUNT)
			return "#LF-BankTransfer_RefuseOwn";

		if (reason == LF_EBankTransferRefusal.UNKNOWN_ACCOUNT)
			return "#LF-BankTransfer_RefuseUnknown";

		if (reason == LF_EBankTransferRefusal.INVALID_AMOUNT)
			return "#LF-BankTransfer_RefuseAmount";

		if (reason == LF_EBankTransferRefusal.NOT_ENOUGH_BALANCE)
			return "#LF-BankTransfer_RefuseBalance";

		if (reason == LF_EBankTransferRefusal.DAILY_LIMIT)
			return "#LF-BankTransfer_RefuseLimit";

		return string.Empty;
	}
}
