//------------------------------------------------------------------------------------------------
//! Carried cash seen through three calls, so the hand-over rule runs the same on a real
//! character (LF_EntityCashWallet over LF_MoneyUtils) and in a LOGIC test.
//------------------------------------------------------------------------------------------------
class LF_CashWallet : Managed
{
	//! \return Cash carried, 0 when unknown.
	int Carried()
	{
		return 0;
	}

	//! \return The amount actually taken.
	int Take(int amount)
	{
		return 0;
	}

	//! \return The amount actually put in.
	int Put(int amount)
	{
		return 0;
	}
}

//------------------------------------------------------------------------------------------------
//! The MoneyStack items a character carries.
class LF_EntityCashWallet : LF_CashWallet
{
	protected IEntity m_Owner;

	//------------------------------------------------------------------------------------------------
	void LF_EntityCashWallet(IEntity owner)
	{
		m_Owner = owner;
	}

	//------------------------------------------------------------------------------------------------
	override int Carried()
	{
		if (!m_Owner)
			return 0;

		int cash = LF_MoneyUtils.GetCash(m_Owner);
		if (cash < 0)
			return 0;

		return cash;
	}

	//------------------------------------------------------------------------------------------------
	override int Take(int amount)
	{
		if (!m_Owner)
			return 0;

		return LF_MoneyUtils.RemoveCash(m_Owner, amount);
	}

	//------------------------------------------------------------------------------------------------
	override int Put(int amount)
	{
		if (!m_Owner)
			return 0;

		return LF_MoneyUtils.AddCash(m_Owner, amount);
	}
}

//------------------------------------------------------------------------------------------------
enum LF_EGiveCash
{
	GIVEN,
	REFUSED_AMOUNT,
	REFUSED_NOT_CARRIED,
	FAILED_TAKE,
	FAILED_PUT,
	REFUSED_REACH
}

//------------------------------------------------------------------------------------------------
//! The hand-over. Cash is never created or lost: whatever the receiver could not take goes back
//! to the giver.
class LF_CashHandover
{
	//------------------------------------------------------------------------------------------------
	//! \param[out] moved Cash that ended up with the receiver.
	static LF_EGiveCash Move(notnull LF_CashWallet from, notnull LF_CashWallet to, int amount, out int moved)
	{
		moved = 0;
		if (amount <= 0)
			return LF_EGiveCash.REFUSED_AMOUNT;

		if (amount > from.Carried())
			return LF_EGiveCash.REFUSED_NOT_CARRIED;

		int taken = from.Take(amount);
		if (taken != amount)
		{
			if (taken > 0)
				from.Put(taken);

			return LF_EGiveCash.FAILED_TAKE;
		}

		int put = to.Put(taken);
		if (put < 0)
			put = 0;

		if (put < taken)
		{
			int refund = taken - put;
			int back = from.Put(refund);
			if (back != refund)
				LF_Debug.Error("Interaction", string.Format("cash refund short: %1 of %2 went back to the giver", back, refund));

			if (put > 0)
			{
				int reclaimed = to.Take(put);
				from.Put(reclaimed);
			}

			return LF_EGiveCash.FAILED_PUT;
		}

		moved = put;
		return LF_EGiveCash.GIVEN;
	}
}
