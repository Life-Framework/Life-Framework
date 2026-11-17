//------------------------------------------------------------------------------------------------
//! Persisted played seconds of one character (#2443): the "graceclock" payload (save version 48) of
//! LF_PersistenceComponentSerializer. Keyed by the character record key (LF_Utils.GetPlayerUID).
//!
//! Positional in a binary save: never add, remove or reorder a field. A new field is a new payload.
class LF_GraceClockRow
{
	string m_sRecordKey;
	int m_iSeconds;

	//------------------------------------------------------------------------------------------------
	static LF_GraceClockRow Create(string recordKey, int seconds)
	{
		LF_GraceClockRow row = new LF_GraceClockRow();
		row.m_sRecordKey = recordKey;
		row.m_iSeconds = seconds;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The grace clock of each character (#2443): seconds that character has been played, so a second
//! character on an old account gets its own grace (LF_Grace.Protects). Server only, never replicated.
//!
//! LF_Playtime.Settle credits the stretch it pays to the account to the account's active character
//! too (CharacterOf). The count stops at CAP_SECONDS, the longest grace the config allows, since
//! nothing reads past it. A save from before this payload seeds every existing character with its
//! account's playtime (SeedFromAccounts), so the upgrade gives no veteran a fresh grace.
class LF_GraceCharacterClock
{
	//! The longest grace LF_GraceConfig allows (MINUTES_MAX = 600) in seconds; a clock stops there.
	static const int CAP_SECONDS = 36000; // lf-fixed: mirrors LF_GraceConfig.MINUTES_MAX * 60, not a tunable

	//! Key: character record key. Value: seconds played, at most CAP_SECONDS.
	protected static ref map<string, int> s_mSeconds;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mSeconds = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, int> Rows()
	{
		if (!s_mSeconds)
			s_mSeconds = new map<string, int>();

		return s_mSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Seconds the character with this record key has been played (0 for a new one).
	static int Seconds(string recordKey)
	{
		if (!s_mSeconds || recordKey.IsEmpty())
			return 0;

		return s_mSeconds.Get(recordKey);
	}

	//------------------------------------------------------------------------------------------------
	//! The one write path: adds played seconds to a character, up to CAP_SECONDS.
	//! An empty key is a player with no character yet (character creation): nothing to count.
	//! \return False for an empty key or a non-positive amount.
	static bool Credit(string recordKey, int seconds)
	{
		if (recordKey.IsEmpty() || seconds <= 0)
			return false;

		int total = Rows().Get(recordKey) + seconds;
		if (total > CAP_SECONDS)
			total = CAP_SECONDS;

		Rows().Set(recordKey, total);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The record key of an account's active character, or the fallback when the account is not
	//! resident or has no character (a player who is leaving keeps the character of the last credit).
	static string CharacterOf(string accountUid, string fallback)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return fallback;

		LF_PlayerAccount account = accounts.GetFromCache(accountUid);
		if (!account || !account.HasCharacters())
			return fallback;

		return account.GetActiveCharacterKey();
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: every clock as a row. LF_Playtime.ExportAll (the "playtime" payload, written
	//! earlier in the same save) has already credited the online clocks up to now.
	static array<ref LF_GraceClockRow> ExportAll()
	{
		array<ref LF_GraceClockRow> rows = new array<ref LF_GraceClockRow>();
		if (!s_mSeconds)
			return rows;

		foreach (string recordKey, int seconds : s_mSeconds)
		{
			rows.Insert(LF_GraceClockRow.Create(recordKey, seconds));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the clocks with the saved ones (idempotent). Rows with no key or no time are skipped;
	//! duplicate keys keep the larger count.
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_GraceClockRow> rows)
	{
		s_mSeconds = new map<string, int>();
		if (!rows)
			return 0;

		int applied = 0;
		foreach (LF_GraceClockRow row : rows)
		{
			if (!row || row.m_sRecordKey.IsEmpty() || row.m_iSeconds <= 0)
			{
				LF_Debug.Warn(LF_Grace.FEATURE, "grace clock load skipped a row with no character or time");
				continue;
			}

			int seconds = row.m_iSeconds;
			if (seconds > CAP_SECONDS)
				seconds = CAP_SECONDS;

			if (s_mSeconds.Get(row.m_sRecordKey) < seconds)
				s_mSeconds.Set(row.m_sRecordKey, seconds);

			applied++;
		}

		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! Load of a save from before #2443 (no "graceclock" payload): every character the save holds is
	//! given its account's playtime, which is what the grace read then, so the upgrade neither starts
	//! nor extends anyone's grace. Characters created after it start at 0. Idempotent.
	//! \return Characters seeded.
	static int SeedFromAccounts()
	{
		s_mSeconds = new map<string, int>();
		int seeded = 0;
		array<ref LF_CharacterSlotRecord> slots = LF_PlayerAccountManager.ExportCharacterSlots();
		foreach (LF_CharacterSlotRecord slot : slots)
		{
			if (!slot || slot.m_sRecordKey.IsEmpty())
				continue;

			int seconds = LF_Playtime.Total(slot.m_sAccountUid);
			if (seconds <= 0)
				continue;

			if (seconds > CAP_SECONDS)
				seconds = CAP_SECONDS;

			s_mSeconds.Set(slot.m_sRecordKey, seconds);
			seeded++;
		}

		LF_Debug.Info(LF_Grace.FEATURE, string.Format("save has no grace clocks: %1 of %2 character(s) seeded from their account playtime", seeded, slots.Count()));
		return seeded;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_GraceCharacterClock's statics (registered at script load; Core/LF_StaticReset.c).
class LF_GraceCharacterClock_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_GraceCharacterClock_StaticResetHook());

	override string Owner()
	{
		return "LF_GraceCharacterClock";
	}

	override void Apply()
	{
		LF_GraceCharacterClock.ResetStatics();
	}
}
