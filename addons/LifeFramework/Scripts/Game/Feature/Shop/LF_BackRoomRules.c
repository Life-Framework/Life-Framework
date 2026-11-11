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
	static const float REACH = 4.0;
	//! A raided room stays shut this long (world ms).
	static const int SHUT_MS = 600000;

	//------------------------------------------------------------------------------------------------
	static bool IsClosed(int closedUntilMs, int nowMs)
	{
		return nowMs < closedUntilMs;
	}

	//------------------------------------------------------------------------------------------------
	static int ClosedUntil(int nowMs)
	{
		return nowMs + SHUT_MS;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EBackRoomRefusal Check(bool onDuty, bool isBackRoom, float distance, int closedUntilMs, int nowMs)
	{
		if (!onDuty)
			return LF_EBackRoomRefusal.NOT_ALLOWED;

		if (!isBackRoom)
			return LF_EBackRoomRefusal.NOT_A_BACK_ROOM;

		if (distance > REACH)
			return LF_EBackRoomRefusal.TOO_FAR;

		if (IsClosed(closedUntilMs, nowMs))
			return LF_EBackRoomRefusal.ALREADY_SHUT;

		return LF_EBackRoomRefusal.NONE;
	}
}
