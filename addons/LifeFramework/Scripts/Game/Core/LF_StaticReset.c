//------------------------------------------------------------------------------------------------
//! Drops process-global (static) feature state when a world ends, so the next world loaded in the
//! same process (Workbench play restart, server mission change) starts clean.
//!
//! Called from the LF_GameModeRoleplay destructor (world teardown), next to
//! LF_PlayerAccountManager.Reset. It is NOT called at game start: placed breach targets, jail
//! cells, garages, and spawn points register in their OnPostInit, which runs before the game
//! mode starts; a start-time reset would wipe them.
//!
//! Nothing is listed here. Each class that owns statics adds an LF_StaticResetHook subclass in its
//! own file that registers itself from a static initializer at script load (the feature scaffold
//! writes it; template in tools/lib/static-reset-hooks.mjs). ResetAll walks the flat array.
//! A central list made every feature PR touch this file; a `modded class` layer per feature nested
//! one super call per feature and overflowed the script stack at about 123 (#2034).
//! validate-registries.mjs rejects both.
//------------------------------------------------------------------------------------------------
class LF_StaticResetHook : Managed
{
	//------------------------------------------------------------------------------------------------
	//! The class whose statics this hook drops (LF_StaticReset.Registered lists it).
	string Owner()
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Drop the owner's statics.
	void Apply()
	{
	}
}

//------------------------------------------------------------------------------------------------
class LF_StaticReset
{
	protected static int s_iRuns;
	//! No initializer on purpose: hooks register from other classes' static initializers, which may
	//! run before this class's own; Register creates the array on first use.
	protected static ref array<ref LF_StaticResetHook> s_aHooks;

	//------------------------------------------------------------------------------------------------
	//! Add a hook (once per owner). Returns true so a static bool initializer can call it.
	static bool Register(LF_StaticResetHook hook)
	{
		if (!hook)
			return false;

		if (!s_aHooks)
			s_aHooks = new array<ref LF_StaticResetHook>();

		string owner = hook.Owner();
		foreach (LF_StaticResetHook h : s_aHooks)
		{
			if (h && h.Owner() == owner)
				return true;
		}

		s_aHooks.Insert(hook);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetAll()
	{
		int count;
		if (s_aHooks)
		{
			foreach (LF_StaticResetHook hook : s_aHooks)
			{
				if (!hook)
					continue;

				hook.Apply();
				count++;
			}
		}

		s_iRuns = s_iRuns + 1;
		LF_Debug.Log("StaticReset", string.Format("feature statics reset (run %1, %2 hooks)", s_iRuns, count));
	}

	//------------------------------------------------------------------------------------------------
	//! The registered hooks by owner class name, without running them.
	static array<string> Registered()
	{
		array<string> done = new array<string>();
		if (!s_aHooks)
			return done;

		foreach (LF_StaticResetHook hook : s_aHooks)
		{
			if (hook)
				done.Insert(hook.Owner());
		}

		return done;
	}
}
