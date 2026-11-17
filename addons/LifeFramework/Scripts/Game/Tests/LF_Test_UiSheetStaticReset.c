// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_DevGameMasterReset,LF_Test_CharacterCreationReset,LF_Test_VehicleStateReset,LF_Test_WhitelistStaticReset,LF_Test_OrgStaticReset,LF_Test_UiSheetStaticReset`: RED PROOF OK (all 6 fail with the hook on, e.g. "a world end clears the missing-file counter (expected 0, got 1)"); green with the hooks (--only, 6 passed).
// tier: LOGIC
//! ui/sheet-static-reset (#2415): the character pick, the registration form and the ID request
//! sheet each keep their open screen in a static. A screen needs a client to open, so this test
//! proves the hooks are registered and leave no screen behind; the pointer swap itself is one line.
class LF_Test_UiSheetStaticReset : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "ui/sheet-static-reset";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<string> done = new array<string>();
		if (!RedProof())
		{
			done = LF_StaticReset.Registered();
			LF_StaticResetHook hook1 = new LF_CharacterSelectSheet_StaticResetHook();
			hook1.Apply();
			LF_StaticResetHook hook2 = new LF_RegistrationForm_StaticResetHook();
			hook2.Apply();
			LF_StaticResetHook hook3 = new LF_IdRequestSheet_StaticResetHook();
			hook3.Apply();
		}

		array<string> expect = {"LF_CharacterSelectSheet", "LF_RegistrationForm", "LF_IdRequestSheet"};
		foreach (string cls : expect)
		{
			ctx.True(done.Contains(cls), cls + " registers its world-end reset hook");
		}

		ctx.True(LF_CharacterSelectSheet.Current() == null, "no pick screen survives a world end");
		ctx.True(LF_RegistrationForm.Current() == null, "no registration form survives a world end");
		ctx.True(LF_IdRequestSheet.Current() == null, "no ID request sheet survives a world end");
	}
}
