//------------------------------------------------------------------------------------------------
//! One hook that drops process-global (static) feature state when a world ends, so the next
//! world loaded in the same process (Workbench play restart, server mission change) starts clean.
//!
//! Called from the LF_GameModeRoleplay destructor (world teardown), next to
//! LF_PlayerAccountManager.Reset. It is NOT called at game start: placed breach targets, jail
//! cells, garages, and spawn points register in their OnPostInit, which runs before the game
//! mode starts; a start-time reset would wipe them.
//!
//! Nothing is listed here. Each class that owns statics registers its reset in its own file with a
//! `modded class LF_StaticResetHooks` (the feature scaffold writes it; template in
//! tools/lib/static-reset-hooks.mjs). A central list made every feature PR touch this file, so the
//! ship queue ran them one after another; validate-registries.mjs rejects a call added here.
//------------------------------------------------------------------------------------------------
class LF_StaticResetHooks : Managed
{
	//------------------------------------------------------------------------------------------------
	//! Every feature's modded override calls super.Run first, inserts its class name into done, and
	//! resets its statics only when apply is true (apply false lists the hooks without running them:
	//! LF_Test_StaticResetHooks).
	void Run(notnull array<string> done, bool apply)
	{
	}
}

//------------------------------------------------------------------------------------------------
class LF_StaticReset
{
	protected static int s_iRuns;

	//------------------------------------------------------------------------------------------------
	static void ResetAll()
	{
		array<string> done = new array<string>();
		LF_StaticResetHooks hooks = new LF_StaticResetHooks();
		hooks.Run(done, true);

		s_iRuns = s_iRuns + 1;
		LF_Debug.Log("StaticReset", string.Format("feature statics reset (run %1, %2 hooks)", s_iRuns, done.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! The registered hooks by class name, without running them.
	static array<string> Registered()
	{
		array<string> done = new array<string>();
		LF_StaticResetHooks hooks = new LF_StaticResetHooks();
		hooks.Run(done, false);
		return done;
	}
}
