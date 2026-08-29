//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "EveronLife/Feature/Banking", description: "Player bank account with deposits and withdrawals")]
class EL_BankAccountComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class EL_BankAccountComponent : ScriptComponent
{
	[RplProp(onRplName: "OnBalanceChanged")]
	protected int m_iAccountBalance;
	
	[Attribute(defvalue: "0.02", UIWidgets.Slider, "Interest rate per paycheck cycle (2% default)", "0 0.2 0.001")]
	protected float m_fInterestRate;
	
	[Attribute(defvalue: "600", UIWidgets.Slider, "Time between interest payments (seconds)", "60 3600 10")]
	protected float m_fInterestInterval;
	
	protected float m_fTimeSinceLastInterest;
	protected int m_iTotalDeposits;
	protected int m_iTotalWithdrawals;
	protected ref array<ref EL_BankTransaction> m_aTransactionHistory;
	protected ref ScriptInvoker m_OnBalanceChanged;
	protected ref ScriptInvoker m_OnTransaction;
	
	//------------------------------------------------------------------------------------------------
	//! Notify replication system that data has changed
	protected void NotifyReplication()
	{
		IEntity owner = GetOwner();
		if (owner)
			Replication.BumpMe();
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);
		m_iAccountBalance = 20000;
		m_aTransactionHistory = new array<ref EL_BankTransaction>();
	}
	
	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		// Only process on server
		if (!Replication.IsServer() && Replication.IsRunning())
			return;
		
		m_fTimeSinceLastInterest += timeSlice;
		
		// Apply interest
		if (m_fTimeSinceLastInterest >= m_fInterestInterval && m_iAccountBalance > 0)
		{
			ApplyInterest();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Deposit money into bank account
	//! NOTE: This only adds to the balance. The caller (terminal/ATM) is responsible for taking cash from player.
	bool Deposit(int amount, string description = "Deposit")
	{
		if (amount <= 0)
			return false;
		
		// Add to bank account
		m_iAccountBalance += amount;
		m_iTotalDeposits += amount;
		
		// Record transaction
		RecordTransaction(EL_ETransactionType.DEPOSIT, amount, description);
		
		OnBalanceChanged();
		NotifyReplication();
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Withdraw money from bank account
	//! NOTE: This only removes from the balance. The caller (terminal/ATM) is responsible for giving cash to player.
	bool Withdraw(int amount, string description = "Withdrawal")
	{
		if (amount <= 0 || amount > m_iAccountBalance)
			return false;
		
		// Remove from bank account
		m_iAccountBalance -= amount;
		m_iTotalWithdrawals += amount;
		
		// Record transaction
		RecordTransaction(EL_ETransactionType.WITHDRAWAL, amount, description);
		
		OnBalanceChanged();
		NotifyReplication();
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Transfer money to another player's bank account
	bool Transfer(int amount, EL_BankAccountComponent recipient, string description = "Transfer")
	{
		if (!recipient || amount <= 0 || amount > m_iAccountBalance)
			return false;
		
		// Remove from sender
		m_iAccountBalance -= amount;
		RecordTransaction(EL_ETransactionType.TRANSFER_OUT, amount, description);
		
		// Add to recipient
		recipient.m_iAccountBalance += amount;
		recipient.RecordTransaction(EL_ETransactionType.TRANSFER_IN, amount, description);
		
		OnBalanceChanged();
		recipient.OnBalanceChanged();
		
		NotifyReplication();
		recipient.NotifyReplication();
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get current bank balance
	int GetBalance()
	{
		return m_iAccountBalance;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Apply interest to account
	protected void ApplyInterest()
	{
		m_fTimeSinceLastInterest = 0;
		
		if (m_iAccountBalance <= 0)
			return;
		
		int interest = Math.Round(m_iAccountBalance * m_fInterestRate);
		if (interest < 1)
			return;
		
		m_iAccountBalance += interest;
		
		RecordTransaction(EL_ETransactionType.INTEREST, interest, "Interest Payment");
		
		OnBalanceChanged();
		NotifyReplication();
		
		// Show notification
		string message = string.Format("Bank interest earned: $%1", interest);
		SCR_HintManagerComponent.ShowCustomHint(message, "Bank Account", 3.0);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Record a transaction
	protected void RecordTransaction(EL_ETransactionType type, int amount, string description)
	{
		EL_BankTransaction transaction = new EL_BankTransaction();
		transaction.m_eType = type;
		transaction.m_iAmount = amount;
		transaction.m_sDescription = description;
		
		m_aTransactionHistory.Insert(transaction);
		
		// Keep only last 100 transactions
		while (m_aTransactionHistory.Count() > 100)
		{
			m_aTransactionHistory.Remove(0);
		}
		
		if (m_OnTransaction)
			m_OnTransaction.Invoke(transaction);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get transaction history
	array<ref EL_BankTransaction> GetTransactionHistory()
	{
		return m_aTransactionHistory;
	}

	//------------------------------------------------------------------------------------------------
	//! Apply state loaded from persistence
	void ApplySavedState(int balance, array<ref EL_BankTransaction> recentTransactions)
	{
		m_iAccountBalance = balance;
		m_aTransactionHistory = new array<ref EL_BankTransaction>();
		if (recentTransactions)
		{
			foreach (EL_BankTransaction tx : recentTransactions)
			{
				m_aTransactionHistory.Insert(tx);
			}
		}
		// Notify listeners and replication after restore
		OnBalanceChanged();
		NotifyReplication();
	}
	
	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnBalanceChanged()
	{
		if (!m_OnBalanceChanged)
			m_OnBalanceChanged = new ScriptInvoker();
		
		return m_OnBalanceChanged;
	}
	
	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnTransaction()
	{
		if (!m_OnTransaction)
			m_OnTransaction = new ScriptInvoker();
		
		return m_OnTransaction;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnBalanceChanged()
	{
		Print(string.Format("[EL_BankAccountComponent] Balance changed to: %1 (IsServer: %2)", m_iAccountBalance, Replication.IsServer()), LogLevel.NORMAL);
		
		if (m_OnBalanceChanged)
			m_OnBalanceChanged.Invoke(m_iAccountBalance);

		// Replicate the balance to clients
		Replication.BumpMe();
	}
}

//------------------------------------------------------------------------------------------------
//! Transaction types
enum EL_ETransactionType
{
	DEPOSIT,
	WITHDRAWAL,
	TRANSFER_IN,
	TRANSFER_OUT,
	INTEREST,
	PAYMENT,
	REFUND
}

//------------------------------------------------------------------------------------------------
//! Bank transaction record
class EL_BankTransaction
{
	EL_ETransactionType m_eType;
	int m_iAmount;
	string m_sDescription;
	float m_fTimestamp;
	
	//------------------------------------------------------------------------------------------------
	void EL_BankTransaction()
	{
		m_eType = EL_ETransactionType.DEPOSIT;
		m_iAmount = 0;
		m_sDescription = "";
		m_fTimestamp = System.GetTickCount();
	}
	
	//------------------------------------------------------------------------------------------------
	string GetTypeString()
	{
		return typename.EnumToString(EL_ETransactionType, m_eType);
	}
}
