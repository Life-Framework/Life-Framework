// LF_ATMTransferPage.c: the ATM's transfer page (#3320, slice of #638). The player keys in an account
// number and an amount, sees the fee, confirms, and gets a toast that names the amount and the last four
// characters of the recipient's number. Refusals are the #3318 rules in words. The page also prints the
// player's own number so they can tell it to someone.
//
// Three parts live here:
//  - LF_ATMTransferService: the server's decision, pure enough for the suite (trim, rate limit, the rules
//    through LF_BankTransfer.Send, and the reply code).
//  - modded LF_CharacterATMComponent: the RPC pair (client asks, server replies to its owner).
//  - LF_ATMTransferPage: the client screen on the UI kit, opened from the terminal's TRANSFER key.

//! The server's answer to one transfer request, on the wire as an int.
enum LF_EATMTransferReply
{
	SENT,
	DISABLED,
	OWN_ACCOUNT,
	UNKNOWN_ACCOUNT,
	INVALID_AMOUNT,
	NOT_ENOUGH_BALANCE,
	DAILY_LIMIT,
	TOO_FAR,
	RATE_LIMITED,
	NO_ACCOUNT,
	RESTRAINED,
	DOWNED,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! The server side of one transfer request. Static and clock-free: the RPC hands in the clock, the suite
//! hands in its own.
class LF_ATMTransferService
{
	static const string FEATURE = "ATMTransfer";
	//! An account number longer than this is no account number; it is refused as unknown before the rules.
	static const int MAX_NUMBER_LENGTH = 64; // lf-fixed: bounds a client string, not a gameplay knob
	//! Requests one player may make per window (a person at the keypad sends one every few seconds).
	static const int RATE_WINDOW_MS = 5000; // lf-fixed: anti-spam floor, like LF_RateLimit's windows
	static const int RATE_BURST = 3; // lf-fixed: see RATE_WINDOW_MS

	protected static ref LF_RateLimiter s_Limiter;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Limiter = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The number as the rules see it: whitespace trimmed off both ends. A number past MAX_NUMBER_LENGTH
	//! comes back empty, which the rules refuse as unknown.
	static string CleanNumber(string raw)
	{
		string number = raw.Trim();
		if (number.Length() > MAX_NUMBER_LENGTH)
			return string.Empty;

		return number;
	}

	//------------------------------------------------------------------------------------------------
	//! The last four characters of an account number, for the toast; the whole number when it is shorter.
	static string LastFour(string number)
	{
		int length = number.Length();
		if (length <= 4)
			return number;

		return number.Substring(length - 4, 4);
	}

