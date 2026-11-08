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
	//------------------------------------------------------------------------------------------------
	//! Where focus lands when a sheet opens: the first entry that is not a text field, so a pad
	//! can press MenuSelect at once. Falls back to the first entry; -1 when there is nothing.
	static int Initial(array<bool> isField)
	{
		if (!isField || isField.Count() == 0)
			return -1;

		for (int i = 0; i < isField.Count(); i++)
		{
			if (!isField.Get(i))
				return i;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Vertical scroll fraction (0 top, 1 bottom) that keeps row index of count rows in view.
	static float ScrollFraction(int index, int count)
	{
		if (count <= 1 || index <= 0)
			return 0;

		if (index >= count - 1)
			return 1;

		float position = index;
		float last = count - 1;
		return position / last;
	}

	//------------------------------------------------------------------------------------------------
	//! Index at which an entry with this order key goes so the keys stay ascending.
	//! Equal keys keep insertion order (the new entry goes after them).
	static int InsertIndex(array<int> keys, int key)
	{
		if (!keys)
			return 0;

		for (int i = 0; i < keys.Count(); i++)
		{
			if (keys.Get(i) > key)
				return i;
		}

		return keys.Count();
	}
}
