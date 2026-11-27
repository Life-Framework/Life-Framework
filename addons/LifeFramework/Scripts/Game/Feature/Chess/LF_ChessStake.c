// LF_ChessStake.c: playing chess for a stake (#3196, chapter #1011 Evenings in the village).
//
// The first player sits with "Play for $20" and the second sits only by paying the same stake. Each
// stake leaves the player's carried cash into escrow on the server; the winner is paid the pot, and a
// draw, an abort (a player who did not come back, a table deleted at world end) refunds both.
//
//   LF_ChessPayout   one amount owed to one player (the table queues them, the component pays them)
//   LF_ChessStake    the escrow of one table: who holds how much, and what each way a game ends pays

//------------------------------------------------------------------------------------------------
//! Cash the server owes a player after a game: the pot, or a refund. Pure data.
class LF_ChessPayout : Managed
{
	int m_iPlayerId;
	string m_sUid;
	int m_iAmount;
	string m_sReason;

	//------------------------------------------------------------------------------------------------
	static LF_ChessPayout Make(int playerId, string uid, int amount, string reason)
	{
		LF_ChessPayout payout = new LF_ChessPayout();
		payout.m_iPlayerId = playerId;
		payout.m_sUid = uid;
		payout.m_iAmount = amount;
		payout.m_sReason = reason;
		return payout;
	}
}

//------------------------------------------------------------------------------------------------
//! The escrow of one table. No entity, no cash, no replication: it records what each seat paid in and
//! turns a game ending into payouts. Every amount in equals the amount out: a payout empties the
//! holds it pays from, so a second ending finds nothing to pay (no double payout, no refund after a
//! payout).
class LF_ChessStake : Managed
{
	static const string FEATURE = "Chess";
	//! The ledger reason of a stake leaving a player and of the pot reaching the winner.
	static const string REASON_STAKE = "chess_stake";
	//! The ledger reason of a stake returned (a draw, an abort, a stand-up before the game).
	static const string REASON_REFUND = "chess_refund";

	//! The fixed stakes a table offers besides a free game (0). From config in a follow-up.
	static const int STAKE_LOW = 20; // lf-fixed: pub table stake, the card names 0, 20, 50; a server setting is a follow-up
	static const int STAKE_HIGH = 50; // lf-fixed: pub table stake, the card names 0, 20, 50; a server setting is a follow-up

	//! Why a sit at a stake was refused (Accepts).
	static const int OK = 0;
	static const int BAD_TIER = 1;
	static const int SHORT_CASH = 2;
	static const int MISMATCH = 3;

	protected ref array<int> m_aHold = {0, 0};
	protected ref array<int> m_aPlayer = {0, 0};
	protected ref array<string> m_aUid = {"", ""};
	//! One hold's identity across a restart (uid, second, counter): the crash refund pays each once.
	protected ref array<string> m_aToken = {"", ""};

	//! Every table's escrow on this server (weak: a deleted table drops out), for the save section.
	protected static ref array<LF_ChessStake> s_aLive;
	//! Tokens this session held or refunded: a load re-applied to a live session pays them nothing.
	protected static ref array<string> s_aSeen;
	protected static int s_iCounter;

