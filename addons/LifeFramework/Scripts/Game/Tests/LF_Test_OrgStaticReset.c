// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_DevGameMasterReset,LF_Test_CharacterCreationReset,LF_Test_VehicleStateReset,LF_Test_WhitelistStaticReset,LF_Test_OrgStaticReset,LF_Test_UiSheetStaticReset`: RED PROOF OK (all 6 fail with the hook on, e.g. "a world end clears the missing-file counter (expected 0, got 1)"); green with the hooks (--only, 6 passed).
// tier: LOGIC
//! organization/static-reset (#2415): the organization config cache and the open ledger page are
//! process statics. A config loaded for one world's pack outlived it; the ledger page pointed at a
//! screen the old world had torn down.
class LF_Test_OrgStaticReset : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "organization/static-reset";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OrgConfig custom = new LF_OrgConfig();
		LF_OrgConfig.UseForTest(custom);
		ctx.True(LF_OrgConfig.Get() == custom, "the cached config is the one the test installed");

		LF_StaticResetHook hook1 = new LF_OrgConfig_StaticResetHook();
		if (!RedProof())
			hook1.Apply();

		ctx.True(LF_OrgConfig.Get() != custom, "a world end drops the cached organization config");
		LF_OrgConfig.Reload();

		LF_StaticResetHook hook2 = new LF_OrgLedger_StaticResetHook();
		if (!RedProof())
			hook2.Apply();

		ctx.True(LF_OrgLedger.Current() == null, "a world end forgets the open ledger page");
		ctx.True(LF_StaticReset.Registered().Contains("LF_OrgConfig"), "a world end resets the organization config");
		ctx.True(LF_StaticReset.Registered().Contains("LF_OrgLedger"), "a world end resets the ledger page");
	}
}
