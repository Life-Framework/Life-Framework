// LF_ATMQuick.c — the cash machine's in-world menu, one part per job: the cash slot pays out, the
// deposit drawer takes cash in, the screen and keypad show the balance. Looking at a part lists its
// fixed amounts; the scroll wheel (or the pad) picks one. No window opens. Pure: the suite drives it.

enum LF_EATMQuickKind
{
	WITHDRAW,
	DEPOSIT,
	BALANCE
}

class LF_ATMQuick
{
	//! A preset of ALL takes everything there is: the whole balance, or every note carried.
	static const int ALL = -1;
	//! Balance and cash read UNKNOWN until the server has answered once.
	static const int UNKNOWN = -1;

	//------------------------------------------------------------------------------------------------
	//! The amount a line moves. A fixed preset is itself; ALL is the balance (withdraw) or the cash
	//! carried (deposit), capped at one transaction. \return 0 when there is nothing to move or the
	//! figure is not known yet.
	static int Resolve(LF_EATMQuickKind kind, int preset, int balance, int cash)
	{
		if (kind == LF_EATMQuickKind.BALANCE)
			return 0;

		if (preset > 0)
			return preset;

		if (preset != ALL)
			return 0;

		int pool = cash;
		if (kind == LF_EATMQuickKind.WITHDRAW)
			pool = balance;

		if (pool <= 0)
			return 0;

		return LF_Utils.MinInt(pool, LF_LedgerConfig.AtmMaxTransaction());
	}

	//------------------------------------------------------------------------------------------------
	//! Why a line is greyed out, from what this player last heard from the server. An unknown figure
	//! never greys a line: the server decides and says why.
	static LF_EATMRefusal Check(LF_EATMQuickKind kind, int preset, int balance, int cash)
	{
		if (kind == LF_EATMQuickKind.BALANCE)
			return LF_EATMRefusal.NONE;

		bool known = balance != UNKNOWN;
		if (kind == LF_EATMQuickKind.DEPOSIT)
			known = cash != UNKNOWN;

		int amount = Resolve(kind, preset, balance, cash);
		if (!known)
		{
			if (preset > 0 && !LF_ATMManager.IsValidAmount(preset))
				return LF_EATMRefusal.INVALID_AMOUNT;

			return LF_EATMRefusal.NONE;
		}

		if (kind == LF_EATMQuickKind.WITHDRAW)
		{
			if (amount <= 0 || amount > balance)
				return LF_EATMRefusal.NOT_ENOUGH_BALANCE;
		}
		else if (amount <= 0 || amount > cash)
		{
			return LF_EATMRefusal.NOT_ENOUGH_CASH;
		}

		if (!LF_ATMManager.IsValidAmount(amount))
			return LF_EATMRefusal.INVALID_AMOUNT;

		return LF_EATMRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The short reason shown beside a greyed line.
	static string ReasonKey(LF_EATMRefusal reason)
	{
		if (reason == LF_EATMRefusal.NOT_ENOUGH_BALANCE)
			return "#LF-ATM_QuickNoBalance";

		if (reason == LF_EATMRefusal.NOT_ENOUGH_CASH)
			return "#LF-ATM_QuickNoCash";

		if (reason == LF_EATMRefusal.CANNOT_ACT)
			return "#LF-ATM_QuickCannotAct";

		if (reason == LF_EATMRefusal.INVALID_AMOUNT)
			return "#LF-ATM_QuickOverLimit";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The line's text, untranslated: a key and the amount it fills in (empty when it has none).
	static string LabelKey(LF_EATMQuickKind kind, int preset, int balance, int cash, out string arg)
	{
		arg = string.Empty;
		if (kind == LF_EATMQuickKind.BALANCE)
			return "#LF-ATM_QuickBalance";

		if (preset > 0)
		{
			arg = LF_MoneyFormat.FormatMoney(preset);
			if (kind == LF_EATMQuickKind.WITHDRAW)
				return "#LF-ATM_QuickWithdraw";

			return "#LF-ATM_QuickDeposit";
		}

		int amount = Resolve(kind, preset, balance, cash);
		bool known = balance != UNKNOWN;
		if (kind == LF_EATMQuickKind.DEPOSIT)
			known = cash != UNKNOWN;

		if (known)
			arg = LF_MoneyFormat.FormatMoney(amount);

		if (kind == LF_EATMQuickKind.WITHDRAW)
		{
			if (known)
				return "#LF-ATM_QuickWithdrawAll";

			return "#LF-ATM_QuickWithdrawAllPlain";
		}

		if (known)
			return "#LF-ATM_QuickDepositAll";

		return "#LF-ATM_QuickDepositAllPlain";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character who cannot use their hands is refused before any money moves. Pure on its
	//! inputs so the suite proves each case without cuffing anyone.
	static LF_EATMRefusal ActorRefusal(bool dead, bool downed, bool restrained)
	{
		if (dead || downed || restrained)
			return LF_EATMRefusal.CANNOT_ACT;

		return LF_EATMRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The toast after a transaction, untranslated: a key and its two figures.
	static string ToastKey(bool deposit, LF_EATMRefusal reason)
	{
		if (reason != LF_EATMRefusal.NONE)
			return LF_ATMRefusal.Key(reason);

		if (deposit)
			return "#LF-ATM_ToastDeposited";

		return "#LF-ATM_ToastWithdrew";
	}
}
