//------------------------------------------------------------------------------------------------
//! The licence desk's purse: a fee comes out of the player's bank account first and carried cash covers
//! the rest (review B13: the postal licence took a newcomer's last $100 of pocket money while $2,000 sat
//! in the bank). A refund goes back where the money came from, cash first.
class LF_LicenceFeeWallet : LF_CashWallet
{
	protected IEntity m_Owner;
	protected string m_sUid;
	protected string m_sReason;
	protected int m_iFromBank;
	protected int m_iFromCash;

	//------------------------------------------------------------------------------------------------
	void LF_LicenceFeeWallet(IEntity owner, string uid, string reason)
	{
		m_Owner = owner;
		m_sUid = uid;
		m_sReason = reason;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_BankAccount Account()
	{
		if (m_sUid.IsEmpty())
			return null;

		return LF_ATMManager.GetOrCreate(m_sUid);
	}

	//------------------------------------------------------------------------------------------------
	protected int Cash()
	{
		if (!m_Owner)
			return 0;

		return LF_Utils.MaxInt(LF_MoneyUtils.GetCash(m_Owner), 0);
	}

	//------------------------------------------------------------------------------------------------
	//! The bank balance plus the cash carried.
	override int Carried()
	{
		int total = Cash();
		LF_BankAccount account = Account();
		if (account)
			total += LF_Utils.MaxInt(account.GetBalance(), 0);

		return total;
	}

	//------------------------------------------------------------------------------------------------
	override int Take(int amount)
	{
		if (amount <= 0)
			return 0;

		int taken = 0;
		LF_BankAccount account = Account();
		if (account)
		{
			int fromBank = LF_Utils.MinInt(amount, account.GetBalance());
			if (fromBank > 0 && account.Withdraw(fromBank, m_sReason))
			{
				taken += fromBank;
				m_iFromBank += fromBank;
			}
		}

		if (taken < amount && m_Owner)
		{
			int fromCash = LF_MoneyUtils.RemoveCash(m_Owner, amount - taken, m_sReason);
			taken += fromCash;
			m_iFromCash += fromCash;
		}

		return taken;
	}

	//------------------------------------------------------------------------------------------------
	override int Put(int amount)
	{
		if (amount <= 0)
			return 0;

		int back = 0;
		int toCash = LF_Utils.MinInt(amount, m_iFromCash);
		if (toCash > 0 && m_Owner)
		{
			int put = LF_MoneyUtils.AddCash(m_Owner, toCash, m_sReason);
			back += put;
			m_iFromCash -= put;
		}

		int rest = amount - back;
		LF_BankAccount account = Account();
		if (rest > 0 && account)
		{
			account.Deposit(rest, m_sReason);
			back += rest;
			m_iFromBank = LF_Utils.MaxInt(m_iFromBank - rest, 0);
		}

		return back;
	}

	//------------------------------------------------------------------------------------------------
	int FromBank()
	{
		return m_iFromBank;
	}

	//------------------------------------------------------------------------------------------------
	int FromCash()
	{
		return m_iFromCash;
	}
}
