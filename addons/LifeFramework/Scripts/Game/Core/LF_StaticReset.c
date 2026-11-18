//------------------------------------------------------------------------------------------------
//! Drops process-global (static) feature state when a world ends, so the next world loaded in the
//! same process (Workbench play restart, server mission change) starts clean.
//!
//! Called from the LF_GameModeRoleplay destructor (world teardown), next to
//! LF_PlayerAccountManager.Reset. It is NOT called at game start: placed breach targets, jail
//! cells, garages, and spawn points register in their OnPostInit, which runs before the game
//! mode starts; a start-time reset would wipe them.
//!
//! Nothing is listed here and nothing registers. Each class that owns statics adds an
//! LF_StaticResetHook subclass in its own file (the feature scaffold writes it; template in
//! tools/lib/static-reset-hooks.mjs); the first ResetAll or Registered finds every subclass by
//! reflection (typename.GetInheritedTypes), spawns each once and sorts them by owner (#2755).
//! A static initializer per hook ate the Game module's one static-init function, which the engine
//! caps ("Too many instructions per function"); a central list made every feature PR touch this
//! file; a `modded class` layer per feature overflowed the script stack at about 123 (#2034).
//! validate-registries.mjs rejects all three.
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
	protected static bool s_bDiscovered;
	protected static ref array<ref LF_StaticResetHook> s_aHooks;

	//------------------------------------------------------------------------------------------------
	//! Add a hook (once per owner). Discovery adds every LF_StaticResetHook subclass already; this is
	//! for a hook object built at run time. Never call it from a static initializer (#2755).
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
	//! Find every LF_StaticResetHook subclass once, spawn it and add it, sorted by owner.
	protected static void EnsureDiscovered()
	{
		if (s_bDiscovered)
			return;

		s_bDiscovered = true;
		array<typename> types = {};
		typename base = LF_StaticResetHook;
		base.GetInheritedTypes(types);

		map<string, ref LF_StaticResetHook> byOwner = new map<string, ref LF_StaticResetHook>();
		array<string> owners = {};
		foreach (typename t : types)
		{
			if (t == base)
				continue;

			LF_StaticResetHook hook = LF_StaticResetHook.Cast(t.Spawn());
			if (!hook)
			{
				LF_Debug.Error("StaticReset", "could not spawn reset hook " + t.ToString());
				continue;
			}

			string owner = hook.Owner();
			if (owner.IsEmpty())
			{
				LF_Debug.Error("StaticReset", "reset hook " + t.ToString() + " names no owner; skipped");
				continue;
			}

			if (byOwner.Contains(owner))
				continue;

			byOwner.Insert(owner, hook);
			owners.Insert(owner);
		}

		owners.Sort();
		foreach (string o : owners)
		{
			Register(byOwner.Get(o));
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetAll()
	{
		EnsureDiscovered();
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
	//! The hooks by owner class name, without running them.
	static array<string> Registered()
	{
		EnsureDiscovered();
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
