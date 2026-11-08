// LF_UIQuantity.c — one stack shown as two faces.
// Take is clamped to 1 .. total-1. Keep is what stays. A stack of one cannot split.

class LF_UIQuantity
{
	//------------------------------------------------------------------------------------------------
	static bool Split(int total, int requestedTake, out int keep, out int take)
	{
		keep = 0;
		take = 0;

		if (total <= 1)
		{
			if (total > 0)
				keep = total;

			return false;
		}

		take = requestedTake;
		if (take < 1)
			take = 1;

		if (take > total - 1)
			take = total - 1;

		keep = total - take;
		return true;
	}
}
