// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_DevGameMasterReset,LF_Test_CharacterCreationReset,LF_Test_VehicleStateReset,LF_Test_WhitelistStaticReset,LF_Test_OrgStaticReset,LF_Test_UiSheetStaticReset`: RED PROOF OK (all 6 fail with the hook on, e.g. "a world end clears the missing-file counter (expected 0, got 1)"); green with the hooks (--only, 6 passed).
// tier: LOGIC
//! moderation/dev-gm-reset (#2415): the dev Game Master handler sits on SCR_EditorManagerCore, which
//! outlives the world. Without a world-end reset a production world loaded after a dev-tools world
//! in one process kept granting EDIT. The reset hook takes the handler off and clears the install
//! flag, and a grant re-checks the dev-tools gate.
class LF_Test_DevGameMasterReset : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "moderation/dev-gm-reset";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_DevGameMaster.MayGrant(true, true), "an installed handler in a dev-tools world grants");
		ctx.False(LF_DevGameMaster.MayGrant(true, false), "a production world never grants, even with a handler left installed");
		ctx.False(LF_DevGameMaster.MayGrant(false, true), "no install, no grant");

		bool wasInstalled = LF_DevGameMaster.IsInstalled();
		LF_DevGameMaster.Install(true);
		ctx.True(LF_DevGameMaster.IsInstalled(), "the dev Game Master installs in a dev-tools world");

		LF_StaticResetHook hook1 = new LF_DevGameMaster_StaticResetHook();
		if (!RedProof())
			hook1.Apply();

		ctx.False(LF_DevGameMaster.IsInstalled(), "the world-end reset uninstalls the dev Game Master");
		ctx.True(LF_StaticReset.Registered().Contains("LF_DevGameMaster"), "a world end resets the dev Game Master");

		// Put the test world back as it was.
		if (wasInstalled)
			LF_DevGameMaster.Install(true);
	}
}
