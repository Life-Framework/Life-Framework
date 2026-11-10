// LF_ATMTerminal.c — the cash machine's screen: green phosphor in a beige bezel, a keypad, and
// the reason when the machine says no. Client-only, one at a time, built on LF_UIScreen.
//
// Keys: the on-screen keypad (mouse, or pad focus that moves like a grid), the keyboard digits and
// numpad (LF_Keypad0..9), Backspace (LF_KeypadBack) and Delete or pad Y (LF_KeypadClear).
// Deposit and Withdraw send the amount to LF_CharacterATMComponent; the reply (balance, carried
// cash, refusal reason) is printed on the screen. It closes on Exit, MenuBack, death, losing the
// character, or walking CLOSE_DISTANCE from the machine.
class LF_ATMTerminal
{
	//! The ATM action shows within this range of the machine's origin.
	static const float SHOW_DISTANCE = 3.0;
	//! Half a metre past the action range, so a step while typing does not close the screen.
	static const float CLOSE_DISTANCE = 3.5;
	//! A request with no reply after this long tells the player instead of hanging.
	static const int REPLY_TIMEOUT_MS = 5000;

	static const string DEPOSIT = "DepositButton";
	static const string WITHDRAW = "WithdrawButton";
	static const string CLEAR = "KeyClearButton";
	static const string BACK = "KeyBackButton";

	protected static ref LF_ATMTerminal s_Current;

	protected ref LF_UIScreen m_Screen;
	protected int m_iAmount;
	protected int m_iBalance = -1;
	protected int m_iCash = -1;
	protected bool m_bWaiting;
	protected string m_sKeySound;
	protected string m_sAcceptSound;
	protected string m_sRefuseSound;

