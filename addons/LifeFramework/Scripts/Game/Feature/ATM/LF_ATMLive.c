// LF_ATMLive.c — what the cash machine's own screen shows the player standing at it: their bank
// balance and the cash they carry, in green on the CRT, and a short toast after each transaction.
// Client-only and local: it is drawn over the machine's screen on this player's HUD, so nobody else
// sees it. The figures come from the server (LF_CharacterATMComponent replies); every transaction
// is still decided there. No window opens: the in-world action lines (LF_ATMQuickAction) send.

class LF_ATMLive
{
	static const ResourceName LAYOUT = "{8E3C51A7D2F04B69}UI/Layouts/HUD/LF_ATMScreen.layout";
	static const int TICK_MS = 33;	// lf-fixed: follows the camera smoothly
	//! Ask the server again this often while the screen shows, so cash picked up elsewhere appears.
	static const int REFRESH_MS = 4000;	// lf-fixed: one small reliable RPC, only while at a machine
	static const float TOAST_SECONDS = 4;	// lf-fixed: a short read, the same as other money toasts

	//! The CRT glass in the machine's own space (tools/assets/atm-wall/kit.json "screen").
	static const vector SCREEN_CENTRE = "0 1.27 0.094";
	static const float SCREEN_HALF_W = 0.13;
	static const float SCREEN_HALF_H = 0.1;
	static const float PANEL_COVER = 1.08;	// lf-fixed: the lit panel overlaps the glass edge so the printed menu cannot show round it

	protected static int s_iBalance = LF_ATMQuick.UNKNOWN;
	protected static int s_iCash = LF_ATMQuick.UNKNOWN;
	protected static bool s_bWaiting;
	protected static bool s_bToastFigures;
	protected static string s_sStatus;
	protected static EntityID s_MachineId = EntityID.INVALID;
	protected static int s_iLastAskMs;

	protected static Widget s_wRoot;
	protected static Widget s_wPanel;
	protected static TextWidget s_wBalance;
	protected static TextWidget s_wCash;
	protected static TextWidget s_wStatus;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	static int Balance()
	{
		return s_iBalance;
	}

