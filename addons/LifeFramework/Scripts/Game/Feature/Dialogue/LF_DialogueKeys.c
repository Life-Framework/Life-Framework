//------------------------------------------------------------------------------------------------
//! The reply numbers of the conversation scene (pure rules, no widgets). The replies a node
//! offers are rows 0..n-1, shown as 1..n; one more row, always last, is Goodbye, shown as 0, which
//! the scene adds itself and which leaves the talk. A digit key, a focus step and the label all
//! come from here, so the screen and the keys cannot disagree.
class LF_DialogueKeys
{
	//! The digit that leaves the conversation.
	static const int GOODBYE_DIGIT = 0; // lf-fixed: the Goodbye row is always "0"

	//------------------------------------------------------------------------------------------------
	//! Rows on screen for `replies` offered replies: the replies and the Goodbye row.
	static int RowCount(int replies)
	{
		return replies + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! The row a digit key picks: 1..replies the reply rows, 0 the Goodbye row (index `replies`),
	//! anything else (a digit with no row on screen) -1.
	static int RowOfDigit(int digit, int replies)
	{
		if (digit == GOODBYE_DIGIT)
			return replies;

		if (digit >= 1 && digit <= replies)
			return digit - 1;

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The number printed in front of a row.
	static int DigitOfRow(int row, int replies)
	{
		if (row == replies)
			return GOODBYE_DIGIT;

		return row + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the row is the Goodbye row.
	static bool IsGoodbye(int row, int replies)
	{
		return row == replies;
	}

	//------------------------------------------------------------------------------------------------
	//! The focus after a step (+1 down, -1 up) over the replies and the Goodbye row, wrapping.
	static int Step(int focus, int step, int replies)
	{
		int rows = RowCount(replies);
		return (focus + step + rows) % rows;
	}
}
