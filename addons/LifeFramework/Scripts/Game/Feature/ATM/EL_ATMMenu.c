//! ATM screen. Stays a MenuManager menu: EL_CharacterATMComponent finds it by preset to deliver
//! the server's replies, so the class name and the On*Result/OnBalanceReceived API are fixed.
//! Input (Enter, Esc, pad) goes through LF_UIMenuInput every frame.
[BaseContainerProps(configRoot: true)]
class EL_ATMMenu : ChimeraMenuBase
{
	protected PlayerController m_PlayerController;

	protected Widget m_wRoot;
	protected TextWidget m_wBalance;
	protected EditBoxWidget m_wAmount;
	protected Widget m_wDepositButton;
	protected Widget m_wWithdrawButton;
	protected Widget m_wCloseButton;

	protected ref LF_UIMenuInput m_Input = new LF_UIMenuInput();

	//! Last balance confirmed by the server. -1 until the first reply.
	protected int m_iServerBalance = -1;

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		m_wRoot = GetRootWidget();
		if (!m_wRoot)
		{
			EL_Debug.Error("ATM", "menu opened without a root widget");
			return;
		}

		m_wBalance = TextWidget.Cast(m_wRoot.FindAnyWidget("BalanceText"));
		m_wAmount = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("AmountEdit"));
		m_wDepositButton = m_wRoot.FindAnyWidget("DepositButton");
		m_wWithdrawButton = m_wRoot.FindAnyWidget("WithdrawButton");
		m_wCloseButton = m_wRoot.FindAnyWidget("CloseButton");

		SetButtonText(m_wDepositButton, "#EL-ATM_Deposit");
		SetButtonText(m_wWithdrawButton, "#EL-ATM_Withdraw");
		SetButtonText(m_wCloseButton, "#EL-ATM_Close");

		ScriptInvoker depositClick = LF_UIMenuInput.ClickInvoker(m_wDepositButton);
		if (depositClick)
			depositClick.Insert(OnDepositPressed);

		ScriptInvoker withdrawClick = LF_UIMenuInput.ClickInvoker(m_wWithdrawButton);
		if (withdrawClick)
			withdrawClick.Insert(OnWithdrawPressed);

		ScriptInvoker closeClick = LF_UIMenuInput.ClickInvoker(m_wCloseButton);
		if (closeClick)
			closeClick.Insert(OnClosePressed);

		array<Widget> focus = {};
		focus.Insert(m_wAmount);
		focus.Insert(m_wDepositButton);
		focus.Insert(m_wWithdrawButton);
		focus.Insert(m_wCloseButton);
		m_Input.SetFocusOrder(focus);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		LF_EUIMenuInput pressed = m_Input.Tick();
		if (pressed == LF_EUIMenuInput.BACK)
		{
			OnClosePressed();
			return;
		}

		if (pressed == LF_EUIMenuInput.SELECT)
			Press(m_Input.GetSelected());
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (Press(w))
			return true;

		return super.OnClick(w, x, y, button);
	}

	//------------------------------------------------------------------------------------------------
	protected bool Press(Widget w)
	{
		Widget entry = m_Input.RingEntry(w);
		if (!entry)
			return false;

		if (entry == m_wDepositButton)
		{
			OnDepositPressed();
			return true;
		}

		if (entry == m_wWithdrawButton)
		{
			OnWithdrawPressed();
			return true;
		}

		if (entry == m_wCloseButton)
		{
			OnClosePressed();
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDepositPressed()
	{
		if (m_Input.Ready("Deposit"))
			Deposit();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnWithdrawPressed()
	{
		if (m_Input.Ready("Withdraw"))
			Withdraw();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosePressed()
	{
		if (m_Input.Ready("Close"))
			Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void SetButtonText(Widget button, string text)
	{
		if (!button)
		{
			EL_Debug.Error("ATM", "layout is missing a button for " + text);
			return;
		}

		SCR_ButtonTextComponent textButton = SCR_ButtonTextComponent.Cast(button.FindHandler(SCR_ButtonTextComponent));
		if (textButton)
			textButton.SetText(text);
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
	//! Reads the typed amount; a blank, zero, negative or non-numeric entry is told, not ignored.
	protected int ReadValidAmount(string title)
	{
		int amount = ReadAmount();
		if (amount > 0)
			return amount;

		EL_Debug.Info("ATM", "rejected: amount must be greater than zero");
		EL_Utils.Notify("#EL-ATM_InvalidAmount", title, 3.0);
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void Deposit()
	{
		EL_CharacterATMComponent atmComponent = GetATMComponent();
		if (!atmComponent)
		{
			EL_Debug.Error("ATM", "deposit failed: no ATM component on the controlled character");
			return;
		}

		int amount = ReadValidAmount("#EL-ATM_Deposit");
		if (amount <= 0)
			return;

		atmComponent.RequestDeposit(amount);
	}

	//------------------------------------------------------------------------------------------------
	protected void Withdraw()
	{
		EL_CharacterATMComponent atmComponent = GetATMComponent();
		if (!atmComponent)
		{
			EL_Debug.Error("ATM", "withdraw failed: no ATM component on the controlled character");
			return;
		}

		int amount = ReadValidAmount("#EL-ATM_Withdraw");
		if (amount <= 0)
			return;

		atmComponent.RequestWithdraw(amount);
	}

	//------------------------------------------------------------------------------------------------
	//! The reply carries no reason code yet (EL_CharacterATMComponent.RpcDo_DepositResult), so the
	//! refusal names the likely cause per direction rather than one shared "insufficient funds".
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
			EL_Utils.Notify("#EL-ATM_DepositRefused", "#EL-ATM_Deposit", 3.0);
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
			EL_Utils.Notify("#EL-ATM_WithdrawRefused", "#EL-ATM_Withdraw", 3.0);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnBalanceReceived(int balance)
	{
		m_iServerBalance = balance;
		UpdateBalance();
	}
};
