// LF_UIClosePolicy.c — when a sheet at a counter should go away.
// Pure facts in, a reason out. The screen measures the player and the anchor.

class LF_UIClosePolicy
{
	static const int STAY = 0;
	static const int DEATH = 1;
	static const int LOST = 2;
	static const int ANCHOR = 3;
	static const int DISTANCE = 4;

	//------------------------------------------------------------------------------------------------
	static int Reason(bool checkDeath, bool checkLost, bool checkDistance, bool dead, bool hasCharacter, bool anchorMissing, float maxDistance, float distance)
	{
		if (checkDeath && dead)
			return DEATH;

		if (checkLost && !hasCharacter)
			return LOST;

		if (checkDistance && !hasCharacter)
			return LOST;

		if (checkDistance && anchorMissing)
			return ANCHOR;

		if (checkDistance && distance > maxDistance)
			return DISTANCE;

		return STAY;
	}

	//------------------------------------------------------------------------------------------------
	static string Name(int reason)
	{
		if (reason == DEATH)
			return "dead";

		if (reason == LOST)
			return "no character";

		if (reason == ANCHOR)
			return "anchor gone";

		if (reason == DISTANCE)
			return "out of range";

		return "stay";
	}
}
