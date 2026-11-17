//------------------------------------------------------------------------------------------------
//! A test that registers itself (#2610, option 3). The test file carries a small subclass that
//! enqueues itself from a static initializer when the script module loads, the same pattern as
//! LF_StaticResetHook (Core/LF_StaticReset.c), so adding a test touches only that test's file:
//!
//!   class LF_TestRegistration_X : LF_TestRegistration
//!   {
//!   	protected static bool s_bRegistered = LF_TestManager.Enqueue(new LF_TestRegistration_X());
//!
//!   	override LF_TestTier Tier()
//!   	{
//!   		return LF_TestTier.LOGIC;
//!   	}
//!
//!   	override LF_Test Create()
//!   	{
//!   		return new LF_Test_X();
//!   	}
//!   }
//!
//! Tier() must match the file's `// tier:` comment (gen-test-registry.ps1 checks it, and leaves a file
//! that self-registers out of LF_TestRegistrations.generated.c). LF_TestManager merges these with the
//! generated registry, drops a duplicate by class name, and slots each in by class name.
//------------------------------------------------------------------------------------------------
class LF_TestRegistration : Managed
{
	//------------------------------------------------------------------------------------------------
	LF_TestTier Tier()
	{
		return LF_TestTier.LOGIC;
	}

	//------------------------------------------------------------------------------------------------
	//! A new instance of the test; null is logged and skipped.
	LF_Test Create()
	{
		return null;
	}
}
