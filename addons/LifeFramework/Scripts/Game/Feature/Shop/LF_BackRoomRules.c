enum LF_EBackRoomRefusal
{
	NONE,
	NOT_ALLOWED,
	NOT_A_BACK_ROOM,
	TOO_FAR,
	ALREADY_SHUT
}

//------------------------------------------------------------------------------------------------
//! Pure rules for an officer's raid on a back room (#600): who may search, from how far, and for
//! how long a raided room stays shut. The shop component applies them on the server.
class LF_BackRoomRules
{
	//! The reach and the shut time are LF_ShopLimitsConfig fields.

	//------------------------------------------------------------------------------------------------
	static bool IsClosed(int closedUntilMs, int nowMs)
	{
		return nowMs < closedUntilMs;
	}

	//------------------------------------------------------------------------------------------------
	static int ClosedUntil(int nowMs)
	{
		return nowMs + LF_ShopLimitsConfig.BackRoomShutMs();
	}

	//------------------------------------------------------------------------------------------------
	static LF_EBackRoomRefusal Check(bool onDuty, bool isBackRoom, float distance, int closedUntilMs, int nowMs)
	{
		if (!onDuty)
			return LF_EBackRoomRefusal.NOT_ALLOWED;

		if (!isBackRoom)
			return LF_EBackRoomRefusal.NOT_A_BACK_ROOM;

		if (distance > LF_ShopLimitsConfig.BackRoomReach())
			return LF_EBackRoomRefusal.TOO_FAR;

		if (IsClosed(closedUntilMs, nowMs))
			return LF_EBackRoomRefusal.ALREADY_SHUT;

		return LF_EBackRoomRefusal.NONE;
	}
}
