//------------------------------------------------------------------------------------------------
//! Persisted form of a bank account.
//!
//! A DEDICATED RECORD, NOT EL_BankAccount ITSELF - the live class is the mutable session cache.
//! m_sPersistentId is the account's character persistence id; m_iBalance mirrors EL_BankAccount.
//------------------------------------------------------------------------------------------------
class EL_BankAccountRecord
{
	string m_sPersistentId;
	int m_iBalance;

	//------------------------------------------------------------------------------------------------
	//! \param[in] persistentId Account persistence id.
	//! \param[in] balance Account balance to store.
	//! \return A fully populated bank account record.
	static EL_BankAccountRecord Create(string persistentId, int balance)
	{
		EL_BankAccountRecord record();
		record.m_sPersistentId = persistentId;
		record.m_iBalance = balance;
		return record;
	}
}