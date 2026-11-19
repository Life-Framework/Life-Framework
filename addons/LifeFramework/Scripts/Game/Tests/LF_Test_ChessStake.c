// red-proof: new test, no failing state to restore. `tools\cli test --no-build --red LF_Test_ChessStake`
// skips the mating move behind the RedProof() hook so the pot is never paid (2026-11-19).

//! #3196: playing chess for a stake. The first player sits at a fixed stake, the second sits only by
//! paying the same one. Both stakes leave carried cash into escrow; the winner is paid the pot
//! (ledger reason chess_stake); a draw, an abort, a player who is gone and a world end refund both;
//! a seat with short cash is refused and nothing moves; and the cash in the world is the same before
//! and after every way a game can end.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ChessStake : LF_Test
{
	static const ResourceName TABLE_PREFAB = "{33881F3E4E64549D}Prefabs/Props/Chess/LF_ChessTable.et";
	static const int WHITE = 301;
	static const int BLACK = 302;
	static const int THIRD = 303;
	static const string REASON = "chess_stake";
	static const string REFUND = "chess_refund";

	override string GetName()
	{
		return "chess/stake";
	}

	override void Run(LF_TestContext ctx)
	{
		CheckRules(ctx);
		CheckCash(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! The rules, pure: who may sit, what each ending queues.
	protected void CheckRules(LF_TestContext ctx)
	{
		LF_ChessTable table = new LF_ChessTable();
		array<ref LF_ChessPayout> payouts = {};

		ctx.Equal(LF_ChessTable.SEAT_WHITE, table.SitStaked(WHITE, 1000, 20, "uw", 100), "the first player sits at $20");
		ctx.Equal(20, table.GetStake().Pending(), "the table now asks $20");
		ctx.Equal(LF_ChessTable.SEAT_NONE, table.SitStaked(BLACK, 1000, 20, "ub", 19), "a second player with $19 is refused");
		ctx.Equal(LF_ChessStake.SHORT_CASH, table.GetRefusal(), "short cash is the reason");
		ctx.Equal(LF_ChessTable.SEAT_NONE, table.SitStaked(BLACK, 1000, 50, "ub", 100), "a different stake is refused");
		ctx.Equal(LF_ChessStake.MISMATCH, table.GetRefusal(), "a stake that does not match is the reason");
		ctx.Equal(LF_ChessTable.SEAT_NONE, table.Sit(BLACK, 1000), "a free sit at a staked table is refused");
		ctx.Equal(LF_ChessTable.SEAT_NONE, table.SitStaked(BLACK, 1000, 30, "ub", 100), "a stake that is no tier is refused");
		ctx.Equal(0, table.GetBlackId(), "the refused seat stayed empty");
		ctx.Equal(LF_ChessTable.SEAT_BLACK, table.SitStaked(BLACK, 1000, 20, "ub", 20), "the second player sits by paying $20");
		ctx.Equal(40, table.GetStake().Total(), "$40 sit in escrow");

		table.Move(WHITE, "f2f3", 1000);
		table.Move(BLACK, "e7e5", 1000);
		table.Move(WHITE, "g2g4", 1000);
		ctx.Equal(0, CountPayouts(table), "no payout while the game runs");
		bool mated = false;
		if (!RedProof())
			mated = table.Move(BLACK, "d8h4", 2000);
		ctx.True(mated, "black mates");
		table.TakePayouts(payouts);
		ctx.Equal(1, payouts.Count(), "one payout: the pot");
		if (payouts.Count() == 1)
		{
			ctx.Equal(BLACK, payouts.Get(0).m_iPlayerId, "the winner is paid");
			ctx.Equal(40, payouts.Get(0).m_iAmount, "the winner gets twice the stake");
			ctx.EqualStr(REASON, payouts.Get(0).m_sReason, "the pot carries the chess_stake reason");
		}
		table.Abort();
		ctx.Equal(0, CountPayouts(table), "an abort after the payout refunds nothing (no refund after a payout)");
		ctx.Equal(0, table.GetStake().Total(), "the escrow is empty after the payout");

		// A draw refunds both.
		LF_ChessStake stake = new LF_ChessStake();
		stake.Hold(0, WHITE, "uw", 50);
		stake.Hold(1, BLACK, "ub", 50);
		payouts.Clear();
		ctx.True(stake.Settle(LF_ChessGame.RESULT_DRAW, false, payouts), "a draw settles");
		ctx.Equal(2, payouts.Count(), "a draw pays both");
		ctx.Equal(100, SumPayouts(payouts), "a draw returns exactly the escrow");
		ctx.False(stake.Settle(LF_ChessGame.RESULT_WHITE_WINS, false, payouts), "a second ending pays nothing");

		// A stake paid in but the game never started: standing up returns it.
		LF_ChessTable lone = new LF_ChessTable();
		lone.SitStaked(WHITE, 0, 50, "uw", 50);
		ctx.True(lone.StandUp(WHITE, 10), "the lone player stands up");
		payouts.Clear();
		lone.TakePayouts(payouts);
		ctx.Equal(50, SumPayouts(payouts), "the stake of a game that never started goes back");
	}

	//------------------------------------------------------------------------------------------------
	protected int CountPayouts(LF_ChessTable table)
	{
		array<ref LF_ChessPayout> payouts = {};
		table.TakePayouts(payouts);
		return payouts.Count();
	}

	//------------------------------------------------------------------------------------------------
	protected int SumPayouts(array<ref LF_ChessPayout> payouts)
	{
		int total = 0;
		foreach (LF_ChessPayout payout : payouts)
			total += payout.m_iAmount;
		return total;
	}

	//------------------------------------------------------------------------------------------------
	protected void Mate(LF_ChessTableComponent table, bool play)
	{
		table.ServerMove(WHITE, "f2f3");
		table.ServerMove(BLACK, "e7e5");
		table.ServerMove(WHITE, "g2g4");
		if (play)
			table.ServerMove(BLACK, "d8h4");
	}

	//------------------------------------------------------------------------------------------------
	//! Real cash: two characters, escrow, payout, refunds, conservation.
	protected void CheckCash(LF_TestContext ctx)
	{
		IEntity entity = LF_Utils.SpawnEntityPrefab(TABLE_PREFAB, "0 0 0");
		LF_ChessTableComponent table = LF_ChessTableComponent.Of(entity);
		IEntity white = LF_TestCharacterFixture.Spawn(ctx);
		IEntity black = LF_TestCharacterFixture.Spawn(ctx);
		if (!table || !table.GetTable() || !white || !black)
		{
			ctx.True(false, "the table and two characters spawn");
			LF_TestCharacterFixture.Delete(white);
			LF_TestCharacterFixture.Delete(black);
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return;
		}

		LF_ATMManager.Reset();
		array<ref LF_LedgerEntry> ledgerBefore = LF_Ledger.ExportAll();
		LF_Ledger.ApplyAll(null);
		LF_Ledger.EnsureSubscribed();
		LF_MoneyUtils.SetUidForTests(black, "test-chess-black");

		table.SetBodyForTests(WHITE, white);
		table.SetBodyForTests(BLACK, black);
		LF_MoneyUtils.AddCash(white, 100, "test-fund");
		LF_MoneyUtils.AddCash(black, 100, "test-fund");
		int total = LF_MoneyUtils.GetCash(white) + LF_MoneyUtils.GetCash(black);
		int w0 = LF_MoneyUtils.GetCash(white);
		int b0 = LF_MoneyUtils.GetCash(black);

		// Start: both charged, and a seat with short cash is refused with nothing moved.
		ctx.Equal(LF_ChessTable.SEAT_WHITE, table.ServerSitFor(WHITE, white, 20), "white sits for $20");
		ctx.Equal(w0 - 20, LF_MoneyUtils.GetCash(white), "white is charged $20 at the start");
		IEntity poor = LF_TestCharacterFixture.Spawn(ctx);
		int poorCash = 0;
		if (poor)
		{
			LF_MoneyUtils.RemoveCash(poor, LF_Utils.MaxInt(LF_MoneyUtils.GetCash(poor), 0));
			poorCash = LF_MoneyUtils.GetCash(poor);
			table.SetBodyForTests(THIRD, poor);
			ctx.Equal(LF_ChessTable.SEAT_NONE, table.ServerSitFor(THIRD, poor, 20), "a seat with short cash is refused");
			ctx.Equal(poorCash, LF_MoneyUtils.GetCash(poor), "a refused seat moves no cash");
			ctx.Equal(0, table.GetBlackId(), "a refused seat stays empty");
		}
		ctx.Equal(LF_ChessTable.SEAT_NONE, table.ServerSitFor(BLACK, black, 50), "a different stake is refused");
		ctx.Equal(b0, LF_MoneyUtils.GetCash(black), "a mismatched stake moves no cash");
		ctx.Equal(LF_ChessTable.SEAT_BLACK, table.ServerSitFor(BLACK, black, 20), "black sits for $20");
		ctx.Equal(b0 - 20, LF_MoneyUtils.GetCash(black), "black is charged $20 at the start");
		ctx.Equal(20, table.GetStake(), "the replicated stake is $20");

		// A win: the pot goes to the winner, through the ledger.
		Mate(table, !RedProof());
		ctx.Equal(LF_ChessGame.RESULT_BLACK_WINS, table.GetResult(), "black mated white");
		ctx.Equal(w0 - 20, LF_MoneyUtils.GetCash(white), "the loser stays $20 down");
		ctx.Equal(b0 + 20, LF_MoneyUtils.GetCash(black), "the winner nets the stake: 2x paid, 1x put in");
		ctx.Equal(total, LF_MoneyUtils.GetCash(white) + LF_MoneyUtils.GetCash(black), "no cash leaked on a win");
		array<ref LF_LedgerEntry> last = {};
		LF_Ledger.Last("test-chess-black", LF_Ledger.ACCOUNT_CASH, 5, last);
		ctx.True(last.Count() >= 2, "the ledger holds the stake and the pot");
		if (last.Count() >= 2)
		{
			ctx.EqualStr(REASON, last.Get(0).m_sReason, "the pot has the chess_stake reason");
			ctx.Equal(40, last.Get(0).m_iDelta, "the pot is 2x the stake");
			ctx.EqualStr(REASON, last.Get(1).m_sReason, "the stake has the chess_stake reason");
			ctx.Equal(-20, last.Get(1).m_iDelta, "the stake left black's cash");
		}

		// An abort: black stands up mid-game and does not come back; both stakes go back.
		w0 = LF_MoneyUtils.GetCash(white);
		b0 = LF_MoneyUtils.GetCash(black);
		table.GetTable().StandUp(WHITE, 100);
		table.GetTable().StandUp(BLACK, 100);
		table.ServerTickAt(100);
		ctx.Equal(LF_ChessTable.SEAT_WHITE, table.ServerSitFor(WHITE, white, 50), "a new game: white sits for $50");
		ctx.Equal(LF_ChessTable.SEAT_BLACK, table.ServerSitFor(BLACK, black, 50), "and black");
		ctx.Equal(w0 - 50, LF_MoneyUtils.GetCash(white), "white is charged $50");
		ctx.Equal(b0 - 50, LF_MoneyUtils.GetCash(black), "black is charged $50");
		table.ServerMove(WHITE, "e2e4");
		table.GetTable().StandUp(BLACK, 5000);
		table.ServerTickAt(5000 + LF_ChessTable.GRACE_MS);
		ctx.Equal(w0, LF_MoneyUtils.GetCash(white), "an abort refunds white");
		ctx.Equal(b0, LF_MoneyUtils.GetCash(black), "an abort refunds black");
		ctx.Equal(total, LF_MoneyUtils.GetCash(white) + LF_MoneyUtils.GetCash(black), "no cash leaked on an abort");

		// A seat refunds before the game ran.
		table.GetTable().StandUp(WHITE, 6000);
		ctx.Equal(LF_ChessTable.SEAT_WHITE, table.ServerSitFor(WHITE, white, 20), "white sits for $20 again");
		table.ServerStandUp(WHITE);
		ctx.Equal(total, LF_MoneyUtils.GetCash(white) + LF_MoneyUtils.GetCash(black), "standing up before the game returns the stake");

		// A player who left the server: the refund goes to the bank account by uid.
		table.SetBodyForTests(BLACK, null);
		ctx.Equal(LF_ChessTable.SEAT_WHITE, table.ServerSitFor(WHITE, white, 20), "white sits for $20 once more");
		ctx.Equal(LF_ChessTable.SEAT_BLACK, table.ServerSitFor(BLACK, black, 20, "test-chess-gone"), "black sits, then leaves");
		ctx.Equal(total - 40, LF_MoneyUtils.GetCash(white) + LF_MoneyUtils.GetCash(black), "$40 in escrow");
		table.ServerAbort();
		LF_BankAccount bank = LF_ATMManager.GetOrCreate("test-chess-gone");
		ctx.Equal(20, bank.GetBalance(), "a gone player's refund reaches their bank account");
		ctx.Equal(w0, LF_MoneyUtils.GetCash(white), "world end refunds the player still here");
		ctx.Equal(total, LF_MoneyUtils.GetCash(white) + LF_MoneyUtils.GetCash(black) + bank.GetBalance(), "no cash leaked on a world end");
		table.ServerAbort();
		ctx.Equal(total, LF_MoneyUtils.GetCash(white) + LF_MoneyUtils.GetCash(black) + bank.GetBalance(), "a second abort pays nothing");

		LF_MoneyUtils.SetUidForTests(null, "");
		LF_Ledger.ApplyAll(ledgerBefore);
		LF_ATMManager.Reset();
		LF_TestCharacterFixture.Delete(white);
		LF_TestCharacterFixture.Delete(black);
		LF_TestCharacterFixture.Delete(poor);
		SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}
}
