// red-proof: observed 2026-11-19 with `tools\cli test --no-build --red LF_Test_WheatFieldSnapshot`: `RED PROOF OK (all 1 listed test(s) fail with the hook on)`; with the encode, snapshot and first send skipped, `the cells wait in the queue (expected 3, got 0)` and `150 ms remain of the 250 ms interval (expected 150, got 250)` fail.
//! harvest/wheat-field-snapshot (#3288): the batch encoder and decoder round-trip any cell set (sign-bit and word
//! edges included), a 260-cell bitset snapshot is 9 ints and rebuilds the same field, a batch goes out at most
//! once per 250 ms, and cutting the same cells a second time queues and sends nothing. Pure: no world.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_WheatFieldSnapshot : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "harvest/wheat-field-snapshot";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckWire(ctx);
		CheckSnapshot(ctx);
		CheckBatch(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_WheatGrid Field()
	{
		return LF_WheatGrid.Create(100, 200, 60, 40);
	}

	//------------------------------------------------------------------------------------------------
	//! Encodes then decodes; the same cells come back ascending. A repeat or unsorted input is fine.
	protected bool RoundTrips(array<int> cells, array<int> expected)
	{
		array<int> back;
		string packed = "";
		if (!RedProof())
			packed = LF_WheatFieldWire.Encode(cells);

		if (!LF_WheatFieldWire.Decode(packed, back))
			return false;

		if (back.Count() != expected.Count())
			return false;

		for (int i = 0; i < back.Count(); i++)
		{
			if (back[i] != expected[i])
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckWire(LF_TestContext ctx)
	{
		array<int> none = {};
		ctx.True(RoundTrips(none, none), "an empty batch round-trips to nothing");
		ctx.EqualStr("", LF_WheatFieldWire.Encode(none), "an empty batch packs to an empty string");

		array<int> edges = {0, 31, 32, 63, 64, 259};
		ctx.True(RoundTrips(edges, edges), "cells on word edges (0, 31 the sign bit, 32, 63, 64, 259) round-trip");

		array<int> all = {};
		for (int i = 0; i < 260; i++)
		{
			all.Insert(i);
		}

		ctx.True(RoundTrips(all, all), "all 260 cells round-trip");
		ctx.Equal(9 * LF_WheatFieldWire.ENTRY_LENGTH, LF_WheatFieldWire.Encode(all).Length(), "all 260 cells pack to 9 entries");

		array<int> messy = {200, 5, 200, -3, 5, 31, 100000000};
		array<int> clean = {5, 31, 200};
		ctx.True(RoundTrips(messy, clean), "repeats, negatives and cells past the cap are dropped, the rest come back ascending");

		array<int> sparse = {};
		for (int j = 0; j < 20; j++)
		{
			sparse.Insert(j * 13);
		}

		ctx.True(RoundTrips(sparse, sparse), "20 spread cells round-trip");

		array<int> ignored;
		ctx.False(LF_WheatFieldWire.Decode("0000000000", ignored), "a string that is not whole 12-character entries is refused");
		ctx.False(LF_WheatFieldWire.Decode("0000000000zz", ignored), "a non-hex character is refused");
		ctx.False(LF_WheatFieldWire.Decode("ffff00000001", ignored), "a word index past the cap is refused");
		ctx.Equal(0, ignored.Count(), "a refused batch adds no cells");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSnapshot(LF_TestContext ctx)
	{
		LF_WheatGrid server = Field();
		for (int i = 0; i < 20; i++)
		{
			server.Cut(i * 13);
		}

		array<int> words = {};
		if (!RedProof())
			words = LF_WheatFieldWire.Snapshot(server);

		ctx.Equal(9, words.Count(), "a 260-cell snapshot is 9 ints");

		LF_WheatGrid joiner = Field();
		joiner.ImportWords(words);
		ctx.Equal(20, joiner.CutCount(), "a late joiner's grid counts the same 20 cut cells");
		for (int cell = 0; cell < 260; cell++)
		{
			if (server.IsCut(cell) != joiner.IsCut(cell))
			{
				ctx.Fail(string.Format("cell %1 differs after the snapshot", cell));
				return;
			}
		}

		ctx.Pass("every one of the 260 cells matches after the snapshot");
	}

	//------------------------------------------------------------------------------------------------
	//! The server path: a cell is queued only when the grid newly cut it, so a repeat cut queues nothing.
	protected void CutInto(LF_WheatGrid grid, LF_WheatCutBatch batch, array<int> cells)
	{
		foreach (int cell : cells)
		{
			if (grid.Cut(cell))
				batch.Add(cell);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBatch(LF_TestContext ctx)
	{
		LF_WheatGrid grid = Field();
		LF_WheatCutBatch batch = new LF_WheatCutBatch();
		array<int> strip = {};
		for (int i = 0; i < 20; i++)
		{
			strip.Insert(i);
		}

		CutInto(grid, batch, strip);
		ctx.Equal(20, batch.Pending(), "20 newly cut cells are queued");

		string first = "";
		if (!RedProof())
			first = batch.Take(1000);

		array<int> got;
		ctx.True(LF_WheatFieldWire.Decode(first, got), "the first batch goes at once and decodes");
		ctx.Equal(20, got.Count(), "the first batch carries all 20 cells");
		ctx.Equal(0, batch.Pending(), "sending empties the queue");

		array<int> more = {20, 21, 22};
		CutInto(grid, batch, more);
		ctx.EqualStr("", batch.Take(1100), "a send 100 ms after the last one waits");
		ctx.Equal(3, batch.Pending(), "the cells wait in the queue");
		ctx.Equal(150, batch.WaitMs(1100), "150 ms remain of the 250 ms interval");

		string second = batch.Take(1250);
		ctx.True(LF_WheatFieldWire.Decode(second, got), "the waiting cells go once 250 ms have passed");
		ctx.Equal(3, got.Count(), "the second batch carries the 3 new cells");

		// The same strip again: nothing is newly cut, so nothing is queued and nothing is sent.
		CutInto(grid, batch, strip);
		CutInto(grid, batch, more);
		ctx.Equal(0, batch.Pending(), "cutting the same cells a second time queues nothing");
		ctx.EqualStr("", batch.Take(5000), "cutting the same cells a second time sends nothing");

		// An empty send does not start the interval: the next cut goes at once.
		array<int> next = {100};
		CutInto(grid, batch, next);
		ctx.Equal(0, batch.WaitMs(5001), "an empty send leaves the interval unstarted");
		ctx.True(LF_WheatFieldWire.Decode(batch.Take(5001), got), "a later cut goes at once");

		// A regrow drops what is queued.
		array<int> late = {120, 121};
		CutInto(grid, batch, late);
		batch.Clear();
		ctx.Equal(0, batch.Pending(), "a regrow clears the queue");
	}
}
