//------------------------------------------------------------------------------------------------
//! Persisted form of a bank account.
//!
//! A DEDICATED RECORD, NOT LF_BankAccount ITSELF - the live class is the mutable session cache.
//! m_sPersistentId is the account's character persistence id; m_iBalance mirrors LF_BankAccount.
//------------------------------------------------------------------------------------------------
class LF_BankAccountRecord
{
	string m_sPersistentId;
	int m_iBalance;

	//------------------------------------------------------------------------------------------------
	//! \param[in] persistentId Account persistence id.
	//! \param[in] balance Account balance to store.
	//! \return A fully populated bank account record.
	static LF_BankAccountRecord Create(string persistentId, int balance)
	{
		LF_BankAccountRecord record();
		record.m_sPersistentId = persistentId;
		record.m_iBalance = balance;
		return record;
	}
}