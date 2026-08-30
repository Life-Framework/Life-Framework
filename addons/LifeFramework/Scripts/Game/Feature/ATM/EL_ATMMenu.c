[BaseContainerProps(configRoot: true)]
class EL_ATMMenu : ChimeraMenuBase
{
	protected PlayerController m_PlayerController;

	protected Widget m_wRoot;
	protected TextWidget m_wBalance;
	protected EditBoxWidget m_wAmount;
	protected ButtonWidget m_wDepositButton;
	protected ButtonWidget m_wWithdrawButton;

	//! Last balance confirmed by the server. -1 until the first reply.
	protected int m_iServerBalance = -1;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		m_wRoot = GetRootWidget();

		m_wBalance = TextWidget.Cast(m_wRoot.FindWidget("BalanceText"));
		m_wAmount = EditBoxWidget.Cast(m_wRoot.FindWidget("AmountEdit"));
		m_wDepositButton = ButtonWidget.Cast(m_wRoot.FindWidget("DepositButton"));
		m_wWithdrawButton = ButtonWidget.Cast(m_wRoot.FindWidget("WithdrawButton"));
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
		RequestBalance();
	}

	//------------------------------------------------------------------------------------------------
	protected EL_CharacterATMComponent GetATMComponent()
	{
		if (!m_PlayerController)
			return null;
		IEntity entity = m_PlayerController.GetControlledEntity();
		if (!entity)
			return null;
		return EL_CharacterATMComponent.Cast(entity.FindComponent(EL_CharacterATMComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected void RequestBalance()
	{
		EL_CharacterATMComponent atmComponent = GetATMComponent();
		if (atmComponent)
			atmComponent.RequestBalance();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateBalance()
	{
		if (m_wBalance && m_iServerBalance >= 0)
		{
			string balanceText = WidgetManager.Translate("#EL-ATM_Balance") + " " + EL_MoneyFormat.FormatMoney(m_iServerBalance);
			m_wBalance.SetText(balanceText);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected int ReadAmount()
	{
		if (!m_wAmount)
			return 0;
		string amountText = m_wAmount.GetText();
		if (amountText.IsEmpty())
			return 0;
		return amountText.ToInt();
	}

	//------------------------------------------------------------------------------------------------
	protected void Deposit()
	{
		EL_CharacterATMComponent atmComponent = GetATMComponent();
		if (!atmComponent)
			return;

		int amount = ReadAmount();
		if (amount <= 0)
			return;

		atmComponent.RequestDeposit(amount);
	}

	//------------------------------------------------------------------------------------------------
	protected void Withdraw()
	{
		EL_CharacterATMComponent atmComponent = GetATMComponent();
		if (!atmComponent)
			return;

		int amount = ReadAmount();
		if (amount <= 0)
			return;

		atmComponent.RequestWithdraw(amount);
	}

	//------------------------------------------------------------------------------------------------
	void OnDepositResult(bool success, int newBalance)
	{
		m_iServerBalance = newBalance;
		UpdateBalance();
		if (success)
		{
			if (m_wAmount)
				m_wAmount.SetText("");
		}
		else
		{
			EL_Utils.Notify("#EL-ATM_InsufficientFunds", "#EL-ATM_Deposit", 3.0);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnWithdrawResult(bool success, int newBalance)
	{
		m_iServerBalance = newBalance;
		UpdateBalance();
		if (success)
		{
			if (m_wAmount)
				m_wAmount.SetText("");
		}
		else
		{
			EL_Utils.Notify("#EL-ATM_InsufficientFunds", "#EL-ATM_Withdraw", 3.0);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnBalanceReceived(int balance)
	{
		m_iServerBalance = balance;
		UpdateBalance();
	}
};