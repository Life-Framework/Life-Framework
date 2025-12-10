[BaseContainerProps(configRoot: true)]
class EL_ATMMenu : ChimeraMenuBase
{
	protected PlayerController m_PlayerController;
	protected EL_BankAccount m_BankAccount;

	protected Widget m_wRoot;
	protected TextWidget m_wBalance;
	protected EditBoxWidget m_wAmount;
	protected ButtonWidget m_wDepositButton;
	protected ButtonWidget m_wWithdrawButton;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		m_wRoot = GetRootWidget();

		m_wBalance = TextWidget.Cast(m_wRoot.FindWidget("BalanceText"));
		m_wAmount = EditBoxWidget.Cast(m_wRoot.FindWidget("AmountEdit"));
		m_wDepositButton = ButtonWidget.Cast(m_wRoot.FindWidget("DepositButton"));
		m_wWithdrawButton = ButtonWidget.Cast(m_wRoot.FindWidget("WithdrawButton"));


		UpdateBalance();
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (w == m_wDepositButton)
		{
			Deposit();
			return true;
		}

		if (w == m_wWithdrawButton)
		{
			Withdraw();
			return true;
		}

		return super.OnClick(w, x, y, button);
	}

	//------------------------------------------------------------------------------------------------
	void SetPlayerController(PlayerController playerController)
	{
		m_PlayerController = playerController;
		IEntity entity = playerController.GetControlledEntity();
		string playerUid = EL_Utils.GetPlayerUID(entity);
		EL_ATMManager atmManager = EL_ATMManager.GetInstance();
		m_BankAccount = atmManager.GetAccount(playerUid);
		if (!m_BankAccount)
		{
			m_BankAccount = atmManager.CreateAccount(playerUid);
		}
	}

	// Simple localization helper used by widgets/menus. Returns the key if no localization
	// system is wired up yet. Replace with real localization lookup if available.
	protected string WidgetLocalize(string key)
	{
		return key;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateBalance()
	{
		if (m_wBalance && m_BankAccount)
		{
			string balanceText = WidgetLocalize("#EL-ATM_Balance") + m_BankAccount.GetBalance().ToString();
			m_wBalance.SetText(balanceText);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Deposit()
	{
		if (!m_BankAccount || !m_PlayerController)
			return;

		string amountText = m_wAmount.GetText();
		if (amountText.IsEmpty())
			return;

		int amount = amountText.ToInt();
		if (amount <= 0)
			return;

		// Assume player has money in inventory or something - placeholder
		// For now, just deposit directly
		EL_ATMManager atmManager = EL_ATMManager.GetInstance();
		atmManager.Deposit(m_BankAccount.GetPersistentId(), amount);
		UpdateBalance();
		m_wAmount.SetText("");
	}

	//------------------------------------------------------------------------------------------------
	protected void Withdraw()
	{
		if (!m_BankAccount || !m_PlayerController)
			return;

		string amountText = m_wAmount.GetText();
		if (amountText.IsEmpty())
			return;

		int amount = amountText.ToInt();
		if (amount <= 0)
			return;

		EL_ATMManager atmManager = EL_ATMManager.GetInstance();
		if (atmManager.Withdraw(m_BankAccount.GetPersistentId(), amount))
		{
			// Give money to player - placeholder
			UpdateBalance();
			m_wAmount.SetText("");
		}
		else
		{
			// Show insufficient funds message
		}
	}
};