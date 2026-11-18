//------------------------------------------------------------------------------------------------
//! What one wash of marked notes yields (#2852): the house cut, the batch size and the per-character
//! daily cap, as pure logic with no entity or inventory. The day total is session-only: a restart
//! forgives the day.
class LF_LaunderQuote
{
	//! Marked notes this wash takes (batch and daily cap applied).
	int m_iAccepted;
	//! Clean cash the character gets.
	int m_iClean;
	//! Marked notes that stay with the character.
	int m_iLeftMarked;
	//! Empty when something is accepted, else the refusal (`daily_cap`, `nothing`).
	string m_sReason;
}

//------------------------------------------------------------------------------------------------
class LF_LaunderDay
{
	int m_iWindowStart;
	int m_iTotal;
}

//------------------------------------------------------------------------------------------------
class LF_LaunderRules
{
	static const int DAY_SECONDS = 86400;
	static const string REASON_DAILY_CAP = "daily_cap";
	static const string REASON_NOTHING = "nothing";

	protected static ref map<string, ref LF_LaunderDay> s_Days = new map<string, ref LF_LaunderDay>();

	//------------------------------------------------------------------------------------------------
	//! Clean cash for `marked` notes at a cut of `cutPct` percent. Integer maths: the cut rounds down
	//! and the house keeps it, so 1000 at 30 % yields 700.
	static int CleanFor(int marked, int cutPct)
	{
		if (marked <= 0)
			return 0;

		int cutPctSafe = LF_Utils.ClampInt(cutPct, 0, 100);
		int cut = (marked * cutPctSafe) / 100;
		return marked - cut;
	}

	//------------------------------------------------------------------------------------------------
	//! Marked notes `key` has already washed in the window that covers `nowSec` (0 once it rolled over).
	static int WashedToday(string key, int nowSec)
	{
		LF_LaunderDay day = s_Days.Get(key);
		if (!day)
			return 0;

		if (nowSec >= day.m_iWindowStart + DAY_SECONDS)
			return 0;

		return day.m_iTotal;
	}

	//------------------------------------------------------------------------------------------------
	//! What a wash of `marked` notes yields for `key` at `nowSec`. Read only: call Record with the
	//! accepted amount once the notes are taken. A batch above batchMax is cut to it; a character at
	//! the daily cap gets 0 and the reason `daily_cap`; the cap's remainder trims a batch.
	static LF_LaunderQuote Quote(string key, int marked, int nowSec, int cutPct, int batchMax, int dailyCap)
	{
		LF_LaunderQuote quote = new LF_LaunderQuote();
		quote.m_sReason = string.Empty;
		if (marked <= 0)
		{
			quote.m_sReason = REASON_NOTHING;
			return quote;
		}

		int room = LF_Utils.MaxInt(0, dailyCap - WashedToday(key, nowSec));
		if (room <= 0)
		{
			quote.m_iLeftMarked = marked;
			quote.m_sReason = REASON_DAILY_CAP;
			return quote;
		}

		int accepted = LF_Utils.MinInt(marked, LF_Utils.MinInt(batchMax, room));
		quote.m_iAccepted = accepted;
		quote.m_iClean = CleanFor(accepted, cutPct);
		quote.m_iLeftMarked = marked - accepted;
		return quote;
	}

	//------------------------------------------------------------------------------------------------
	//! Quote with the loaded config. Null when the config is missing or refused (logged at load).
	static LF_LaunderQuote QuoteWithConfig(string key, int marked, int nowSec)
	{
		LF_LaunderConfig config = LF_LaunderConfig.Get();
		if (!config)
			return null;

		return Quote(key, marked, nowSec, config.m_iCutPct, config.m_iBatchMax, config.m_iDailyCap);
	}

	//------------------------------------------------------------------------------------------------
	//! Count `amount` washed notes against `key`'s day. The first wash opens the 24 h window; a wash
	//! after it ended opens a new one.
	static void Record(string key, int amount, int nowSec)
	{
		if (amount <= 0)
			return;

		LF_LaunderDay day = s_Days.Get(key);
		if (!day)
		{
			day = new LF_LaunderDay();
			day.m_iWindowStart = nowSec;
			s_Days.Set(key, day);
		}
		else if (nowSec >= day.m_iWindowStart + DAY_SECONDS)
		{
			day.m_iWindowStart = nowSec;
			day.m_iTotal = 0;
		}

		day.m_iTotal = day.m_iTotal + amount;
		LF_Debug.Info("Laundering", string.Format("%1 washed %2 marked notes, %3 today", key, amount, day.m_iTotal));
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds a wash of `accepted` notes takes: one config batch time per started batch of batchMax.
	static int SecondsFor(int accepted, int batchMax, int batchSeconds)
	{
		if (accepted <= 0 || batchMax <= 0)
			return 0;

		int batches = (accepted + batchMax - 1) / batchMax;
		return batches * batchSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Days = new map<string, ref LF_LaunderDay>();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LaunderRules' day totals (LF_StaticReset finds it by reflection).
class LF_LaunderRules_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LaunderRules";
	}

	override void Apply()
	{
		LF_LaunderRules.ResetStatics();
	}
}
