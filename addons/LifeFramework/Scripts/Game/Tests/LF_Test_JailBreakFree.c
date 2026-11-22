// red-proof: Observed 2026-11-22 with `tools/cli test --no-build --red LF_Test_JailBreakLayers,LF_Test_JailBreakFree,LF_Test_JailBail`: RED PROOF OK with the cell charge skipped: `the first prisoner's sentence ended`, `a freed prisoner is posted as escaped custody`, `the jailbreak is on the breaker's record`. The unhooked run passes.
//! Blowing the cells of a two-layer jail (LF_JailBreak.OnOpened, LF_Jail.FreeInside) frees every
//! prisoner whose body is in the cell: the sentence ends, they are posted as escaped custody and
//! wanted. A prisoner with no body in the world (offline) is not in the cell and serves on.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_JailBreakFree : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/jailbreak-free";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string breakerUid = "jb-free-breaker";
		string firstUid = "jb-free-first";
		string secondUid = "jb-free-second";
		string offlineUid = "jb-free-offline";
		string outerId = "jb-free-outer";
		string cellId = "jb-free-cell";

		IEntity first = LF_TestCharacterFixture.Spawn(ctx);
		IEntity second = LF_TestCharacterFixture.Spawn(ctx);
		if (!first || !second)
		{
			LF_TestCharacterFixture.Delete(first);
			LF_TestCharacterFixture.Delete(second);
			return;
		}

		LF_JailBreak.ResetForTest();
		LF_JailBreakConfig config = LF_JailBreakConfig.CreateDefault();
		config.m_fOuterFuseSeconds = 0;
		config.m_fCellFuseSeconds = 0;
		LF_JailBreakConfig.UseForTest(config);
		LF_CrimeRecord.EnsureSubscribed();

		LF_Jail.Configure(40, 200, 1, 4);
		LF_Jail.SetCellOrigin(first.GetOrigin());
		LF_Jail.SetDoorTarget(cellId);
		LF_Jail.SetOuterDoorTarget(outerId);
		LF_Breach.Configure(outerId, 0, "");
		LF_Breach.Configure(cellId, 0, "");

		LF_PlayerAccount breaker = LF_PlayerAccountManager.GetOrCreate(breakerUid);
		breaker.SetWantedLevel(0);
		LF_PlayerAccount firstAccount = LF_PlayerAccountManager.GetOrCreate(firstUid);
		firstAccount.SetWantedLevel(0);
		LF_PlayerAccountManager.GetOrCreate(secondUid).SetWantedLevel(0);
		ctx.True(LF_Jail.Imprison(firstUid, 120, 2, first), "the first prisoner is in the cell");
		ctx.True(LF_Jail.Imprison(secondUid, 120, 2, second), "the second prisoner is in the cell");
		ctx.True(LF_Jail.Imprison(offlineUid, 120, 2, null), "an offline prisoner serves with no body");

		ctx.True(LF_Breach.Arm(outerId, breakerUid), "layer one: the outer door");
		ctx.True(LF_Jail.IsServing(firstUid), "blowing the outer door frees nobody");
		// RedProof(): the cells are never charged, so nobody is freed and nothing is posted.
		if (!RedProof())
			ctx.True(LF_Breach.Arm(cellId, breakerUid), "layer two: the cells");

		ctx.False(LF_Jail.IsServing(firstUid), "the first prisoner's sentence ended");
		ctx.False(LF_Jail.IsServing(secondUid), "the second prisoner's sentence ended");
		ctx.True(LF_Jail.IsServing(offlineUid), "the offline prisoner serves on");
		ctx.True(LF_PrisonEscape.HasWantedRow(firstUid, LF_PrisonEscape.OFFENCE_ESCAPED), "a freed prisoner is posted as escaped custody");
		ctx.True(firstAccount.GetWantedLevel() >= 1, "a freed prisoner is wanted");
		ctx.False(LF_PrisonEscape.HasWantedRow(offlineUid, LF_PrisonEscape.OFFENCE_ESCAPED), "the offline prisoner is not posted");
		ctx.True(LF_CrimeRecord.Count(breakerUid, LF_JailBreak.OFFENCE_PRISON_BREACH) >= 1, "the jailbreak is on the breaker's record");
		ctx.True(vector.Distance(first.GetOrigin(), LF_Jail.GetCellOrigin()) < 1, "a freed prisoner is not moved: they run from the cell");

		// Clean up what this test made.
		LF_Jail.Advance(offlineUid, 100000);
		LF_TestCharacterFixture.Delete(first);
		LF_TestCharacterFixture.Delete(second);
		breaker.SetWantedLevel(0);
		firstAccount.SetWantedLevel(0);
		LF_PlayerAccountManager.GetOrCreate(secondUid).SetWantedLevel(0);
		LF_JailBreak.ResetForTest();
		LF_JailBreakConfig.Reload();
		LF_Jail.SyncPlacedCell();
	}
}
