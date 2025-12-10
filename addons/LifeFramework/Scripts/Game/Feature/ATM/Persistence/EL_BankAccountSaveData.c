[
	EPF_PersistentScriptedStateSettings(EL_BankAccount),
	EDF_DbName.Automatic()
]
class EL_BankAccountSaveData : EPF_ScriptedStateSaveData
{
	int m_iBalance;

	//------------------------------------------------------------------------------------------------
	override EPF_EReadResult ReadFrom(notnull Managed scriptedState)
	{
		EL_BankAccount account = EL_BankAccount.Cast(scriptedState);
		SetId(account.GetPersistentId());
		m_iBalance = account.GetBalance();
		return EPF_EReadResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override EPF_EApplyResult ApplyTo(notnull Managed scriptedState)
	{
		EL_BankAccount account = EL_BankAccount.Cast(scriptedState);
		account.SetPersistentId(GetId());
		account.SetBalance(m_iBalance);
		return EPF_EApplyResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	override bool Equals(notnull EPF_ScriptedStateSaveData other)
	{
		EL_BankAccountSaveData otherData = EL_BankAccountSaveData.Cast(other);
		return m_iBalance == otherData.m_iBalance;
	}
};