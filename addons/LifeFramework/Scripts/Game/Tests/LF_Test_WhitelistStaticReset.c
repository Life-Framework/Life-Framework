// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_DevGameMasterReset,LF_Test_CharacterCreationReset,LF_Test_VehicleStateReset,LF_Test_WhitelistStaticReset,LF_Test_OrgStaticReset,LF_Test_UiSheetStaticReset`: RED PROOF OK (all 6 fail with the hook on, e.g. "a world end clears the missing-file counter (expected 0, got 1)"); green with the hooks (--only, 6 passed).
// tier: LOGIC
//! whitelist/static-reset (#2415): the roster lists, the load-failed latch, the missing-file counter
//! and the save path are process statics. A world loaded after one whose roster file was unreadable
//! kept refusing saves; a test save path outlived its world.
class LF_Test_WhitelistStaticReset : LF_Test
{
	protected static const string BAD = "$profile:LifeFramework/test_whitelist_static_reset_bad.txt";
	protected static const string MISSING = "$profile:LifeFramework/test_whitelist_static_reset_missing.txt";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "whitelist/static-reset";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		FileIO.DeleteFile(MISSING);
		FileHandle handle = FileIO.OpenFile(BAD, FileMode.WRITE);
		if (handle)
		{
			handle.WriteLine("99999");
			handle.Close();
		}

		LF_WhitelistManager.SetSavePath(BAD);
		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();
		ctx.True(LF_WhitelistManager.IsLoadFailed(), "an unreadable roster file latches the load-failed flag");

		LF_WhitelistManager.SetSavePath(MISSING);
		LF_WhitelistManager.Load();
		ctx.True(LF_WhitelistManager.GetMissingFileCount() > 0, "a missing roster file is counted");

		LF_StaticResetHook hook1 = new LF_WhitelistManager_StaticResetHook();
		if (!RedProof())
			hook1.Apply();

		ctx.False(LF_WhitelistManager.IsLoadFailed(), "a world end clears the load-failed flag");
		ctx.Equal(0, LF_WhitelistManager.GetMissingFileCount(), "a world end clears the missing-file counter");
		ctx.True(LF_StaticReset.Registered().Contains("LF_WhitelistManager"), "a world end resets the whitelist");

		FileIO.DeleteFile(BAD);
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
	}
}
