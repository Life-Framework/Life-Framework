// red-proof: Observed 2026-11-19 with `--no-build --red LF_Test_HarvestBoard`: the red hook skips the 95 % cut, so the bonus, the XP and the done-state lines fail.
//! harvest/harvest-board (#3656): the farm board's prompts through LF_HarvestBoard.Use, the server entry both the
//! prompt and the rental RPC call. The board prefab carries four LF_HarvestBoardAction rows and finds its field; Post,
//! Take (rental and own reaper), the DLC answer picking the Z7211 or the UAZ469, a stranger at the kit, Return and
//! the refund, a user out of reach, and a board with no field. The test deletes what it spawns.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HarvestBoard : LF_Test
{
	protected IEntity m_Stranger;

	protected static const ResourceName FIELD = "{5FF806585DB556B3}Prefabs/Harvest/LF_WheatField.et";
	protected static const ResourceName BOARD_PREFAB = "{194CC74891AEC339}Prefabs/Harvest/LF_HarvestBoard.et";
	protected static const vector FIELD_AT = "460 0 140";
	protected static const vector BOARD_AT = "460 0 100";
	protected static const vector SECOND_FIELD_AT = "460 0 125";
	protected static const vector LONELY_AT = "900 0 900";
	protected static const string TAKER = "test-board-taker";
	protected static const string STRANGER = "test-board-stranger";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "harvest/harvest-board";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_HarvestContract.ResetStatics();
		LF_OwnedImplements.ResetStatics();
		IEntity fieldEntity = SpawnAt(FIELD, FIELD_AT);
		IEntity boardEntity = SpawnAt(BOARD_PREFAB, BOARD_AT);
		IEntity lonely = SpawnAt(BOARD_PREFAB, LONELY_AT);
		IEntity taker = LF_TestCharacterFixture.Spawn(ctx);
		IEntity stranger = LF_TestCharacterFixture.Spawn(ctx);
		m_Stranger = stranger;
		LF_WheatField field = LF_WheatField.Cast(fieldEntity);
		ctx.NotNull(field, "the wheat field spawns");
		ctx.NotNull(boardEntity, "the board prefab spawns");
		if (field && boardEntity && taker && stranger)
			Check(ctx, field, boardEntity, lonely, taker, stranger);

		LF_Utils.SetUidForTests(null, "");
		LF_HarvestContract.ResetStatics();
		LF_OwnedImplements.ResetStatics();
		if (taker)
			LF_TestCharacterFixture.Delete(taker);

		if (stranger)
			LF_TestCharacterFixture.Delete(stranger);

		if (field)
			field.ClearLocal();

		if (fieldEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(fieldEntity);

		if (boardEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(boardEntity);

		if (lonely)
			SCR_EntityHelper.DeleteEntityAndChildren(lonely);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, LF_WheatField field, IEntity board, IEntity lonely, IEntity taker, IEntity stranger)
	{
		ctx.True(LF_HarvestBoard.IsBoard(board), "the board prefab carries the board prompts");
		ctx.False(LF_HarvestBoard.IsBoard(field), "a field is not a board");
		ctx.True(LF_HarvestBoard.FieldOf(board) == field, "the board finds the field beside it");
		ctx.True(LF_HarvestBoard.FieldOf(lonely) == null, "a board with no field around it finds none");

		vector angles;
		vector spot = LF_HarvestBoard.KitSpot(board, field, angles);
		ctx.True(vector.Distance(spot, board.GetOrigin()) <= LF_OwnedImplements.RACK_REACH_M, "the rental kit stands inside the return reach of the board");
		ctx.True(vector.Distance(spot, field.GetOrigin()) > vector.Distance(board.GetOrigin(), field.GetOrigin()), "the kit stands on the side away from the field");

		LF_Utils.SetUidForTests(taker, TAKER);
		ctx.Equal(500, LF_MoneyUtils.AddCash(taker, 500), "the taker is seeded with $500");
		LF_Utils.SetUidForTests(stranger, STRANGER);
		ctx.Equal(400, LF_MoneyUtils.AddCash(stranger, 400), "the stranger is seeded with $400");
		taker.SetOrigin(board.GetOrigin() + "2 0 0");
		stranger.SetOrigin(board.GetOrigin() + "-2 0 0");

		// Out of reach and no field: refused before anything changes.
		taker.SetOrigin(board.GetOrigin() + "30 0 0");
		ctx.True(Use(board, taker, LF_EHarvestBoardMode.POST, true) == LF_EHarvestContract.KIT_FAR, "a user away from the board is refused");
		ctx.False(LF_HarvestContract.IsPosted(field), "the far user posted nothing");
		taker.SetOrigin(board.GetOrigin() + "2 0 0");
		ctx.True(Use(lonely, taker, LF_EHarvestBoardMode.POST, true) == LF_EHarvestContract.KIT_FAR, "a board the user is not at refuses");
		taker.SetOrigin(lonely.GetOrigin() + "2 0 0");
		ctx.True(Use(lonely, taker, LF_EHarvestBoardMode.POST, true) == LF_EHarvestContract.NO_FIELD, "a board with no field refuses with no field");
		taker.SetOrigin(board.GetOrigin() + "2 0 0");

		// Take and return before a contract exists.
		ctx.True(Use(board, taker, LF_EHarvestBoardMode.TAKE_RENTAL, false) == LF_EHarvestContract.NOT_POSTED, "taking before posting is refused");
		ctx.True(Use(board, taker, LF_EHarvestBoardMode.RETURN_KIT, false) == LF_EHarvestContract.NOT_POSTED, "returning before posting is refused");
		ctx.Equal(500, LF_MoneyUtils.GetCash(taker), "the refusals cost nothing");

		// Post, once.
		ctx.True(Use(board, taker, LF_EHarvestBoardMode.POST, true) == LF_EHarvestContract.OK, "the board posts the field's contract");
		ctx.True(Use(board, stranger, LF_EHarvestBoardMode.POST, true) == LF_EHarvestContract.ALREADY_POSTED, "a second post is refused");

		// The own-reaper route needs a reaper on the owned list.
		ctx.True(Use(board, stranger, LF_EHarvestBoardMode.TAKE_OWN, true) == LF_EHarvestContract.NO_REAPER, "taking with an own reaper needs one");

		// Rental with a client that has no Rally licence: the UAZ469, with a reaper, at the board.
		ctx.True(Use(board, taker, LF_EHarvestBoardMode.TAKE_RENTAL, false) == LF_EHarvestContract.OK, "the taker takes the contract with a rental kit");
		ctx.Equal(200, LF_MoneyUtils.GetCash(taker), "the $300 deposit is taken");
		IEntity kit = LF_HarvestContract.KitOf(field);
		ctx.NotNull(kit, "the kit stands in the world");
		if (!kit)
			return;

		ctx.True(LF_Utils.GetPrefabName(kit).Contains("UAZ469"), "a client without the licence gets the UAZ469");
		ctx.True(LF_ImplementHitch.FindHitched(kit) != null, "the kit tows a reaper");
		ctx.True(vector.Distance(kit.GetOrigin(), board.GetOrigin()) <= LF_OwnedImplements.RACK_REACH_M, "the kit stands at the board");
		ctx.True(Use(board, stranger, LF_EHarvestBoardMode.TAKE_RENTAL, false) == LF_EHarvestContract.ALREADY_TAKEN, "a second taker is refused");
		ctx.Equal(400, LF_MoneyUtils.GetCash(stranger), "the refused taker paid nothing");

		// Cut to the line: the bonus once.
		int cells = field.CellCount();
		int line = LF_HarvestContract.COMPLETE_PERCENT * cells / 100;
		array<int> cut = {};
		for (int i = 0; i < line + 1; i++)
		{
			cut.Insert(i);
		}

		int xpBefore = LF_JobLevels.XpOf(TAKER, LF_JobLevels.JOB_FARMER);
		// RedProof(): the cut is skipped, so the bonus never arrives (`--red`).
		if (!RedProof())
			field.CutCells(cut);

		ctx.True(LF_HarvestContract.Settle(field, System.GetUnixTime()) == LF_EHarvestContract.PAID, "the contract pays at 95 %");
		ctx.Equal(240, LF_MoneyUtils.GetCash(taker), "the $40 bonus is paid");
		ctx.True(LF_JobLevels.XpOf(TAKER, LF_JobLevels.JOB_FARMER) > xpBefore, "FARMER XP is paid with the bonus");

		// Return: a stranger cannot, the taker gets the deposit back.
		ctx.True(Use(board, stranger, LF_EHarvestBoardMode.RETURN_KIT, false) == LF_EHarvestContract.NOT_TAKER, "someone else cannot return the kit");
		ctx.True(Use(board, taker, LF_EHarvestBoardMode.RETURN_KIT, false) == LF_EHarvestContract.OK, "the taker returns the kit");
		ctx.Equal(540, LF_MoneyUtils.GetCash(taker), "the deposit is back in the taker's cash");
		ctx.True(LF_HarvestContract.KitOf(field) == null, "the returned kit is gone");
		ctx.True(Use(board, taker, LF_EHarvestBoardMode.RETURN_KIT, false) == LF_EHarvestContract.NOT_POSTED, "a second return finds nothing to refund");
		ctx.Equal(540, LF_MoneyUtils.GetCash(taker), "no second refund");
		ctx.True(Use(board, taker, LF_EHarvestBoardMode.POST, true) == LF_EHarvestContract.NOT_RIPE, "a field cut to the line is not postable");

		CheckRallyAndOwnReaper(ctx, board, taker);
	}

	//------------------------------------------------------------------------------------------------
	//! A new field state: the DLC answer picks the Z7211, and the own-reaper route takes a field with no deposit.
	protected void CheckRallyAndOwnReaper(LF_TestContext ctx, IEntity board, IEntity taker)
	{
		IEntity secondEntity = SpawnAt(FIELD, SECOND_FIELD_AT);
		LF_WheatField field = LF_WheatField.Cast(secondEntity);
		ctx.True(field != null && LF_HarvestBoard.FieldOf(board) == field, "a second, nearer field is the board's field now");
		if (!field)
			return;

		ctx.True(Use(board, taker, LF_EHarvestBoardMode.POST, true) == LF_EHarvestContract.OK, "the second field is posted");
		int cashBefore = LF_MoneyUtils.GetCash(taker);
		// The Rally licence answer picks the Z7211 (LF_HarvestContract.KitPrefab, LF_Test_HarvestContract). The headless test
		// server has no Rally content to spawn it, so a "yes" here would fail the spawn; the refusal rolls the deposit back.
		ctx.True(Use(board, taker, LF_EHarvestBoardMode.TAKE_RENTAL, true) == LF_EHarvestContract.KIT_FAILED || LF_HarvestContract.KitOf(field) != null, "a rental with the licence answer takes the Z7211 or, with no Rally content, gives the deposit back");
		if (LF_HarvestContract.KitOf(field))
		{
			ctx.True(Use(board, taker, LF_EHarvestBoardMode.RETURN_KIT, false) == LF_EHarvestContract.OK, "the Rally kit goes back");
		}
		else
		{
			ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(taker), "a failed Rally kit leaves the cash as it was");
		}

		LF_OwnedImplements.Grant(TAKER, LF_OwnedImplements.ID_REAPER);
		ctx.True(Use(board, taker, LF_EHarvestBoardMode.TAKE_OWN, false) == LF_EHarvestContract.OK, "the taker takes the field with an own reaper");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(taker), "the own-reaper route costs no deposit");
		ctx.True(LF_HarvestContract.KitOf(field) == null, "the own-reaper route spawns no kit");
		field.ClearLocal();
		SCR_EntityHelper.DeleteEntityAndChildren(secondEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! One prompt as `user`: the test identity is one global, so it is set to the user before every call.
	protected LF_EHarvestContract Use(IEntity board, IEntity user, LF_EHarvestBoardMode mode, bool dlcOwned)
	{
		string uid = TAKER;
		if (user == m_Stranger)
			uid = STRANGER;

		LF_Utils.SetUidForTests(user, uid);
		return LF_HarvestBoard.Use(board, user, mode, dlcOwned);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAt(ResourceName prefab, vector at)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		return LF_Utils.SpawnEntityPrefab(prefab, at);
	}
}
