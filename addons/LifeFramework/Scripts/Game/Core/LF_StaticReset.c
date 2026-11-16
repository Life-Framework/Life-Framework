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
//! `modded class LF_StaticResetHooks<N>` (the feature scaffold writes it and picks the bucket N from
//! the class name; template in tools/lib/static-reset-hooks.mjs). A central list made every feature PR
//! touch this file, so the ship queue ran them one after another; validate-registries.mjs rejects a
//! call added here.
//!
//! Why 8 classes: every modded layer calls super.Run, so one class with N hooks is N nested
//! frames, and the script VM overflows its stack near 120 ("Stack overflow" in LF_StaticResetHooks at
//! world teardown, 2026-11-16: the main guard blamed the merge that added the 120th hook). The buckets
//! are sibling classes, so no chain is deeper than one bucket (validate-registries caps a bucket at
//! 40 hooks).
//------------------------------------------------------------------------------------------------
class LF_StaticResetHooks : Managed
{
	//------------------------------------------------------------------------------------------------
	//! Bucket 0. Every feature's modded override calls super.Run first, inserts its class name into
	//! done, and resets its statics only when apply is true (apply false lists the hooks without
	//! running them: LF_Test_StaticResetHooks).
	void Run(notnull array<string> done, bool apply)
	{
	}
}

class LF_StaticResetHooks1 : Managed
{
	//! Bucket 1: a sibling chain, see the header. Features write `modded class LF_StaticResetHooks1`.
	void Run(notnull array<string> done, bool apply)
	{
	}
}

class LF_StaticResetHooks2 : Managed
{
	//! Bucket 2: a sibling chain, see the header. Features write `modded class LF_StaticResetHooks2`.
	void Run(notnull array<string> done, bool apply)
	{
	}
}

class LF_StaticResetHooks3 : Managed
{
	//! Bucket 3: a sibling chain, see the header. Features write `modded class LF_StaticResetHooks3`.
	void Run(notnull array<string> done, bool apply)
	{
	}
}

class LF_StaticResetHooks4 : Managed
{
	//! Bucket 4: a sibling chain, see the header. Features write `modded class LF_StaticResetHooks4`.
	void Run(notnull array<string> done, bool apply)
	{
	}
}

class LF_StaticResetHooks5 : Managed
{
	//! Bucket 5: a sibling chain, see the header. Features write `modded class LF_StaticResetHooks5`.
	void Run(notnull array<string> done, bool apply)
	{
	}
}

class LF_StaticResetHooks6 : Managed
{
	//! Bucket 6: a sibling chain, see the header. Features write `modded class LF_StaticResetHooks6`.
	void Run(notnull array<string> done, bool apply)
	{
	}
}

class LF_StaticResetHooks7 : Managed
{
	//! Bucket 7: a sibling chain, see the header. Features write `modded class LF_StaticResetHooks7`.
	void Run(notnull array<string> done, bool apply)
	{
	}
}

//------------------------------------------------------------------------------------------------
class LF_StaticReset
{
	protected static int s_iRuns;

	//------------------------------------------------------------------------------------------------
	//! Runs the buckets one after another (never one chain through another: see the header).
	protected static void RunBuckets(notnull array<string> done, bool apply)
	{
		LF_StaticResetHooks b0 = new LF_StaticResetHooks();
		b0.Run(done, apply);
		LF_StaticResetHooks1 b1 = new LF_StaticResetHooks1();
		b1.Run(done, apply);
		LF_StaticResetHooks2 b2 = new LF_StaticResetHooks2();
		b2.Run(done, apply);
		LF_StaticResetHooks3 b3 = new LF_StaticResetHooks3();
		b3.Run(done, apply);
		LF_StaticResetHooks4 b4 = new LF_StaticResetHooks4();
		b4.Run(done, apply);
		LF_StaticResetHooks5 b5 = new LF_StaticResetHooks5();
		b5.Run(done, apply);
		LF_StaticResetHooks6 b6 = new LF_StaticResetHooks6();
		b6.Run(done, apply);
		LF_StaticResetHooks7 b7 = new LF_StaticResetHooks7();
		b7.Run(done, apply);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetAll()
	{
		array<string> done = new array<string>();
		RunBuckets(done, true);

		s_iRuns = s_iRuns + 1;
		LF_Debug.Log("StaticReset", string.Format("feature statics reset (run %1, %2 hooks)", s_iRuns, done.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! The registered hooks by class name, without running them.
	static array<string> Registered()
	{
		array<string> done = new array<string>();
		RunBuckets(done, false);
		return done;
	}
}
