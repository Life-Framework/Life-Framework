//------------------------------------------------------------------------------------------------
//! One stall pitch that was paid for and still held when the save was written: whose it is, the day it was rented
//! for, the fee and a token that names this rent across a restart. Field order is the save order; append only.
class LF_StallRentRow : Managed
{
	static const int ROW_VERSION = 1;

	int m_iVersion;
	string m_sHolder;
	string m_sToken;
	int m_iDay;
	int m_iFee;

	//------------------------------------------------------------------------------------------------
	static LF_StallRentRow Create(string holder, string token, int day, int fee)
	{
		LF_StallRentRow row = new LF_StallRentRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sHolder = holder;
		row.m_sToken = token;
		row.m_iDay = day;
		row.m_iFee = fee;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The pitches rented this session (#3279), kept by key so the save does not depend on which stall prop is alive.
//! `LF_StallComponent.Rent` records a rent, `Lapse` and `Clear` release it. A stall lives in server memory and is
//! free again after a restart, so a rent still held at a save is a fee the restart ate: the load pays it back into
//! the holder's bank (`RefundSaved`). `m_aSeen` holds every token this session recorded or refunded, so a second
//! load pays nothing twice and a re-apply in the session that took the rent pays nothing.
class LF_StallRentBook
{
	//! The ledger reason of the refund.
	static const string REASON_REFUND = "stall_rent-restart-refund";

	protected static ref map<string, ref LF_StallRentRow> s_mActive;
	protected static ref array<string> s_aSeen;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mActive = null;
		s_aSeen = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_StallRentRow> Active()
	{
		if (!s_mActive)
			s_mActive = new map<string, ref LF_StallRentRow>();

		return s_mActive;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<string> Seen()
	{
		if (!s_aSeen)
			s_aSeen = new array<string>();

		return s_aSeen;
	}

	//------------------------------------------------------------------------------------------------
	//! One rent's identity across a restart: holder, market day and the unix second it was paid.
	static string MakeToken(string holder, int day, int unix)
	{
		return holder + "|" + day.ToString() + "|" + unix.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a pitch was paid for. \return Its token, for `Release`; empty when the input is unusable.
	static string Record(string holder, int day, int fee, int unix)
	{
		if (holder.IsEmpty() || fee < 0)
			return "";

		string token = MakeToken(holder, day, unix);
		Active().Set(token, LF_StallRentRow.Create(holder, token, day, fee));
		if (!Seen().Contains(token))
			Seen().Insert(token);

		return token;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the pitch ended (the day passed, or the stall was cleared); it is no longer saved.
	static void Release(string token)
	{
		if (token.IsEmpty())
			return;

		Active().Remove(token);
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many pitches are held now.
	static int ActiveCount()
	{
		return Active().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: every pitch held now. Stalls past their closing hour are freed first.
	static array<ref LF_StallRentRow> Export()
	{
		LF_StallComponent.LapseAll();
		array<ref LF_StallRentRow> rows = new array<ref LF_StallRentRow>();
		foreach (string token, LF_StallRentRow row : Active())
		{
			if (row)
				rows.Insert(LF_StallRentRow.Create(row.m_sHolder, row.m_sToken, row.m_iDay, row.m_iFee));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: pays the saved fees back into the holders' bank accounts; their stalls went with the last
	//! session. A rent this session took or already refunded is skipped.
	//! \return How many fees were paid back.
	static int RefundSaved(notnull array<ref LF_StallRentRow> rows)
	{
		int refunded;
		foreach (LF_StallRentRow row : rows)
		{
			if (!row || row.m_sHolder.IsEmpty() || row.m_sToken.IsEmpty() || row.m_iFee <= 0)
				continue;

			if (Seen().Contains(row.m_sToken))
				continue;

			int amount = LF_Utils.ClampInt(row.m_iFee, 0, LF_StallConfig.MAX_CASH);
			LF_BankAccount account = LF_ATMManager.GetOrCreate(row.m_sHolder);
			if (!account)
			{
				LF_Debug.Error(LF_StallConfig.FEATURE, string.Format("restart refund of a $%1 stall rent failed: no bank account for %2", amount, row.m_sHolder));
				continue;
			}

			Seen().Insert(row.m_sToken);
			account.Deposit(amount, REASON_REFUND);
			refunded++;
			LF_Debug.Info(LF_StallConfig.FEATURE, string.Format("restart refund: the $%1 pitch fee of a stall held at the restart is back in %2's bank", amount, row.m_sHolder));
		}

		return refunded;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_StallRentBook's statics.
class LF_StallRentBook_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_StallRentBook";
	}

	override void Apply()
	{
		LF_StallRentBook.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "stall_rents" (#3279): the market-stall pitches held while the save was written. A stall is server
//! memory and is free after a restart, so the load pays each saved fee back into the holder's bank (sections apply
//! after the bank payload) instead of eating it. Listings are claims on the holder's own items and need no save.
class LF_StallSavePayload : LF_SavePayload
{
	static const string NAME = "stall_rents";

	//------------------------------------------------------------------------------------------------
	override string Name()
	{
		return NAME;
	}

	//------------------------------------------------------------------------------------------------
	override int Version()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override void WriteSection(notnull SaveContext context)
	{
		array<ref LF_StallRentRow> rows = LF_StallRentBook.Export();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 held stall rent(s)", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_StallRentRow> rows = new array<ref LF_StallRentRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_StallConfig.FEATURE, "held stall rents failed to read; none is refunded");
			return false;
		}

		int refunded = LF_StallRentBook.RefundSaved(rows);
		LF_Debug.Info("Persistence", string.Format("refunded %1 of %2 saved stall rent(s)", refunded, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before the section held no rents.
	override void OnAbsent()
	{
		LF_Debug.Info(LF_StallConfig.FEATURE, "save has no stall rents; nothing to refund");
	}
}
