// LF_UIFocus.c — pad movement through the focusable parts on a sheet.
// MenuDown steps forward, MenuUp steps back, and either end wraps.

class LF_UIFocus
{
	//------------------------------------------------------------------------------------------------
	static int Step(int count, int current, int direction)
	{
		if (count <= 0)
			return -1;

		if (current < 0 || current >= count)
		{
			if (direction < 0)
				return count - 1;

			return 0;
		}

		int next = current + direction;
		if (next < 0)
			return count - 1;

		if (next >= count)
			return 0;

		return next;
	}
}
