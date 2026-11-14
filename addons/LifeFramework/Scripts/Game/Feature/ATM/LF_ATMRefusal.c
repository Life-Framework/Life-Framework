// LF_ATMRefusal.c — why the cash machine said no. The server decides, the reply carries the
// reason, and the terminal prints it on its screen (no toast).

enum LF_EATMRefusal
{
	NONE,
	INVALID_AMOUNT,
	NOT_ENOUGH_CASH,
	NOT_ENOUGH_BALANCE,
	POCKETS_FULL,
	TOO_FAR,
	NO_ACCOUNT,
	SYSTEM_OFF
}

class LF_ATMRefusal
{
	//------------------------------------------------------------------------------------------------
	//! Screen line for a refusal; empty for NONE.
	static string Key(LF_EATMRefusal reason)
	{
		if (reason == LF_EATMRefusal.INVALID_AMOUNT)
			return "#LF-ATM_RefuseAmount";

		if (reason == LF_EATMRefusal.NOT_ENOUGH_CASH)
			return "#LF-ATM_RefuseCash";

		if (reason == LF_EATMRefusal.NOT_ENOUGH_BALANCE)
			return "#LF-ATM_RefuseBalance";

		if (reason == LF_EATMRefusal.POCKETS_FULL)
			return "#LF-ATM_RefusePockets";

		if (reason == LF_EATMRefusal.TOO_FAR)
			return "#LF-ATM_RefuseFar";

		if (reason == LF_EATMRefusal.NO_ACCOUNT)
			return "#LF-ATM_RefuseAccount";

		if (reason == LF_EATMRefusal.SYSTEM_OFF)
			return "#LF-Systems_Off";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! A reason that arrives over the wire as an int; anything unknown reads as no account.
	static LF_EATMRefusal FromWire(int reason)
	{
		if (reason < LF_EATMRefusal.NONE || reason > LF_EATMRefusal.SYSTEM_OFF)
			return LF_EATMRefusal.NO_ACCOUNT;

		return reason;
	}
}
