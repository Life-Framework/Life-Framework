// red-proof: Observed 2026-11-22 with `tools/cli test --no-build --red LF_Test_JailBreakLayers,LF_Test_JailBreakFree,LF_Test_JailBail`: RED PROOF OK with the cell charge skipped: `the cells are blown`, `the breaker is highly wanted for the jailbreak`, `the outer door refuses inside the cooldown (expected '#LF-JailBreak_Cooldown', got '')`. The unhooked run passes.
//! The two-layer jailbreak (LF_JailBreak): the cell door cannot be charged while the outer door is
//! shut, a serving prisoner cannot charge either door, a refused arm keeps the charge's effects off
//! (no wanted), the outer door opens the way for the cells, the cooldown after the cells blow refuses
//! both doors, and a blown door seals itself again.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_JailBreakLayers : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/jailbreak-layers";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string officerUid = "jb-layers-officer";
		string breakerUid = "jb-layers-breaker";
		string prisonerUid = "jb-layers-prisoner";
		string outerId = "jb-layers-outer";
		string cellId = "jb-layers-cell";

		LF_JailBreak.ResetForTest();
		LF_JailBreakConfig config = LF_JailBreakConfig.CreateDefault();
		config.m_fOuterFuseSeconds = 0;
		config.m_fCellFuseSeconds = 0;
		LF_JailBreakConfig.UseForTest(config);

		LF_Jail.Configure(40, 200, 1, 4);
		LF_Jail.SetCellOrigin("52 0 70");
		LF_Jail.SetDoorTarget(cellId);
		LF_Jail.SetOuterDoorTarget(outerId);
		LF_Breach.Configure(outerId, 30, "");
		LF_Breach.Configure(cellId, 30, "");
		ctx.True(LF_JailBreak.IsTwoLayer(), "a jail with an outer door is two-layer");

		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetOnDuty(true);
		officer.SetPoliceRank(LF_PoliceRanks.SENIOR);
		LF_PlayerAccount breaker = LF_PlayerAccountManager.GetOrCreate(breakerUid);
		breaker.SetWantedLevel(0);
		LF_PlayerAccountManager.GetOrCreate(prisonerUid).SetWantedLevel(2);
		ctx.True(LF_Jail.Arrest(officerUid, prisonerUid, null), "the prisoner is jailed");

		// Layer order: the cells wait for the outer door.
		ctx.EqualStr(LF_JailBreak.REFUSE_OUTER_FIRST, LF_JailBreak.ArmRefusal(cellId, breakerUid, 1000), "the cells are refused while the outer door is shut");
		ctx.False(LF_Breach.Arm(cellId, breakerUid), "arming the cells first fails");
		ctx.True(LF_Breach.IsLocked(cellId), "the cells stay locked");
		ctx.Equal(0, breaker.GetWantedLevel(), "a refused arm adds no wanted");

		// Nobody breaches from inside.
		ctx.EqualStr(LF_JailBreak.REFUSE_FROM_INSIDE, LF_JailBreak.ArmRefusal(outerId, prisonerUid, 1000), "a serving prisoner cannot charge the outer door");
		ctx.False(LF_Breach.Arm(outerId, prisonerUid), "the prisoner's arm fails");

		// Layer 1, then layer 2. The config fuse (0) replaces the door's own 30 s.
		ctx.True(LF_Breach.Arm(outerId, breakerUid), "the outer door is charged");
		ctx.True(LF_Breach.IsOpen(outerId), "the config fuse blew the outer door at once");
		ctx.EqualStr("", LF_JailBreak.ArmRefusal(cellId, breakerUid, 1000), "with the outer door open the cells may be charged");
		// RedProof(): the cell charge is never set, so the cells stay shut and the cooldown never starts.
		if (!RedProof())
			ctx.True(LF_Breach.Arm(cellId, breakerUid), "the cells are charged");

		ctx.True(LF_Breach.IsOpen(cellId), "the cells are blown");
		ctx.True(breaker.GetWantedLevel() >= 4, "the breaker is highly wanted for the jailbreak");
		ctx.True(LF_PrisonEscape.HasWantedRow(breakerUid, LF_JailBreak.OFFENCE_PRISON_BREACH), "the breaker is on the board for the jailbreak");

		// Cooldown: both doors refuse until it is spent.
		int now = System.GetUnixTime();
		ctx.True(LF_JailBreak.CooldownLeft(now) > 0, "the cooldown runs after the cells blew");
		ctx.True(LF_JailBreak.Reseal(outerId), "the outer door seals itself");
		ctx.True(LF_JailBreak.Reseal(cellId), "the cells seal themselves");
		ctx.True(LF_Breach.IsLocked(cellId), "the resealed cells are locked");
		ctx.False(LF_JailBreak.Reseal(cellId), "a locked door is not resealed twice");
		ctx.EqualStr(LF_JailBreak.REFUSE_COOLDOWN, LF_JailBreak.ArmRefusal(outerId, breakerUid, now), "the outer door refuses inside the cooldown");
		ctx.EqualStr("", LF_JailBreak.ArmRefusal(outerId, breakerUid, now + LF_JailBreakConfig.Get().CooldownSeconds() + 1), "the cooldown ends");

		// A jail with no outer door is the plain single door.
		LF_Jail.SetOuterDoorTarget("");
		ctx.False(LF_JailBreak.IsTwoLayer(), "no outer door, no layers");
		ctx.EqualStr("", LF_JailBreak.ArmRefusal(cellId, breakerUid, 0), "the single door has no layer rule");

		// Clean up what this test made.
		LF_Jail.Advance(prisonerUid, 100000);
		breaker.SetWantedLevel(0);
		LF_JailBreak.ResetForTest();
		LF_JailBreakConfig.Reload();
		LF_Jail.SyncPlacedCell();
	}
}