	//------------------------------------------------------------------------------------------------
	//! Counts one request from a player against their bucket (own limiter, so the shop's bucket is not eaten).
	//! \param playerKey A stable per-player key; the player id on the server.
	//! \return True when the request may proceed.
	static bool TakeSlot(int playerKey, int nowMs)
	{
		if (!s_Limiter)
			s_Limiter = new LF_RateLimiter();

		int refusedBefore;
		LF_ERateVerdict verdict = s_Limiter.Take(playerKey, LF_ERequestKind.PURCHASE, nowMs, RATE_WINDOW_MS, RATE_BURST, refusedBefore);
		if (verdict == LF_ERateVerdict.ALLOW)
			return true;

		if (verdict == LF_ERateVerdict.REJECT)
			LF_Debug.Warn(FEATURE, string.Format("transfer request refused: player %1 is over %2 per %3 ms", playerKey, RATE_BURST, RATE_WINDOW_MS));

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the sender's body may send at all. A forged RPC (or a page left open) must not let a cuffed
	//! player move the account to a friend before the custody fine, nor a downed or dead one send.
	//! \return SENT when the sender may go on, else the refusal.
	static LF_EATMTransferReply SenderReply(bool alive, bool downed, bool restrained)
	{
		if (!alive || downed)
			return LF_EATMTransferReply.DOWNED;

		if (restrained)
			return LF_EATMTransferReply.RESTRAINED;

		return LF_EATMTransferReply.SENT;
	}

	//------------------------------------------------------------------------------------------------
	//! The wire reply for a rule refusal.
	static LF_EATMTransferReply FromRefusal(LF_EBankTransferRefusal refusal)
	{
		if (refusal == LF_EBankTransferRefusal.DISABLED)
			return LF_EATMTransferReply.DISABLED;

		if (refusal == LF_EBankTransferRefusal.OWN_ACCOUNT)
			return LF_EATMTransferReply.OWN_ACCOUNT;

		if (refusal == LF_EBankTransferRefusal.UNKNOWN_ACCOUNT)
			return LF_EATMTransferReply.UNKNOWN_ACCOUNT;

		if (refusal == LF_EBankTransferRefusal.INVALID_AMOUNT)
			return LF_EATMTransferReply.INVALID_AMOUNT;

		if (refusal == LF_EBankTransferRefusal.NOT_ENOUGH_BALANCE)
			return LF_EATMTransferReply.NOT_ENOUGH_BALANCE;

		if (refusal == LF_EBankTransferRefusal.DAILY_LIMIT)
			return LF_EATMTransferReply.DAILY_LIMIT;

		return LF_EATMTransferReply.FAILED;
	}

	//------------------------------------------------------------------------------------------------
	//! Screen line for a reply; empty for SENT.
	static string Key(LF_EATMTransferReply reply)
	{
		if (reply == LF_EATMTransferReply.DISABLED)
			return LF_BankTransferRules.Key(LF_EBankTransferRefusal.DISABLED);

		if (reply == LF_EATMTransferReply.OWN_ACCOUNT)
			return LF_BankTransferRules.Key(LF_EBankTransferRefusal.OWN_ACCOUNT);

		if (reply == LF_EATMTransferReply.UNKNOWN_ACCOUNT)
			return LF_BankTransferRules.Key(LF_EBankTransferRefusal.UNKNOWN_ACCOUNT);

		if (reply == LF_EATMTransferReply.INVALID_AMOUNT)
			return LF_BankTransferRules.Key(LF_EBankTransferRefusal.INVALID_AMOUNT);

		if (reply == LF_EATMTransferReply.NOT_ENOUGH_BALANCE)
			return LF_BankTransferRules.Key(LF_EBankTransferRefusal.NOT_ENOUGH_BALANCE);

		if (reply == LF_EATMTransferReply.DAILY_LIMIT)
			return LF_BankTransferRules.Key(LF_EBankTransferRefusal.DAILY_LIMIT);

		if (reply == LF_EATMTransferReply.TOO_FAR)
			return "#LF-ATM_RefuseFar";

		if (reply == LF_EATMTransferReply.RATE_LIMITED)
			return "#LF-ATMTransfer_RefuseRate";

		if (reply == LF_EATMTransferReply.NO_ACCOUNT)
			return "#LF-ATM_RefuseAccount";

		if (reply == LF_EATMTransferReply.RESTRAINED)
			return "#LF-CharSwitch_Cuffed";

		if (reply == LF_EATMTransferReply.DOWNED)
			return "#LF-CharSwitch_Downed";

		if (reply == LF_EATMTransferReply.FAILED)
			return "#LF-ATMTransfer_RefuseFailed";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! A reply that arrives over the wire as an int; anything unknown reads as failed.
	static LF_EATMTransferReply FromWire(int reply)
	{
		if (reply < LF_EATMTransferReply.SENT || reply > LF_EATMTransferReply.FAILED)
			return LF_EATMTransferReply.FAILED;

		return reply;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one transfer request after the range check and the rate limit. The transfer id is made here,
	//! never taken from the client; the number is trimmed first; LF_BankTransfer.Send runs the #3318 rules
	//! again on the server and moves the money exactly once.
	//! \param[out] amountSent What reached the recipient.
	//! \param[out] fee What the sender paid on top.
	static LF_EATMTransferReply Serve(string fromAccount, string rawNumber, int amount, out int amountSent, out int fee)
	{
		amountSent = 0;
		fee = 0;
		if (fromAccount.IsEmpty())
			return LF_EATMTransferReply.NO_ACCOUNT;

		string number = CleanNumber(rawNumber);
		if (number.IsEmpty())
			return LF_EATMTransferReply.UNKNOWN_ACCOUNT;

		string transferId = LF_BankTransfer.NewTransferId(fromAccount);
		LF_BankTransferOutcome outcome = LF_BankTransfer.Send(transferId, fromAccount, number, amount);
		if (outcome.IsDone())
		{
			amountSent = outcome.m_iAmount;
			fee = outcome.m_iFee;
			return LF_EATMTransferReply.SENT;
		}

		if (outcome.m_eStatus == LF_EBankTransferStatus.REFUSED)
			return FromRefusal(outcome.m_eRefusal);

		LF_Debug.Warn(FEATURE, string.Format("transfer %1 did not go: status %2", transferId, typename.EnumToString(LF_EBankTransferStatus, outcome.m_eStatus)));
		return LF_EATMTransferReply.FAILED;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ATMTransferService's statics (LF_StaticReset finds it by reflection).
class LF_ATMTransferService_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ATMTransferService";
	}

	override void Apply()
	{
		LF_ATMTransferService.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The transfer RPC pair on the character's ATM component.
modded class LF_CharacterATMComponent
{
	//------------------------------------------------------------------------------------------------
	//! Client: asks the server to send amount to the account number. A listen-server host calls the handler
	//! directly, like the deposit and withdraw requests.
	void RequestTransfer(string accountNumber, int amount)
	{
		if (Replication.IsServer())
			RpcAsk_Transfer(accountNumber, amount);
		else
			Rpc(RpcAsk_Transfer, accountNumber, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one transfer request from the owning client. Always replies, so the page never hangs.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Transfer(string accountNumber, int amount)
	{
		IEntity character = GetOwner();
		if (!character)
		{
			LF_Debug.Error(LF_ATMTransferService.FEATURE, "transfer rejected: no owning character");
			return;
		}

		string playerUid = LF_Utils.GetPlayerUID(character);
		if (playerUid.IsEmpty())
		{
			LF_Debug.Error(LF_ATMTransferService.FEATURE, "transfer rejected: player UID is unavailable");
			ReplyTransfer(LF_EATMTransferReply.NO_ACCOUNT, 0, 0, string.Empty, 0);
			return;
		}

		int playerKey = 0;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager)
			playerKey = playerManager.GetPlayerIdFromControlledEntity(character);

		// A request with no player (server-local) is not pooled into one bucket, the same rule as LF_RateLimit.
		if (playerKey > 0 && !LF_ATMTransferService.TakeSlot(playerKey, System.GetTickCount()))
		{
			ReplyTransfer(LF_EATMTransferReply.RATE_LIMITED, 0, 0, string.Empty, 0);
			return;
		}

		LF_EATMTransferReply senderReply = LF_ATMTransferService.SenderReply(LF_Utils.IsAliveState(character), LF_Downed.IsDowned(character), LF_Restraint.IsRestrained(character));
		if (senderReply != LF_EATMTransferReply.SENT)
		{
			LF_Debug.Warn(LF_ATMTransferService.FEATURE, string.Format("transfer refused: sender cannot act, %1 (uid %2)", typename.EnumToString(LF_EATMTransferReply, senderReply), playerUid));
			ReplyTransfer(senderReply, 0, 0, string.Empty, 0);
			return;
		}

		if (!IsAtATM(character.GetOrigin()))
		{
			LF_Debug.Warn(LF_ATMTransferService.FEATURE, string.Format("transfer refused: not at an ATM (uid %1)", playerUid));
			ReplyTransfer(LF_EATMTransferReply.TOO_FAR, 0, 0, string.Empty, 0);
			return;
		}

		int amountSent;
		int fee;
		LF_EATMTransferReply reply = LF_ATMTransferService.Serve(playerUid, accountNumber, amount, amountSent, fee);
		int balance;
		LF_BankAccount account = LF_ATMManager.GetInstance().GetAccount(playerUid);
		if (account)
			balance = account.GetBalance();

		string last4;
		if (reply == LF_EATMTransferReply.SENT)
			last4 = LF_ATMTransferService.LastFour(LF_ATMTransferService.CleanNumber(accountNumber));

		ReplyTransfer(reply, amountSent, fee, last4, balance);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReplyTransfer(LF_EATMTransferReply reply, int amount, int fee, string last4, int balance)
	{
		int wire = reply;
		if (IsLocalPlayer())
			RpcDo_TransferResult(wire, amount, fee, last4, balance);
		else
			Rpc(RpcDo_TransferResult, wire, amount, fee, last4, balance);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_TransferResult(int reply, int amount, int fee, string last4, int balance)
	{
		LF_ATMTransferPage.OnResult(LF_ATMTransferService.FromWire(reply), amount, fee, last4, balance);
	}
}

//------------------------------------------------------------------------------------------------
//! The client screen: your number, the number to send to, the amount, the fee, a confirm key. One at a
//! time, built on LF_UIScreen like the terminal.
class LF_ATMTransferPage
{
	//! A request with no reply after this long tells the player instead of hanging.
	static const int REPLY_TIMEOUT_MS = 5000; // lf-fixed: same bound as the terminal's reply wait, not a gameplay knob
	static const float TOAST_SECONDS = 6.0; // lf-fixed: how long the toast stays up, a UI timing

	static const string TARGET = "Target";
	static const string SEND = "SendButton";
	static const string BACK_TO_MENU = "MenuButton";
	static const string CLEAR = "KeyClearButton";
	static const string BACK = "KeyBackButton";

	protected static ref LF_ATMTransferPage s_Current;

	protected ref LF_UIScreen m_Screen;
	protected EntityID m_MachineId = EntityID.INVALID;
	protected ref LF_ATMAction m_Action;
	protected int m_iAmount;
	protected bool m_bWaiting;
	protected string m_sKeySound;
	protected string m_sAcceptSound;
	protected string m_sRefuseSound;

	//------------------------------------------------------------------------------------------------
	static LF_ATMTransferPage Current()
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
	//! Opens the page for the local player at machine. \param action The ATM action, for its sounds.
	static LF_ATMTransferPage Open(IEntity machine, LF_ATMAction action)
	{
		Hide();
		if (!machine)
		{
			LF_Debug.Error(LF_ATMTransferService.FEATURE, "transfer page refused: no machine");
			return null;
		}

		LF_ATMTransferPage page = new LF_ATMTransferPage();
		page.m_MachineId = machine.GetID();
		page.m_Action = action;
		if (action)
		{
			page.m_sKeySound = action.GetKeySound();
			page.m_sAcceptSound = action.GetAcceptSound();
			page.m_sRefuseSound = action.GetRefuseSound();
		}

		if (!page.Build(machine))
			return null;

		s_Current = page;
		if (action)
			LF_UISound.Play(action.GetOpenSound());

		LF_Debug.Info(LF_ATMTransferService.FEATURE, "transfer page open");
		return page;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Current)
			return;

		LF_ATMTransferPage current = s_Current;
		s_Current = null;
		if (current.m_Screen)
			current.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! The number the local player tells someone to send money to.
	static string OwnNumber()
	{
		return LF_Utils.GetPlayerUID(SCR_PlayerController.GetLocalPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	//! The entry line: the typed amount.
	static string EntryText(int amount)
	{
		return WidgetManager.Translate("#LF-ATMTransfer_LineAmount", LF_MoneyFormat.FormatMoney(amount));
	}

	//------------------------------------------------------------------------------------------------
	//! The fee line for an amount: the fee and what leaves the account.
	static string FeeText(int amount)
	{
		int fee = LF_BankTransferRules.Fee(LF_BankTransferConfig.GetOrDefaults(), amount);
		return WidgetManager.Translate("#LF-ATMTransfer_LineFee", LF_MoneyFormat.FormatMoney(fee), LF_MoneyFormat.FormatMoney(amount + fee));
	}

	//------------------------------------------------------------------------------------------------
	//! The parts of the page, top to bottom.
	static array<ref LF_UIPart> BuildParts(string ownNumber)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", "#LF-ATMTransfer_Header"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "YourNumber", WidgetManager.Translate("#LF-ATMTransfer_LineOwn", ownNumber)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, TARGET, "#LF-ATMTransfer_FieldTarget"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Entry", EntryText(0)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Fee", FeeText(0)));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Status", "#LF-ATMTransfer_Prompt"));
		parts.Insert(LF_ATMTerminal.Keys("Keys1", "Key1Button,Key2Button,Key3Button", "1,2,3"));
		parts.Insert(LF_ATMTerminal.Keys("Keys2", "Key4Button,Key5Button,Key6Button", "4,5,6"));
		parts.Insert(LF_ATMTerminal.Keys("Keys3", "Key7Button,Key8Button,Key9Button", "7,8,9"));
		parts.Insert(LF_ATMTerminal.Keys("Keys4", CLEAR + ",Key0Button," + BACK, "#LF-ATM_KeyClear,0,#LF-ATM_KeyBack"));
		parts.Insert(LF_ATMTerminal.Keys("Actions", SEND + "," + BACK_TO_MENU, "#LF-ATMTransfer_KeySend,#LF-ATMTransfer_KeyMenu"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "CloseButton", "#LF-ATM_Exit"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Build(IEntity machine)
	{
		m_Screen = LF_UIScreen.Open(BuildParts(OwnNumber()), LF_UISkin.Terminal());
		if (!m_Screen)
		{
			LF_Debug.Error(LF_ATMTransferService.FEATURE, "transfer page failed to open");
			return false;
		}

		m_Screen.GetOnButton().Insert(OnButton);
		m_Screen.GetOnClosed().Insert(OnClosed);
		for (int digit = 0; digit <= 9; digit++)
		{
			m_Screen.BindHotkey("LF_Keypad" + digit.ToString(), "Key" + digit.ToString() + "Button");
		}

		m_Screen.BindHotkey(LF_UIBack.KEYPAD_BACK, BACK);
		m_Screen.BindHotkey("LF_KeypadClear", CLEAR);
		m_Screen.CloseOnDeath();
		m_Screen.CloseWhenUncontrolled();
		m_Screen.CloseBeyond(LF_ATMTerminal.CLOSE_DISTANCE, machine.GetID());
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
			SetStatus("#LF-ATMTransfer_Prompt");
			LF_UISound.Play(m_sKeySound);
			return;
		}

		if (name == SEND)
		{
			Confirm();
			return;
		}

		if (name == BACK_TO_MENU)
		{
			BackToMenu();
			return;
		}

		LF_Debug.Warn(LF_ATMTransferService.FEATURE, "transfer page ignored button " + name);
	}

	//------------------------------------------------------------------------------------------------
	//! Back to the terminal's main screen at the same machine.
	protected void BackToMenu()
	{
		IEntity machine = null;
		if (GetGame() && GetGame().GetWorld())
			machine = GetGame().GetWorld().FindEntityByID(m_MachineId);

		LF_ATMAction action = m_Action;
		Hide();
		LF_ATMTerminal.Open(machine, LF_Utils.GetLocalCharacter(), action);
	}

	//------------------------------------------------------------------------------------------------
	//! Sends the request. The number goes as typed; the server trims it and decides everything.
	protected void Confirm()
	{
		if (m_bWaiting)
		{
			LF_Debug.Info(LF_ATMTransferService.FEATURE, "request ignored: the machine is still working");
			return;
		}

		LF_UISound.Play(m_sKeySound);
		if (!LF_ATMManager.IsValidAmount(m_iAmount))
		{
			Refuse(LF_EATMTransferReply.INVALID_AMOUNT);
			return;
		}

		string number = LF_ATMTransferService.CleanNumber(m_Screen.GetFieldText(TARGET));
		if (number.IsEmpty())
		{
			Refuse(LF_EATMTransferReply.UNKNOWN_ACCOUNT);
			return;
		}

		LF_CharacterATMComponent atm = LF_Component<LF_CharacterATMComponent>.Find(LF_Utils.GetLocalCharacter());
		if (!atm)
		{
			LF_Debug.Error(LF_ATMTransferService.FEATURE, "request failed: no ATM component on the controlled character");
			Refuse(LF_EATMTransferReply.NO_ACCOUNT);
			return;
		}

		m_bWaiting = true;
		SetStatus("#LF-ATM_Working");
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(ReplyTimedOut, REPLY_TIMEOUT_MS, false);

		atm.RequestTransfer(number, m_iAmount);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReplyTimedOut()
	{
		if (!m_bWaiting)
			return;

		m_bWaiting = false;
		LF_Debug.Warn(LF_ATMTransferService.FEATURE, "no reply from the server within " + REPLY_TIMEOUT_MS.ToString() + " ms");
		SetStatus("#LF-ATM_NoReply");
		LF_UISound.Play(m_sRefuseSound);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server's answer. The toast shows even when the page was closed meanwhile.
	static void OnResult(LF_EATMTransferReply reply, int amount, int fee, string last4, int balance)
	{
		if (reply == LF_EATMTransferReply.SENT)
			ShowToast(amount, last4);

		if (!s_Current)
		{
			LF_Debug.Info(LF_ATMTransferService.FEATURE, "reply arrived after the page closed");
			return;
		}

		s_Current.ApplyResult(reply, amount, fee, last4, balance);
	}

	//------------------------------------------------------------------------------------------------
	//! The toast: the amount and the last four characters of the recipient's number.
	protected static void ShowToast(int amount, string last4)
	{
		LF_NotificationManagerComponent notifications = LF_NotificationManagerComponent.GetInstance();
		if (!notifications)
		{
			LF_Debug.Warn(LF_ATMTransferService.FEATURE, "no notification manager for the transfer toast");
			return;
		}

		string message = WidgetManager.Translate("#LF-ATMTransfer_Sent", LF_MoneyFormat.FormatMoney(amount), last4);
		notifications.ShowNotificationToast("#LF-ATMTransfer_ToastTitle", message, TOAST_SECONDS, LF_ENotificationType.SUCCESS);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyResult(LF_EATMTransferReply reply, int amount, int fee, string last4, int balance)
	{
		m_bWaiting = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ReplyTimedOut);

		if (reply != LF_EATMTransferReply.SENT)
		{
			Refuse(reply);
			return;
		}

		SetAmount(0);
		if (m_Screen)
			m_Screen.SetFieldText(TARGET, "");

		SetStatus(WidgetManager.Translate("#LF-ATMTransfer_Done", LF_MoneyFormat.FormatMoney(amount), last4, LF_MoneyFormat.FormatMoney(balance)));
		LF_UISound.Play(m_sAcceptSound);
	}

	//------------------------------------------------------------------------------------------------
	protected void Refuse(LF_EATMTransferReply reply)
	{
		SetStatus(LF_ATMTransferService.Key(reply));
		LF_UISound.Play(m_sRefuseSound);
		LF_Debug.Info(LF_ATMTransferService.FEATURE, "transfer page shows refusal " + typename.EnumToString(LF_EATMTransferReply, reply));
	}

	//------------------------------------------------------------------------------------------------
	protected void SetAmount(int amount)
	{
		m_iAmount = amount;
		if (!m_Screen)
			return;

		m_Screen.SetLine("Entry", EntryText(amount));
		m_Screen.SetLine("Fee", FeeText(amount));
	}

	//------------------------------------------------------------------------------------------------
	protected void SetStatus(string text)
	{
		if (m_Screen)
			m_Screen.SetLine("Status", text);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the listeners and timers if the instance goes away without a close.
	void ~LF_ATMTransferPage()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ReplyTimedOut);

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

		LF_Debug.Info(LF_ATMTransferService.FEATURE, "transfer page closed");
	}

	//------------------------------------------------------------------------------------------------
	int GetAmount()
	{
		return m_iAmount;
	}
}
