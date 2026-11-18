// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_WheatFieldRules`: `RED PROOF OK (all 1 listed test(s) fail with the hook on)`; with the acts skipped, `a 60 x 40 m field has 260 cells`, `7 m/s is TOO_FAST` and `a hopper at 595 of 600 kg is HOPPER_FULL for 10 kg` (and `takes 5 kg (expected 5, got -1)`) fail.
//! Wheat field rules (#3285): the pure grid, footprint, speed gate, yield and hopper, with no world.
//! A 60 x 40 m field is 20 x 13 = 260 cells and 9 bitset words; a 3 m bar driven down one row cuts exactly
//! that row; a diagonal pass cuts the diagonal and its two neighbours; cutting twice counts once; 7 m/s is
//! TOO_FAST; a hopper at 595 of 600 kg takes 5 kg and says HOPPER_FULL.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_WheatFieldRules : LF_Test
{
	override string GetName()
	{
		return "harvest/wheat-field-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		CheckGrid(ctx);
		CheckBitset(ctx);
		CheckRowPass(ctx);
		CheckDiagonal(ctx);
		CheckSpeedGate(ctx);
		CheckHopper(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_WheatGrid Field()
	{
		return LF_WheatGrid.Create(100, 200, 60, 40);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckGrid(LF_TestContext ctx)
	{
		LF_WheatGrid grid = Field();
		int cells = 0;
		if (!RedProof())
			cells = grid.CellCount();

		ctx.Equal(260, cells, "a 60 x 40 m field has 260 cells");
		ctx.Equal(20, grid.Columns(), "20 columns of 3 m");
		ctx.Equal(13, grid.Rows(), "13 rows of 3 m");
		ctx.Equal(0, grid.CutCount(), "a new field stands uncut");
		ctx.Equal(0, grid.CellAt(100.1, 200.1), "the south-west corner is cell 0");
		ctx.Equal(19, grid.CellAt(159.9, 200.1), "the last column of row 0");
		ctx.Equal(20, grid.CellAt(100.1, 203.1), "the next row starts at 20");
		ctx.Equal(259, grid.CellAt(159.9, 238.9), "the north-east corner is the last cell");
		ctx.Equal(-1, grid.CellAt(99.9, 210), "west of the field is outside");
		ctx.Equal(-1, grid.CellAt(160.1, 210), "east of the field is outside");
		ctx.Equal(-1, grid.CellAt(130, 199.9), "south of the field is outside");
		ctx.Equal(1, LF_WheatGrid.Create(0, 0, 1, 1).CellCount(), "a box smaller than a cell still has one cell");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBitset(LF_TestContext ctx)
	{
		LF_WheatGrid grid = Field();
		array<int> words;
		grid.ExportWords(words);
		ctx.Equal(9, words.Count(), "260 cells need 9 words");

		bool first = false;
		if (!RedProof())
			first = grid.Cut(37);
		ctx.True(first, "a standing cell is cut");
		ctx.True(grid.IsCut(37), "the cut cell tests as cut");
		ctx.True(!grid.IsCut(36) && !grid.IsCut(38), "its neighbours stand");
		ctx.True(!grid.Cut(37), "cutting twice does not count twice");
		ctx.Equal(1, grid.CutCount(), "one cell is counted once");
		ctx.True(!grid.Cut(-1) && !grid.Cut(260), "a cell outside the grid is refused");

		// Cells 31, 32 and 259 sit on the word edges, including bit 31 (the sign bit) and the last bit used.
		grid.Cut(31);
		grid.Cut(32);
		grid.Cut(259);
		ctx.Equal(4, grid.CutCount(), "four cells are cut");
		ctx.True(grid.IsCut(31) && grid.IsCut(32) && grid.IsCut(259), "word-edge and sign-bit cells test as cut");
		ctx.True(!grid.IsCut(30) && !grid.IsCut(33) && !grid.IsCut(258), "the cells beside them stand");

		grid.ExportWords(words);
		LF_WheatGrid late = Field();
		late.ImportWords(words);
		ctx.Equal(4, late.CutCount(), "a late joiner's snapshot carries the count");
		ctx.True(late.IsCut(37) && late.IsCut(31) && late.IsCut(32) && late.IsCut(259), "and the same cut cells");
		ctx.True(!late.IsCut(36), "and not the standing ones");

		// A snapshot with stray bits past the last cell keeps the count true.
		array<int> stray = {};
		for (int i = 0; i < 9; i++)
		{
			stray.Insert(-1);
		}
		late.ImportWords(stray);
		ctx.Equal(260, late.CutCount(), "all-ones words count 260 cells, not 288");

		grid.Reset();
		ctx.Equal(0, grid.CutCount(), "a regrown field has nothing cut");
		ctx.True(!grid.IsCut(37) && !grid.IsCut(259), "every cell stands again");
	}

	//------------------------------------------------------------------------------------------------
	//! A 3 m bar facing +X (heading 90), centred on row 4, driven the length of the field in 1 m steps.
	protected void CheckRowPass(LF_TestContext ctx)
	{
		LF_WheatGrid grid = Field();
		float rowZ = 200 + 4.5 * 3;
		int cut = 0;
		for (int step = 0; step <= 60; step++)
		{
			array<int> fresh;
			cut = cut + grid.CutFootprint(100 + step, rowZ, 90, 3, 1, fresh);
		}

		ctx.Equal(20, cut, "a 3 m bar down one row cuts that row's 20 cells");
		ctx.Equal(20, grid.CutCount(), "and nothing else");
		bool allInRow = true;
		for (int column = 0; column < 20; column++)
		{
			if (!grid.IsCut(grid.CellIndex(column, 4)))
				allInRow = false;
		}
		ctx.True(allInRow, "every cell of row 4 is cut");
		ctx.True(!grid.IsCut(grid.CellIndex(0, 3)) && !grid.IsCut(grid.CellIndex(10, 5)), "rows 3 and 5 stand");

		array<int> again;
		ctx.Equal(0, grid.CutFootprint(130, rowZ, 90, 3, 1, again), "a second pass over the same row cuts nothing");
		ctx.Equal(20, grid.CutCount(), "the count did not move");

		// The same bar astride two rows takes both.
		array<int> astride;
		grid.Footprint(131.5, 200 + 3 * 5, 90, 3, 1, astride);
		ctx.Equal(2, astride.Count(), "a bar astride a row boundary touches the cell on each side");

		// A bar driven off the field cuts only what is inside it.
		LF_WheatGrid edge = Field();
		array<int> edgeCut;
		ctx.Equal(1, edge.CutFootprint(100, 201.5, 90, 3, 1, edgeCut), "a bar half off the west edge cuts its one cell");
		ctx.Equal(0, edge.CutFootprint(50, 201.5, 90, 3, 1, edgeCut), "a bar wholly off the field cuts nothing");
	}

	//------------------------------------------------------------------------------------------------
	//! A 6 m bar turned 45 degrees across a cell corner touches the four cells round the corner; a 3 m bar
	//! driven down the diagonal cuts the diagonal and its two neighbours, and nothing two cells away.
	protected void CheckDiagonal(LF_TestContext ctx)
	{
		LF_WheatGrid corner = Field();
		array<int> touched;
		corner.Footprint(106, 206, 45, 6, 1, touched);
		ctx.Equal(4, touched.Count(), "a diagonal bar across a corner touches four cells");
		int c1 = corner.CellIndex(1, 1);
		int c2 = corner.CellIndex(2, 1);
		int c3 = corner.CellIndex(1, 2);
		int c4 = corner.CellIndex(2, 2);
		ctx.True(touched.Find(c1) != -1 && touched.Find(c2) != -1 && touched.Find(c3) != -1 && touched.Find(c4) != -1, "they are the four round the corner");

		LF_WheatGrid grid = Field();
		for (int step = 0; step <= 30; step++)
		{
			array<int> fresh;
			grid.CutFootprint(101.5 + step, 201.5 + step, 45, 3, 1, fresh);
		}

		bool diagonal = true;
		for (int i = 0; i < 10; i++)
		{
			if (!grid.IsCut(grid.CellIndex(i, i)))
				diagonal = false;
		}
		ctx.True(diagonal, "the ten diagonal cells are cut");
		bool far = false;
		for (int row = 0; row < grid.Rows(); row++)
		{
			for (int column = 0; column < grid.Columns(); column++)
			{
				int gap = column - row;
				if (gap < 0)
					gap = -gap;
				if (gap >= 2 && grid.IsCut(grid.CellIndex(column, row)))
					far = true;
			}
		}
		ctx.True(!far, "no cell two or more columns off the diagonal is cut");
		ctx.True(grid.IsCut(grid.CellIndex(3, 4)) && grid.IsCut(grid.CellIndex(4, 3)), "the bar's width takes the cells beside the diagonal");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSpeedGate(LF_TestContext ctx)
	{
		LF_EHarvestRefusal fast = LF_EHarvestRefusal.OK;
		if (!RedProof())
			fast = LF_WheatFieldRules.CheckSpeed(7);

		ctx.True(fast == LF_EHarvestRefusal.TOO_FAST, "7 m/s is TOO_FAST");
		ctx.True(LF_WheatFieldRules.CheckSpeed(0) == LF_EHarvestRefusal.STOPPED, "standing still is STOPPED");
		ctx.True(LF_WheatFieldRules.CheckSpeed(0.4) == LF_EHarvestRefusal.STOPPED, "0.4 m/s is STOPPED");
		ctx.True(LF_WheatFieldRules.CheckSpeed(0.5) == LF_EHarvestRefusal.OK, "0.5 m/s cuts");
		ctx.True(LF_WheatFieldRules.CheckSpeed(2.2) == LF_EHarvestRefusal.OK, "8 km/h cuts");
		ctx.True(LF_WheatFieldRules.CheckSpeed(6) == LF_EHarvestRefusal.OK, "6 m/s cuts");
		ctx.True(LF_WheatFieldRules.CheckSpeed(6.1) == LF_EHarvestRefusal.TOO_FAST, "6.1 m/s is TOO_FAST");
		ctx.EqualStr("#LF-Harvest_TooFast", LF_WheatFieldRules.RefusalKey(LF_EHarvestRefusal.TOO_FAST), "TOO_FAST has a hint");
		ctx.EqualStr("#LF-Harvest_HopperFull", LF_WheatFieldRules.RefusalKey(LF_EHarvestRefusal.HOPPER_FULL), "HOPPER_FULL has a hint");
		ctx.EqualStr("", LF_WheatFieldRules.RefusalKey(LF_EHarvestRefusal.STOPPED), "STOPPED has none");
		ctx.EqualStr("TOO_FAST", LF_WheatFieldRules.RefusalName(LF_EHarvestRefusal.TOO_FAST), "the log word names the refusal");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckHopper(LF_TestContext ctx)
	{
		ctx.Equal(0, LF_WheatFieldRules.YieldKg(0), "no cells, no grain");
		ctx.Equal(30, LF_WheatFieldRules.YieldKg(3), "three cells give 30 kg");
		ctx.Equal(2600, LF_WheatFieldRules.YieldKg(260), "a whole field gives 2,600 kg");

		int taken = -1;
		LF_EHarvestRefusal result = LF_EHarvestRefusal.OK;
		if (!RedProof())
			result = LF_WheatFieldRules.FillHopper(595, 10, taken);

		ctx.True(result == LF_EHarvestRefusal.HOPPER_FULL, "a hopper at 595 of 600 kg is HOPPER_FULL for 10 kg");
		ctx.Equal(5, taken, "and takes 5 kg");

		result = LF_WheatFieldRules.FillHopper(595, 5, taken);
		ctx.True(result == LF_EHarvestRefusal.OK, "595 plus exactly 5 fits");
		ctx.Equal(5, taken, "all 5 kg go in");

		result = LF_WheatFieldRules.FillHopper(600, 10, taken);
		ctx.True(result == LF_EHarvestRefusal.HOPPER_FULL, "a full hopper refuses");
		ctx.Equal(0, taken, "and takes nothing");

		result = LF_WheatFieldRules.FillHopper(0, 10, taken);
		ctx.True(result == LF_EHarvestRefusal.OK && taken == 10, "an empty hopper takes a cell's grain");
		result = LF_WheatFieldRules.FillHopper(700, 10, taken);
		ctx.True(result == LF_EHarvestRefusal.HOPPER_FULL && taken == 0, "a hopper over the cap takes nothing and never goes negative");
	}
}