	//------------------------------------------------------------------------------------------------
	void LF_ChessStake()
	{
		Live().Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	protected static array<LF_ChessStake> Live()
	{
		if (!s_aLive)
			s_aLive = new array<LF_ChessStake>();
		return s_aLive;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<string> Seen()
	{
		if (!s_aSeen)
			s_aSeen = new array<string>();
		return s_aSeen;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: every stake in escrow on the server, keyed by the payer's uid.
	static array<ref LF_ChessEscrowRow> ExportEscrow()
	{
		array<ref LF_ChessEscrowRow> rows = new array<ref LF_ChessEscrowRow>();
		foreach (LF_ChessStake stake : Live())
		{
			if (!stake)
				continue;
			for (int seat = 0; seat < 2; seat++)
			{
				if (stake.m_aHold[seat] > 0 && !stake.m_aUid[seat].IsEmpty())
					rows.Insert(LF_ChessEscrowRow.Create(stake.m_aUid[seat], stake.m_aToken[seat], stake.m_aHold[seat]));
			}
		}
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the server went down with stakes in escrow (a crash; a clean world end refunds
	//! first). Each saved stake goes back into its payer's bank account once; a token this session
	//! held or refunded is skipped, so a second load pays nothing twice.
	//! \return how many stakes were paid back
	static int RefundSaved(notnull array<ref LF_ChessEscrowRow> rows)
	{
		int refunded;
		foreach (LF_ChessEscrowRow row : rows)
		{
			if (!row || row.m_sKey.IsEmpty() || row.m_sToken.IsEmpty() || row.m_iAmount <= 0)
				continue;
			if (Seen().Contains(row.m_sToken))
				continue;

			int amount = LF_Utils.MinInt(row.m_iAmount, STAKE_HIGH);
			LF_BankAccount account = LF_ATMManager.GetOrCreate(row.m_sKey);
			if (!account)
			{
				LF_Debug.Error(FEATURE, string.Format("restart refund of a $%1 stake failed: no bank account for %2", amount, row.m_sKey));
				continue;
			}

			Seen().Insert(row.m_sToken);
			account.Deposit(amount, REASON_REFUND);
			refunded++;
			LF_Debug.Info(FEATURE, string.Format("restart refund: a $%1 chess stake left in escrow is back in %2's bank", amount, row.m_sKey));
		}
		return refunded;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aLive = null;
		s_aSeen = null;
		s_iCounter = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! True for a stake a table offers: 0 (free), STAKE_LOW or STAKE_HIGH.
	static bool IsTier(int stake)
	{
		return stake == 0 || stake == STAKE_LOW || stake == STAKE_HIGH;
	}

	//------------------------------------------------------------------------------------------------
	//! Cash in escrow at the table (both holds).
	int Total()
	{
		return m_aHold[0] + m_aHold[1];
	}

	//------------------------------------------------------------------------------------------------
	//! The stake the table currently asks: the hold of whoever has paid in, 0 when nobody has.
	int Pending()
	{
		return LF_Utils.MaxInt(m_aHold[0], m_aHold[1]);
	}

	//------------------------------------------------------------------------------------------------
	//! May a player holding `cash` take `seat` at `stake`? A free game (0) needs the other seat free
	//! or free too; a staked game needs a fixed tier, the cash, and an empty other seat or one that
	//! paid the same.
	//! \param otherSeated true when the other seat is taken (its hold is 0 for a free player)
	int Accepts(int seat, int stake, int cash, bool otherSeated)
	{
		if (!IsTier(stake))
			return BAD_TIER;
		if (stake > 0 && cash < stake)
			return SHORT_CASH;

		int other = 0;
		if (seat == 0)
			other = m_aHold[1];
		else
			other = m_aHold[0];
		if (otherSeated && other != stake)
			return MISMATCH;
		if (!otherSeated && other != 0 && other != stake)
			return MISMATCH;
		return OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Records that `seat` paid `amount` in. The caller took the cash and has checked Accepts.
	void Hold(int seat, int playerId, string uid, int amount)
	{
		if (seat < 0 || seat > 1 || amount <= 0)
			return;

		m_aHold[seat] = amount;
		m_aPlayer[seat] = playerId;
		m_aUid[seat] = uid;
		s_iCounter++;
		m_aToken[seat] = uid + "|" + System.GetUnixTime().ToString() + "|" + s_iCounter.ToString();
		Seen().Insert(m_aToken[seat]);
		LF_Debug.Info(FEATURE, string.Format("stake: player %1 put %2 in escrow, %3 held", playerId, amount, Total()));
	}

	//------------------------------------------------------------------------------------------------
	//! The seat stands up before the game ran: its hold goes back (queued in `queue` when `refund`).
	//! Nothing held, nothing queued.
	void Release(int seat, bool refund, notnull array<ref LF_ChessPayout> queue)
	{
		if (seat < 0 || seat > 1 || m_aHold[seat] <= 0)
			return;

		if (refund)
			queue.Insert(LF_ChessPayout.Make(m_aPlayer[seat], m_aUid[seat], m_aHold[seat], REASON_REFUND));
		LF_Debug.Info(FEATURE, string.Format("stake: player %1 released %2 from escrow (refund %3)", m_aPlayer[seat], m_aHold[seat], refund));
		Clear(seat);
	}

	//------------------------------------------------------------------------------------------------
	//! A game ended. `result` is an LF_ChessGame RESULT_*; `aborted` ends it whatever the result.
	//! Queues the payouts in `queue` and empties the escrow: the winner gets the pot, a draw or an abort
	//! returns each stake. Called again (or with the game still ongoing and not aborted) it queues
	//! nothing, so a game ending twice never pays twice.
	//! \return true when something was queued
	bool Settle(int result, bool aborted, notnull array<ref LF_ChessPayout> queue)
	{
		int pot = Total();
		if (pot <= 0)
			return false;
		if (!aborted && result == LF_ChessGame.RESULT_ONGOING)
			return false;

		int winner = -1;
		if (!aborted)
		{
			if (result == LF_ChessGame.RESULT_WHITE_WINS)
				winner = 0;
			else if (result == LF_ChessGame.RESULT_BLACK_WINS)
				winner = 1;
		}

		if (winner >= 0)
		{
			queue.Insert(LF_ChessPayout.Make(m_aPlayer[winner], m_aUid[winner], pot, REASON_STAKE));
			LF_Debug.Info(FEATURE, string.Format("stake: player %1 wins the pot of %2", m_aPlayer[winner], pot));
		}
		else
		{
			for (int seat = 0; seat < 2; seat++)
			{
				if (m_aHold[seat] > 0)
					queue.Insert(LF_ChessPayout.Make(m_aPlayer[seat], m_aUid[seat], m_aHold[seat], REASON_REFUND));
			}
			LF_Debug.Info(FEATURE, string.Format("stake: %1 refunded (aborted %2, result %3)", pot, aborted, result));
		}

		Clear(0);
		Clear(1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Clear(int seat)
	{
		m_aHold[seat] = 0;
		m_aPlayer[seat] = 0;
		m_aUid[seat] = "";
		m_aToken[seat] = "";
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ChessStake's statics (LF_StaticReset finds it by reflection).
class LF_ChessStake_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ChessStake";
	}

	override void Apply()
	{
		LF_ChessStake.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Moves the cash for the table: takes a stake from a body, pays a payout to a body or, when the
//! player has no body (left the server), into the bank account of their uid so nothing is lost.
class LF_ChessStakeCash
{
	//------------------------------------------------------------------------------------------------
	//! Takes `stake` from the body's carried cash. False, and nothing moved, when it is short.
	static bool Take(IEntity body, int stake)
	{
		if (!body || stake <= 0)
			return false;
		if (LF_MoneyUtils.GetCash(body) < stake)
			return false;

		int taken = LF_MoneyUtils.RemoveAmount(body, stake, LF_ChessStake.REASON_STAKE);
		if (taken == stake)
			return true;

		if (taken > 0)
			LF_MoneyUtils.AddCash(body, taken, LF_ChessStake.REASON_REFUND, "", true);
		LF_Debug.Warn(LF_ChessStake.FEATURE, string.Format("stake: took %1 of %2, returned", taken, stake));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Pays one payout to `body` (carried cash), else to the bank account of the payout's uid.
	//! \return true when the money reached someone
	static bool Pay(LF_ChessPayout payout, IEntity body)
	{
		if (!payout || payout.m_iAmount <= 0)
			return false;

		// A player id can be reused by someone who joined after the owner left: the body must be the
		// owner's (same uid), else the money goes to the owner's bank account.
		if (body && !payout.m_sUid.IsEmpty() && LF_Utils.GetPlayerUID(body) != payout.m_sUid)
		{
			LF_Debug.Warn(LF_ChessStake.FEATURE, string.Format("stake: body of player %1 is not %2, paying the bank", payout.m_iPlayerId, payout.m_sUid));
			body = null;
		}

		if (body)
		{
			int added = LF_MoneyUtils.AddCash(body, payout.m_iAmount, payout.m_sReason, "", true);
			if (added == payout.m_iAmount)
				return true;
			LF_Debug.Error(LF_ChessStake.FEATURE, string.Format("stake: paid %1 of %2 to player %3 (%4)", added, payout.m_iAmount, payout.m_iPlayerId, payout.m_sReason));
			return false;
		}

		if (payout.m_sUid.IsEmpty())
		{
			LF_Debug.Error(LF_ChessStake.FEATURE, string.Format("stake: %1 for player %2 has no body and no uid, lost (%3)", payout.m_iAmount, payout.m_iPlayerId, payout.m_sReason));
			return false;
		}

		LF_BankAccount account = LF_ATMManager.GetOrCreate(payout.m_sUid);
		if (!account)
		{
			LF_Debug.Error(LF_ChessStake.FEATURE, string.Format("stake: no bank account for %1, %2 lost (%3)", payout.m_sUid, payout.m_iAmount, payout.m_sReason));
			return false;
		}

		account.Deposit(payout.m_iAmount, payout.m_sReason);
		LF_Debug.Info(LF_ChessStake.FEATURE, string.Format("stake: player %1 is away, %2 went to their bank account (%3)", payout.m_iPlayerId, payout.m_iAmount, payout.m_sReason));
		return true;
	}
}
