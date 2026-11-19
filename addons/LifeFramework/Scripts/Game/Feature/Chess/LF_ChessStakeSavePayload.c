//------------------------------------------------------------------------------------------------
//! One chess stake still in escrow when the save was written: whose it is, which hold, and how much.
class LF_ChessEscrowRow : Managed
{
	static const int ROW_VERSION = 1;

	int m_iVersion;
	string m_sKey;
	string m_sToken;
	int m_iAmount;

	//------------------------------------------------------------------------------------------------
	static LF_ChessEscrowRow Create(string key, string token, int amount)
	{
		LF_ChessEscrowRow row = new LF_ChessEscrowRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sKey = key;
		row.m_sToken = token;
		row.m_iAmount = amount;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Save section "chess_escrow" (#3196): the stakes in escrow at chess tables. A game never outlives the
//! session, so a stake in the save is one whose game a crash ended; the load pays it back into the payer's
//! bank (sections apply after the bank payload). LF_ChessStake skips a hold it took or refunded this session,
//! so a second load pays nothing twice.
class LF_ChessStakeSavePayload : LF_SavePayload
{
	static const string NAME = "chess_escrow";

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
		array<ref LF_ChessEscrowRow> rows = LF_ChessStake.ExportEscrow();
		context.WriteValue("rows", rows);
		LF_Debug.Log("Persistence", string.Format("saved %1 chess stake(s) in escrow", rows.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override bool ReadSection(notnull LoadContext context, int savedVersion)
	{
		array<ref LF_ChessEscrowRow> rows = new array<ref LF_ChessEscrowRow>();
		if (!context.ReadValue("rows", rows))
		{
			LF_Debug.Warn(LF_ChessStake.FEATURE, "chess escrow failed to read; no stake is refunded");
			return false;
		}

		int refunded = LF_ChessStake.RefundSaved(rows);
		LF_Debug.Info("Persistence", string.Format("refunded %1 of %2 saved chess stake(s)", refunded, rows.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A save from before chess stakes held no escrow.
	override void OnAbsent()
	{
		LF_Debug.Info(LF_ChessStake.FEATURE, "save has no chess escrow; nothing to refund");
	}
}
