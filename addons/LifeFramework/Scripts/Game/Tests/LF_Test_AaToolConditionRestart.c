// red-proof: observed 2026-11-24 `tools/cli.mjs wt e2e tool-wear-item-4821` while the check ran late in the reload boot: "e2e the stored pickaxe keeps its condition across the restart (expected 42.5, got -1)", because earlier tests had loaded older saves onto the live store; sorted first, it reads the state the boot load applied.
//! persist/tool-condition-restart (#4821): after a real save, restart and load (`wt e2e`), the worn pickaxe
//! LF_Test_ZzToolConditionRestart left in a stored snapshot is back at its condition (the "toolcondition"
//! section), and the worn pickaxe it left on the ground is back at its condition
//! (LF_ToolConditionComponentSerializer). The class name sorts it first on purpose: later persistence tests
//! load older saves onto the live store, so the check must read the boot load before they run.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_AaToolConditionRestart : LF_Test
{
	override string GetName()
	{
		return "persist/tool-condition-restart";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.NotNull(LF_SaveSections.Get(LF_ToolConditionSavePayload.NAME), "toolcondition is a registered save section");

#ifdef LF_E2E_LOAD_BOOT
		if (!LF_E2E.RequiresSection(LF_ToolConditionSavePayload.NAME))
			return;

		LF_StorageSnapshotRecord stored = LF_StorageSnapshot.Get(LF_Test_ZzToolConditionRestart.E2E_KEY, LF_Test_ZzToolConditionRestart.E2E_SLOT);
		ctx.NotNull(stored, "e2e the stored snapshot with the tool survives the restart");
		if (stored && stored.m_aItems && stored.m_aItems.Count() == 1)
			ctx.EqualFloat(LF_Test_ZzToolConditionRestart.E2E_CARRIED, stored.m_aItems[0].m_fToolCondition, 0.01, "e2e the stored pickaxe keeps its condition across the restart");

		LF_Test_ZzToolConditionRestart finder = new LF_Test_ZzToolConditionRestart();
		LF_ToolConditionComponent ground = finder.FindGroundTool(LF_Test_ZzToolConditionRestart.E2E_GROUND);
		ctx.NotNull(ground, "e2e the pickaxe left on the ground is back at its condition after the restart");
		if (ground)
			LF_TestCharacterFixture.Delete(ground.GetOwner());
#endif
	}
}
