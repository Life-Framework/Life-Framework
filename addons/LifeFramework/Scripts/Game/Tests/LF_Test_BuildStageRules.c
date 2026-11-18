// red-proof: observed 2026-11-18 with `tools\cli test --no-build --only LF_Test_BuildStageRules` before LF_BuildStageRules existed: the script failed to compile (`Unable to find class 'LF_BuildStageRules'`) and the run failed; with the rules in place it ran green.

//! Build-stage rules (#2648, slice of #747): what a stage still misses, who may deposit, when the
//! stage is done. Pure logic over the stage def and the "prefab=count;..." deposit string.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_BuildStageRules : LF_Test
{
	protected const string PLANK = "{11111111AAAAAAAA}Prefabs/Test/Plank.et";
	protected const string NAILS = "{22222222BBBBBBBB}Prefabs/Test/Nails.et";
	protected const string BRICK = "{33333333CCCCCCCC}Prefabs/Test/Brick.et";

	override string GetName()
	{
		return "property/build-stage-rules";
	}

	//------------------------------------------------------------------------------------------------
	//! A stage needing 10 planks, 4 nails and `work` units of work.
	protected LF_PropertyStageDef MakeStage(int work)
	{
		LF_PropertyStageDef def = new LF_PropertyStageDef();
		def.m_sStageId = "test";
		def.m_iWorkUnits = work;
		def.m_aRequirements = new array<ref LF_PropertyStageReqDef>();

		LF_PropertyStageReqDef planks = new LF_PropertyStageReqDef();
		planks.m_Prefab = PLANK;
		planks.m_iCount = 10;
		def.m_aRequirements.Insert(planks);

		LF_PropertyStageReqDef nails = new LF_PropertyStageReqDef();
		nails.m_Prefab = NAILS;
		nails.m_iCount = 4;
		def.m_aRequirements.Insert(nails);
		return def;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunMissing(ctx);
		RunDeposit(ctx);
		RunWork(ctx);
		RunDone(ctx);
		RunAccess(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunMissing(LF_TestContext ctx)
	{
		LF_PropertyStageDef def = MakeStage(8);
		ctx.Equal(10, LF_BuildStageRules.Missing(def, "", PLANK), "nothing deposited: all 10 planks missing");
		ctx.Equal(4, LF_BuildStageRules.Missing(def, "", NAILS), "nothing deposited: all 4 nails missing");
		ctx.Equal(0, LF_BuildStageRules.Missing(def, "", BRICK), "an item the stage does not need is never missing");

		string deposited = PLANK + "=3;" + NAILS + "=4";
		ctx.Equal(7, LF_BuildStageRules.Missing(def, deposited, PLANK), "3 of 10 planks leaves 7 missing");
		ctx.Equal(0, LF_BuildStageRules.Missing(def, deposited, NAILS), "all nails in leaves 0 missing");
		ctx.EqualStr(PLANK + "=7", LF_BuildStageRules.MissingText(def, deposited), "the missing list names only what is still short");
		ctx.EqualStr("", LF_BuildStageRules.MissingText(def, PLANK + "=10;" + NAILS + "=4"), "a full stage misses nothing");

		ctx.Equal(3, LF_BuildStageRules.CountOf(deposited, PLANK), "CountOf reads one item");
		ctx.Equal(0, LF_BuildStageRules.CountOf("junk;;=;x=notanumber", PLANK), "a malformed string reads as 0");
		ctx.Equal(0, LF_BuildStageRules.Missing(null, "", PLANK), "a null stage misses nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDeposit(LF_TestContext ctx)
	{
		LF_PropertyStageDef def = MakeStage(8);
		array<string> noKeys = new array<string>();

		LF_BuildDepositResult ok = LF_BuildStageRules.Deposit(def, "", PLANK, 6, "owner", noKeys, "owner");
		ctx.True(ok.m_bAccepted, "a deposit within the need is accepted");
		ctx.EqualStr(PLANK + "=6", ok.m_sDeposited, "the new string holds the deposit");

		LF_BuildDepositResult more = LF_BuildStageRules.Deposit(def, ok.m_sDeposited, PLANK, 4, "owner", noKeys, "owner");
		ctx.True(more.m_bAccepted, "the deposit that exactly fills the need is accepted");
		ctx.Equal(10, LF_BuildStageRules.CountOf(more.m_sDeposited, PLANK), "counts add across deposits");

		LF_BuildDepositResult over = LF_BuildStageRules.Deposit(def, ok.m_sDeposited, PLANK, 5, "owner", noKeys, "owner");
		ctx.False(over.m_bAccepted, "a deposit beyond the need is refused");
		ctx.Equal(LF_BuildStageRules.REFUSED_OVERFLOW, over.m_iReason, "the refusal is an overflow");
		ctx.Equal(1, over.m_iOverflow, "5 on top of 6 of 10 overflows by 1");
		ctx.EqualStr(PLANK, over.m_sOverflowPrefab, "the overflow names the item");
		ctx.EqualStr(ok.m_sDeposited, over.m_sDeposited, "a refused deposit changes nothing");

		LF_BuildDepositResult wrong = LF_BuildStageRules.Deposit(def, "", BRICK, 2, "owner", noKeys, "owner");
		ctx.False(wrong.m_bAccepted, "an item the stage does not need is refused");
		ctx.Equal(2, wrong.m_iOverflow, "the whole count of an unneeded item overflows");

		LF_BuildDepositResult zero = LF_BuildStageRules.Deposit(def, "", PLANK, 0, "owner", noKeys, "owner");
		ctx.False(zero.m_bAccepted, "a deposit of 0 is refused");
		ctx.Equal(LF_BuildStageRules.REFUSED_BAD_COUNT, zero.m_iReason, "0 is a bad count");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWork(LF_TestContext ctx)
	{
		LF_PropertyStageDef def = MakeStage(12);
		ctx.Equal(12, LF_BuildStageRules.WorkRemaining(def, 0), "a fresh stage needs all its work");
		ctx.Equal(5, LF_BuildStageRules.WorkRemaining(def, 7), "7 of 12 leaves 5");
		ctx.Equal(0, LF_BuildStageRules.WorkRemaining(def, 99), "work past the need leaves 0");

		// Shared with LF_StationProgress.Rounds, not copied: same numbers.
		ctx.Equal(LF_StationProgress.Rounds(12, 1), LF_BuildStageRules.RoundsLeft(def, 0, 1), "one worker takes the station rounds");
		ctx.Equal(LF_StationProgress.Rounds(12, 3), LF_BuildStageRules.RoundsLeft(def, 0, 3), "three workers take a third");
		ctx.Equal(4, LF_BuildStageRules.RoundsLeft(def, 0, 3), "12 units over 3 workers is 4 rounds");
		ctx.Equal(3, LF_BuildStageRules.RoundsLeft(def, 6, 2), "6 units left over 2 workers is 3 rounds");
		ctx.Equal(0, LF_BuildStageRules.RoundsLeft(def, 12, 2), "a finished stage has no rounds left");
		ctx.Equal(0, LF_BuildStageRules.RoundsLeft(def, 0, 0), "no workers means no rounds");

		ctx.Equal(10, LF_BuildStageRules.WorkAfter(def, 7, 3), "work adds up");
		ctx.Equal(12, LF_BuildStageRules.WorkAfter(def, 7, 50), "work never passes the need");
		ctx.Equal(7, LF_BuildStageRules.WorkAfter(def, 7, 0), "0 units changes nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDone(LF_TestContext ctx)
	{
		LF_PropertyStageDef def = MakeStage(8);
		string full = PLANK + "=10;" + NAILS + "=4";
		string part = PLANK + "=10;" + NAILS + "=3";

		ctx.False(LF_BuildStageRules.IsDone(def, "", 0), "nothing yet is not done");
		ctx.False(LF_BuildStageRules.IsDone(def, full, 7), "all items but 7 of 8 work is not done");
		ctx.False(LF_BuildStageRules.IsDone(def, part, 8), "all work but one nail short is not done");
		ctx.True(LF_BuildStageRules.IsDone(def, full, 8), "items and work both met is done");
		ctx.True(LF_BuildStageRules.ItemsMet(def, full), "items met");
		ctx.False(LF_BuildStageRules.ItemsMet(def, part), "items not met");
		ctx.True(LF_BuildStageRules.WorkMet(def, 8), "work met");

		// Work-only and item-only stages.
		LF_PropertyStageDef workOnly = new LF_PropertyStageDef();
		workOnly.m_iWorkUnits = 5;
		ctx.False(LF_BuildStageRules.IsDone(workOnly, "", 4), "a work-only stage waits for its work");
		ctx.True(LF_BuildStageRules.IsDone(workOnly, "", 5), "a work-only stage is done on its work");

		LF_PropertyStageDef itemsOnly = MakeStage(0);
		ctx.True(LF_BuildStageRules.IsDone(itemsOnly, full, 0), "an items-only stage is done on its items");
		ctx.False(LF_BuildStageRules.IsDone(null, full, 99), "a missing stage is never done");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunAccess(LF_TestContext ctx)
	{
		LF_PropertyStageDef def = MakeStage(8);
		array<string> keys = new array<string>();
		keys.Insert("friend");

		ctx.True(LF_BuildStageRules.MayContribute("owner", keys, "owner"), "the owner may contribute");
		ctx.True(LF_BuildStageRules.MayContribute("owner", keys, "friend"), "an invited key holder may contribute");
		ctx.False(LF_BuildStageRules.MayContribute("owner", keys, "stranger"), "a stranger may not");
		ctx.False(LF_BuildStageRules.MayContribute("owner", keys, ""), "an empty key may not");
		ctx.False(LF_BuildStageRules.MayContribute("", keys, ""), "a site with no owner key admits no empty key");

		LF_BuildDepositResult owner = LF_BuildStageRules.Deposit(def, "", PLANK, 2, "owner", keys, "owner");
		ctx.True(owner.m_bAccepted, "the owner's deposit is accepted");
		LF_BuildDepositResult friend = LF_BuildStageRules.Deposit(def, owner.m_sDeposited, PLANK, 2, "owner", keys, "friend");
		ctx.True(friend.m_bAccepted, "a key holder's deposit is accepted");
		ctx.Equal(4, LF_BuildStageRules.CountOf(friend.m_sDeposited, PLANK), "both deposits count");

		LF_BuildDepositResult stranger = LF_BuildStageRules.Deposit(def, friend.m_sDeposited, PLANK, 2, "owner", keys, "stranger");
		ctx.False(stranger.m_bAccepted, "a stranger's deposit is refused");
		ctx.Equal(LF_BuildStageRules.REFUSED_NOT_ALLOWED, stranger.m_iReason, "the refusal is access");
		ctx.EqualStr(friend.m_sDeposited, stranger.m_sDeposited, "a refused stranger changes nothing");
	}
}