	//------------------------------------------------------------------------------------------------
	static int Cash()
	{
		return s_iCash;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsWaiting()
	{
		return s_bWaiting;
	}

	//------------------------------------------------------------------------------------------------
	//! "shown", "hidden" (panel built but off screen or covered), "none": for the harness and logs.
	static string State()
	{
		if (!s_wRoot)
			return "none";

		if (s_wPanel && s_wPanel.IsVisible())
			return "shown";

		return "hidden";
	}

	//------------------------------------------------------------------------------------------------
	//! The local player is at machine (an ATM line is being offered): light its screen.
	static void Near(IEntity machine)
	{
		if (!machine || !GetGame() || RplSession.Mode() == RplMode.Dedicated)
			return;

		if (s_wRoot && s_MachineId == machine.GetID())
			return;

		Show(machine);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Show(IEntity machine)
	{
		Remove();
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		s_wRoot = workspace.CreateWidgets(LAYOUT);
		if (!s_wRoot)
		{
			LF_Debug.Error("ATM", "the ATM screen layout did not load: " + LAYOUT);
			return;
		}

		s_wPanel = s_wRoot.FindAnyWidget("Panel");
		s_wBalance = TextWidget.Cast(s_wRoot.FindAnyWidget("Balance"));
		s_wCash = TextWidget.Cast(s_wRoot.FindAnyWidget("Cash"));
		s_wStatus = TextWidget.Cast(s_wRoot.FindAnyWidget("Status"));
		s_MachineId = machine.GetID();
		s_sStatus = "#LF-ATM_ScreenHello";
		if (GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
			s_bTicking = true;
		}

		LF_Debug.Info("ATM", "screen lit for the local player");
		AskFigures();
		Refresh();
		Tick();
	}

	//------------------------------------------------------------------------------------------------
	static void Remove()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bTicking = false;
		if (s_wRoot)
			s_wRoot.RemoveFromHierarchy();

		s_wRoot = null;
		s_wPanel = null;
		s_wBalance = null;
		s_wCash = null;
		s_wStatus = null;
		s_MachineId = EntityID.INVALID;
	}

	//------------------------------------------------------------------------------------------------
	//! World-end reset.
	static void ResetStatics()
	{
		Remove();
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ReplyTimedOut);

		s_iBalance = LF_ATMQuick.UNKNOWN;
		s_iCash = LF_ATMQuick.UNKNOWN;
		s_bWaiting = false;
		s_bToastFigures = false;
		s_sStatus = string.Empty;
		s_iLastAskMs = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_CharacterATMComponent LocalAtm()
	{
		LF_CharacterATMComponent atm = LF_Component<LF_CharacterATMComponent>.Find(LF_Utils.GetLocalCharacter());
		if (!atm)
			LF_Debug.Error("ATM", "no LF_CharacterATMComponent on the controlled character");

		return atm;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AskFigures()
	{
		LF_CharacterATMComponent atm = LocalAtm();
		if (!atm)
			return;

		s_iLastAskMs = System.GetTickCount();
		atm.RequestBalance();
	}

	//------------------------------------------------------------------------------------------------
	//! "Check balance": ask the server and toast the answer.
	static void CheckBalance(IEntity machine)
	{
		Near(machine);
		s_bToastFigures = true;
		AskFigures();
	}

	//------------------------------------------------------------------------------------------------
	//! Sends one withdrawal or deposit. One at a time: a second line picked before the answer is
	//! ignored, so a fast double scroll-and-press cannot send twice.
	static bool Send(LF_EATMQuickKind kind, int amount, IEntity machine)
	{
		if (s_bWaiting)
		{
			LF_Debug.Info("ATM", "quick request ignored: the machine is still working");
			return false;
		}

		if (kind == LF_EATMQuickKind.BALANCE || !LF_ATMManager.IsValidAmount(amount))
		{
			LF_Debug.Warn("ATM", string.Format("quick request not sent: kind %1 amount %2", typename.EnumToString(LF_EATMQuickKind, kind), amount));
			return false;
		}

		LF_CharacterATMComponent atm = LocalAtm();
		if (!atm)
			return false;

		Near(machine);
		s_bWaiting = true;
		s_sStatus = "#LF-ATM_Working";
		Refresh();
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(ReplyTimedOut, LF_ATMTerminal.REPLY_TIMEOUT_MS, false);

		LF_Debug.Info("ATM", string.Format("quick %1 %2 sent", typename.EnumToString(LF_EATMQuickKind, kind), amount));
		if (kind == LF_EATMQuickKind.DEPOSIT)
			atm.RequestDeposit(amount);
		else
			atm.RequestWithdraw(amount);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReplyTimedOut()
	{
		if (!s_bWaiting)
			return;

		s_bWaiting = false;
		s_sStatus = "#LF-ATM_NoReply";
		LF_Debug.Warn("ATM", "quick request: no reply within " + LF_ATMTerminal.REPLY_TIMEOUT_MS.ToString() + " ms");
		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server's figures (the screen asked, or the terminal opened).
	static void OnFigures(int balance, int cash)
	{
		s_iBalance = balance;
		s_iCash = cash;
		if (s_bToastFigures)
		{
			s_bToastFigures = false;
			Toast(WidgetManager.Translate("#LF-ATM_ToastBalance", LF_MoneyFormat.FormatMoney(balance), LF_MoneyFormat.FormatMoney(cash)), LF_ENotificationType.INFO);
		}

		Refresh();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server's answer to a withdrawal or deposit, from this screen or the terminal.
	static void OnResult(bool deposit, LF_EATMRefusal reason, int amount, int balance, int cash)
	{
		bool mine = s_bWaiting;
		s_bWaiting = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(ReplyTimedOut);

		s_iBalance = balance;
		s_iCash = cash;
		string key = LF_ATMQuick.ToastKey(deposit, reason);
		if (reason == LF_EATMRefusal.NONE)
		{
			if (deposit)
				s_sStatus = WidgetManager.Translate("#LF-ATM_Deposited", LF_MoneyFormat.FormatMoney(amount));
			else
				s_sStatus = WidgetManager.Translate("#LF-ATM_Dispensed", LF_MoneyFormat.FormatMoney(amount));
		}
		else
		{
			s_sStatus = key;
		}

		Refresh();
		// The terminal prints its own answer; the toast is for the in-world lines.
		if (!mine)
			return;

		if (reason == LF_EATMRefusal.NONE)
			Toast(LF_Terms.Text(key, LF_MoneyFormat.FormatMoney(amount), LF_MoneyFormat.FormatMoney(balance)), LF_ENotificationType.SUCCESS);
		else
			Toast(LF_Terms.Text(key), LF_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Toast(string text, LF_ENotificationType type)
	{
		LF_NotificationManagerComponent manager = LF_NotificationManagerComponent.GetInstance();
		if (!manager)
		{
			LF_Debug.Warn("ATM", "no toast: no notification manager");
			return;
		}

		manager.ShowNotificationToast("#EL-ATM_Title", text, TOAST_SECONDS, type);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the figures and the status line onto the screen.
	protected static void Refresh()
	{
		if (!s_wRoot)
			return;

		string unknown = WidgetManager.Translate("#LF-ATM_Unknown");
		string balance = unknown;
		if (s_iBalance != LF_ATMQuick.UNKNOWN)
			balance = LF_MoneyFormat.FormatMoney(s_iBalance);

		string cash = unknown;
		if (s_iCash != LF_ATMQuick.UNKNOWN)
			cash = LF_MoneyFormat.FormatMoney(s_iCash);

		if (s_wBalance)
			s_wBalance.SetText(WidgetManager.Translate("#LF-ATM_ScreenBank", balance));

		if (s_wCash)
			s_wCash.SetText(WidgetManager.Translate("#LF-ATM_ScreenCash", cash));

		if (s_wStatus)
			s_wStatus.SetText(WidgetManager.Translate(s_sStatus));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		IEntity me = LF_Utils.GetLocalCharacter();
		IEntity machine = null;
		if (GetGame() && GetGame().GetWorld())
			machine = GetGame().GetWorld().FindEntityByID(s_MachineId);

		if (!me || !machine || vector.Distance(me.GetOrigin(), machine.GetOrigin()) > LF_ATMTerminal.CLOSE_DISTANCE || !LF_Systems.IsEnabled(LF_ATMAction.SYSTEM))
		{
			LF_Debug.Info("ATM", "screen dark: the player left the machine");
			Remove();
			return;
		}

		if (!s_bWaiting && System.GetTickCount() - s_iLastAskMs > REFRESH_MS)
			AskFigures();

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || !s_wPanel)
			return;

		// Dark behind the machine and under a full screen (the terminal or the transfer page).
		vector centre = machine.CoordToParent(SCREEN_CENTRE);
		vector eye = me.GetOrigin() + "0 1.6 0";
		bool facing = vector.Dot(machine.GetTransformAxis(2), eye - centre) > 0;
		if (!facing || LF_ATMTerminal.Current() || LF_UIScreen.IsActionPromptSuppressed())
		{
			s_wPanel.SetVisible(false);
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		vector left = workspace.ProjWorldToScreen(machine.CoordToParent(SCREEN_CENTRE - Vector(SCREEN_HALF_W, 0, 0)), world);
		vector right = workspace.ProjWorldToScreen(machine.CoordToParent(SCREEN_CENTRE + Vector(SCREEN_HALF_W, 0, 0)), world);
		vector top = workspace.ProjWorldToScreen(machine.CoordToParent(SCREEN_CENTRE + Vector(0, SCREEN_HALF_H, 0)), world);
		vector bottom = workspace.ProjWorldToScreen(machine.CoordToParent(SCREEN_CENTRE - Vector(0, SCREEN_HALF_H, 0)), world);
		vector mid = workspace.ProjWorldToScreen(centre, world);
		if (mid[2] <= 0)
		{
			s_wPanel.SetVisible(false);
			return;
		}

		float w = Math.AbsFloat(right[0] - left[0]);
		float h = Math.AbsFloat(bottom[1] - top[1]);
		s_wPanel.SetVisible(true);
		// A little larger than the glass and fully opaque: the machine's printed menu (WITHDRAW, BALANCE, $20...)
		// no longer ghosts through behind the live figures (first-2-hours review C8).
		float cover = w * PANEL_COVER;
		float coverH = h * PANEL_COVER;
		FrameSlot.SetPos(s_wPanel, mid[0] - cover * 0.5, mid[1] - coverH * 0.5);
		FrameSlot.SetSize(s_wPanel, cover, coverH);
		int font = Math.Round(h * 0.15);
		if (font < 6)
			font = 6;

		if (s_wBalance)
			s_wBalance.SetExactFontSize(font);

		if (s_wCash)
			s_wCash.SetExactFontSize(font);

		if (s_wStatus)
			s_wStatus.SetExactFontSize(Math.Round(font * 0.7));
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ATMLive's statics (LF_StaticReset finds it by reflection).
class LF_ATMLive_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ATMLive";
	}

	override void Apply()
	{
		LF_ATMLive.ResetStatics();
	}
}