	//------------------------------------------------------------------------------------------------
	static LF_ATMTerminal Current()
	{
		return s_Current;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen CurrentScreen()
	{
		if (!s_Current)
			return null;

		return s_Current.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the terminal for the local player at machine.
	//! \param action The ATM action, for its configured sounds; null keeps the defaults.
	static LF_ATMTerminal Open(IEntity machine, IEntity user, LF_ATMAction action)
	{
		Hide();
		if (!machine || !user)
		{
			LF_Debug.Error("ATM", "terminal refused: no machine or no user");
			return null;
		}

		LF_ATMTerminal terminal = new LF_ATMTerminal();
		if (action)
		{
			terminal.m_sKeySound = action.GetKeySound();
			terminal.m_sAcceptSound = action.GetAcceptSound();
			terminal.m_sRefuseSound = action.GetRefuseSound();
		}

		if (!terminal.Build(machine))
			return null;

		s_Current = terminal;
		if (action)
			LF_UISound.Play(action.GetOpenSound());

		LF_CharacterATMComponent atm = LF_Component<LF_CharacterATMComponent>.Find(user);
		if (atm)
			atm.RequestBalance();
		else
			LF_Debug.Error("ATM", "terminal open but the character has no LF_CharacterATMComponent");

		LF_Debug.Info("ATM", "terminal open");
		return terminal;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Current)
			return;

		LF_ATMTerminal current = s_Current;
		s_Current = null;
		if (current.m_Screen)
			current.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! The parts of the terminal, top to bottom.
	static array<ref LF_UIPart> BuildParts()
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", "#LF-ATM_Header"));
		string unknown = WidgetManager.Translate("#LF-ATM_Unknown");
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Balance", WidgetManager.Translate("#LF-ATM_LineBalance", unknown)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Cash", WidgetManager.Translate("#LF-ATM_LineCash", unknown)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Debt", ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Entry", EntryText(0)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Status", "#LF-ATM_Prompt"));
		parts.Insert(Keys("Keys1", "Key1Button,Key2Button,Key3Button", "1,2,3"));
		parts.Insert(Keys("Keys2", "Key4Button,Key5Button,Key6Button", "4,5,6"));
		parts.Insert(Keys("Keys3", "Key7Button,Key8Button,Key9Button", "7,8,9"));
		parts.Insert(Keys("Keys4", CLEAR + ",Key0Button," + BACK, "#LF-ATM_KeyClear,0,#LF-ATM_KeyBack"));
		parts.Insert(Keys("Actions", DEPOSIT + "," + WITHDRAW, "#LF-ATM_KeyDeposit,#LF-ATM_KeyWithdraw"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "CloseButton", "#LF-ATM_Exit"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! A KEYS part from comma-separated key names and labels.
	static LF_UIPart Keys(string name, string keyNames, string keyTexts)
	{
		array<string> names = {};
		array<string> texts = {};
		keyNames.Split(",", names, true);
		keyTexts.Split(",", texts, true);
		return LF_UIPart.CreateKeys(name, names, texts);
	}

	//------------------------------------------------------------------------------------------------
	//! The entry line: the typed amount with a cursor.
	static string EntryText(int amount)
	{
		return WidgetManager.Translate("#LF-ATM_LineEntry", LF_MoneyFormat.FormatMoney(amount));
	}

	//------------------------------------------------------------------------------------------------
	protected bool Build(IEntity machine)
	{
		m_Screen = LF_UIScreen.Open(BuildParts(), LF_UISkin.Terminal());
		if (!m_Screen)
		{
			LF_Debug.Error("ATM", "terminal failed to open");
			return false;
		}

		m_Screen.GetOnButton().Insert(OnButton);
		m_Screen.GetOnClosed().Insert(OnClosed);
		for (int digit = 0; digit <= 9; digit++)
		{
			m_Screen.BindHotkey("LF_Keypad" + digit.ToString(), "Key" + digit.ToString() + "Button");
		}

		m_Screen.BindHotkey("LF_KeypadBack", BACK);
		m_Screen.BindHotkey("LF_KeypadClear", CLEAR);
		m_Screen.CloseOnDeath();
		m_Screen.CloseWhenUncontrolled();
		m_Screen.CloseBeyond(CLOSE_DISTANCE, machine.GetID());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		int digit = LF_ATMKeypad.DigitOf(name);
		if (digit >= 0)
		{
			SetAmount(LF_ATMKeypad.Press(m_iAmount, digit));
			LF_UISound.Play(m_sKeySound);
			return;
		}

		if (name == BACK)
		{
			SetAmount(LF_ATMKeypad.Back(m_iAmount));
			LF_UISound.Play(m_sKeySound);
			return;
		}

		if (name == CLEAR)
		{
			SetAmount(0);
			SetStatus("#LF-ATM_Prompt");
			LF_UISound.Play(m_sKeySound);
			return;
		}

		if (name == DEPOSIT)
		{
			Send(true);
			return;
		}

		if (name == WITHDRAW)
		{
			Send(false);
			return;
		}

		LF_Debug.Warn("ATM", "terminal ignored button " + name);
	}

	//------------------------------------------------------------------------------------------------
	protected void Send(bool deposit)
	{
		if (m_bWaiting)
		{
			LF_Debug.Info("ATM", "request ignored: the machine is still working");
			return;
		}

		LF_UISound.Play(m_sKeySound);
		if (!LF_ATMManager.IsValidAmount(m_iAmount))
		{
			LF_Debug.Info("ATM", "rejected: amount must be greater than zero");
			Refuse(LF_EATMRefusal.INVALID_AMOUNT);
			return;
		}

		LF_CharacterATMComponent atm = LF_Component<LF_CharacterATMComponent>.Find(LF_Utils.GetLocalCharacter());
		if (!atm)
		{
			LF_Debug.Error("ATM", "request failed: no ATM component on the controlled character");
			Refuse(LF_EATMRefusal.NO_ACCOUNT);
			return;
		}

		m_bWaiting = true;
		SetStatus("#LF-ATM_Working");
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(ReplyTimedOut, REPLY_TIMEOUT_MS, false);

		if (deposit)
			atm.RequestDeposit(m_iAmount);
		else
			atm.RequestWithdraw(m_iAmount);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReplyTimedOut()
	{
		if (!m_bWaiting)
			return;

		m_bWaiting = false;
		LF_Debug.Warn("ATM", "no reply from the server within " + REPLY_TIMEOUT_MS.ToString() + " ms");
		SetStatus("#LF-ATM_NoReply");
		LF_UISound.Play(m_sRefuseSound);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server's answer to a deposit or withdraw.
	static void OnResult(bool deposit, LF_EATMRefusal reason, int amount, int balance, int cash, int debt)
	{
		if (!s_Current)
		{
			LF_Debug.Info("ATM", "reply arrived after the terminal closed");
			return;
		}

		s_Current.ApplyResult(deposit, reason, amount, balance, cash, debt);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the balance and carried cash when the terminal opens.
	static void OnBalance(int balance, int cash, int debt)
	{
		if (s_Current)
			s_Current.ApplyFigures(balance, cash, debt);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyResult(bool deposit, LF_EATMRefusal reason, int amount, int balance, int cash, int debt)
	{
		m_bWaiting = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ReplyTimedOut);

		ApplyFigures(balance, cash, debt);
		if (reason != LF_EATMRefusal.NONE)
		{
			Refuse(reason);
			return;
		}

		SetAmount(0);
		if (deposit)
			SetStatus(WidgetManager.Translate("#LF-ATM_Deposited", LF_MoneyFormat.FormatMoney(amount)));
		else
			SetStatus(WidgetManager.Translate("#LF-ATM_Dispensed", LF_MoneyFormat.FormatMoney(amount)));

		LF_UISound.Play(m_sAcceptSound);
	}

	//------------------------------------------------------------------------------------------------
	protected void Refuse(LF_EATMRefusal reason)
	{
		SetStatus(LF_ATMRefusal.Key(reason));
		LF_UISound.Play(m_sRefuseSound);
		LF_Debug.Info("ATM", "terminal shows refusal " + typename.EnumToString(LF_EATMRefusal, reason));
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyFigures(int balance, int cash, int debt)
	{
		m_iBalance = balance;
		m_iCash = cash;
		if (!m_Screen)
			return;

		m_Screen.SetLine("Balance", WidgetManager.Translate("#LF-ATM_LineBalance", LF_MoneyFormat.FormatMoney(balance)));
		string cashText = WidgetManager.Translate("#LF-ATM_Unknown");
		if (cash >= 0)
			cashText = LF_MoneyFormat.FormatMoney(cash);

		m_Screen.SetLine("Cash", WidgetManager.Translate("#LF-ATM_LineCash", cashText));
		// Unpaid hospital bills past their deadline (#464); blank while nothing is owed.
		string debtText;
		if (debt > 0)
			debtText = WidgetManager.Translate("#LF-ATM_LineDebt", LF_MoneyFormat.FormatMoney(debt));

		m_Screen.SetLine("Debt", debtText);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetAmount(int amount)
	{
		m_iAmount = amount;
		if (m_Screen)
			m_Screen.SetLine("Entry", EntryText(amount));
	}

	//------------------------------------------------------------------------------------------------
	protected void SetStatus(string text)
	{
		if (m_Screen)
			m_Screen.SetLine("Status", text);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the listeners and timers if the instance goes away without a close.
	void ~LF_ATMTerminal()
	{
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(ReplyTimedOut);
		}

		if (m_Screen)
		{
			m_Screen.GetOnButton().Remove(OnButton);
			m_Screen.GetOnClosed().Remove(OnClosed);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosed()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ReplyTimedOut);

		if (s_Current == this)
			s_Current = null;

		LF_Debug.Info("ATM", "terminal closed");
	}

	//------------------------------------------------------------------------------------------------
	int GetAmount()
	{
		return m_iAmount;
	}
}
