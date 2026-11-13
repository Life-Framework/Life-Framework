//------------------------------------------------------------------------------------------------
//! The identity card registry (#902). Server only: every card ever printed is one LF_PhotoIdRow,
//! numbered in order. The card item carries its number in an LF_SerialComponent; everything
//! printed on it (the name, the age, the date of issue, the photo) is read back from the row. A
//! holder's newest row is their valid card; an older one is void (the card was replaced).
//! Persisted as the "idcards" payload (save version 30).
//------------------------------------------------------------------------------------------------
class LF_PhotoIdRow
{
	//! Positional in a binary save: never add, remove or reorder a field. A new field is a new payload.
	string m_sNumber;
	string m_sHolderId;
	string m_sFirstName;
	string m_sLastName;
	int m_iAge;
	string m_sIssued;
	string m_sLook;

	//------------------------------------------------------------------------------------------------
	static LF_PhotoIdRow Create(string number, string holderId, string firstName, string lastName, int age, string issued, string look)
	{
		LF_PhotoIdRow row = new LF_PhotoIdRow();
		row.m_sNumber = number;
		row.m_sHolderId = holderId;
		row.m_sFirstName = firstName;
		row.m_sLastName = lastName;
		row.m_iAge = age;
		row.m_sIssued = issued;
		row.m_sLook = look;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Why the clerk will not print a replacement.
enum LF_EIdReplace
{
	OK,
	//! The player was never issued a card: the arrival chain hands out the first one.
	NEVER_ISSUED,
	//! The player still carries their valid card.
	STILL_CARRIED,
	NOT_ENOUGH_CASH
}

//------------------------------------------------------------------------------------------------
//! What a spawn does about the card.
enum LF_EIdSpawn
{
	NOTHING,
	//! First card for a resident who earned one before the registry existed (or a grant-at-spawn mode).
	FIRST_CARD,
	//! A grant-at-spawn mode (no clerk) replaces a lost card for free.
	FREE_REPLACEMENT
}

//------------------------------------------------------------------------------------------------
class LF_PhotoIdCard
{
	static const string FEATURE = "IdCard";
	//! Printed before the running number: "EV 000042".
	static const string NUMBER_PREFIX = "EV ";
	static const int NUMBER_DIGITS = 6;	// lf-fixed: the printed field's width

	protected static ref array<ref LF_PhotoIdRow> s_aRows;
	protected static int s_iLastSequence;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_PhotoIdRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_PhotoIdRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static string FormatNumber(int sequence)
	{
		return NUMBER_PREFIX + sequence.ToString(NUMBER_DIGITS);
	}

	//------------------------------------------------------------------------------------------------
	//! The running number in a printed card number; 0 for anything that is not one.
	static int SequenceOf(string number)
	{
		if (!number.StartsWith(NUMBER_PREFIX))
			return 0;

		string digits = number.Substring(NUMBER_PREFIX.Length(), number.Length() - NUMBER_PREFIX.Length());
		int sequence = digits.ToInt();
		if (sequence <= 0 || sequence.ToString(NUMBER_DIGITS) != digits)
			return 0;

		return sequence;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: records a newly printed card for the holder and returns its row. The holder's
	//! earlier cards become void.
	static LF_PhotoIdRow Issue(string holderId, string firstName, string lastName, int age, string issued, string look)
	{
		if (holderId.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "card not recorded: no holder id");
			return null;
		}

		s_iLastSequence++;
		LF_PhotoIdRow row = LF_PhotoIdRow.Create(FormatNumber(s_iLastSequence), holderId, firstName, lastName, age, issued, look);
		Rows().Insert(row);
		LF_Debug.Info(FEATURE, string.Format("card %1 recorded for character %2 (%3 %4, issued %5, look %6 chars)", row.m_sNumber, holderId, firstName, lastName, issued, look.Length()));
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! The holder's valid card, or null when none was ever issued.
	static LF_PhotoIdRow Current(string holderId)
	{
		if (holderId.IsEmpty() || !s_aRows)
			return null;

		for (int i = s_aRows.Count() - 1; i >= 0; i--)
		{
			if (s_aRows[i].m_sHolderId == holderId)
				return s_aRows[i];
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PhotoIdRow FindNumber(string number)
	{
		if (number.IsEmpty() || !s_aRows)
			return null;

		foreach (LF_PhotoIdRow row : s_aRows)
		{
			if (row.m_sNumber == number)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the number is its holder's newest card. A card replaced since is void.
	static bool IsValid(string number)
	{
		LF_PhotoIdRow row = FindNumber(number);
		if (!row)
			return false;

		return Current(row.m_sHolderId) == row;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		if (!s_aRows)
			return 0;

		return s_aRows.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The clerk's rule for a replacement.
	static LF_EIdReplace CheckReplace(bool wasIssued, bool carriesValid, int cashCarried, int fee)
	{
		if (!wasIssued)
			return LF_EIdReplace.NEVER_ISSUED;

		if (carriesValid)
			return LF_EIdReplace.STILL_CARRIED;

		if (cashCarried < fee)
			return LF_EIdReplace.NOT_ENOUGH_CASH;

		return LF_EIdReplace.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The spawn's rule. A lost card stays lost unless the mode has no clerk to replace it.
	static LF_EIdSpawn DecideSpawn(bool grantAtSpawn, bool wasIssued, bool carriesValid)
	{
		if (carriesValid)
			return LF_EIdSpawn.NOTHING;

		if (!wasIssued)
			return LF_EIdSpawn.FIRST_CARD;

		if (grantAtSpawn)
			return LF_EIdSpawn.FREE_REPLACEMENT;

		return LF_EIdSpawn.NOTHING;
	}

	//------------------------------------------------------------------------------------------------
	//! The "idcards" payload: every row, oldest first.
	static array<ref LF_PhotoIdRow> ExportAll()
	{
		array<ref LF_PhotoIdRow> rows = new array<ref LF_PhotoIdRow>();
		if (!s_aRows)
			return rows;

		foreach (LF_PhotoIdRow row : s_aRows)
		{
			rows.Insert(LF_PhotoIdRow.Create(row.m_sNumber, row.m_sHolderId, row.m_sFirstName, row.m_sLastName, row.m_iAge, row.m_sIssued, row.m_sLook));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the registry with the saved rows (idempotent). A row without a valid number or a
	//! holder is skipped; the next number follows the highest one loaded.
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_PhotoIdRow> rows)
	{
		s_aRows = new array<ref LF_PhotoIdRow>();
		s_iLastSequence = 0;
		if (!rows)
			return 0;

		foreach (LF_PhotoIdRow row : rows)
		{
			if (!row)
				continue;

			int sequence = SequenceOf(row.m_sNumber);
			if (sequence <= 0 || row.m_sHolderId.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, string.Format("saved card row skipped: number '%1' holder '%2'", row.m_sNumber, row.m_sHolderId));
				continue;
			}

			s_aRows.Insert(LF_PhotoIdRow.Create(row.m_sNumber, row.m_sHolderId, row.m_sFirstName, row.m_sLastName, row.m_iAge, row.m_sIssued, row.m_sLook));
			if (sequence > s_iLastSequence)
				s_iLastSequence = sequence;
		}

		return s_aRows.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The in-game date printed on a card: "14/06/1989".
	static string IssueDate()
	{
		string stamp = LF_HospitalBill.InGameDate();
		int space = stamp.IndexOf(" ");
		if (space > 0)
			return stamp.Substring(0, space);

		return stamp;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		s_aRows = null;
		s_iLastSequence = 0;
	}
}
