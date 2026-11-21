//------------------------------------------------------------------------------------------------
//! Felled trees grow back (#3262, design docs/roadmap/lumber.md "Felling a real map tree"). A server
//! tick every 10 s asks the felled-tree ledger (LF_FelledTrees) which trees' time has come, stands each
//! one again on the server (LF_FelledTrees.Show) and takes its key out of the table clients mirror
//! (LF_FelledTreesReplication.Retract), so every screen shows it. The save section (LF_LumberSavePayload)
//! calls HideKey for every tree it loads, so a restart does not grow a stand that was just cut; a tree the
//! map has not streamed in yet waits in a pending list the tick retries. The tick starts with the first
//! fell or load and stops when the ledger is empty. Server only: every call is a no-op elsewhere.
class LF_TreeRegrow
{
	static const string FEATURE = "Lumber";
	static const int TICK_MS = 10000;	// lf-fixed: the regrow cadence, seconds of precision on a 45 min timer
	static const int RETRY_PER_PASS = 24;	// lf-fixed: bounds the tree queries one tick makes for keys not streamed in yet

	protected static bool s_bTicking;
	protected static ref array<string> s_aPendingHide;

	//------------------------------------------------------------------------------------------------
	protected static array<string> Pending()
	{
		if (!s_aPendingHide)
			s_aPendingHide = new array<string>();

		return s_aPendingHide;
	}

	//------------------------------------------------------------------------------------------------
	//! How many loaded keys wait for their tree to stream in.
	static int PendingCount()
	{
		return Pending().Count();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsTicking()
	{
		return s_bTicking;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the 10 s tick when it is not running. Called by a fell and by a save load.
	static void EnsureTicking()
	{
		if (s_bTicking || !Replication.IsServer())
			return;

		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetCallqueue())
			return;

		s_bTicking = true;
		game.GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	static void StopTicking()
	{
		if (!s_bTicking)
			return;

		s_bTicking = false;
		ArmaReforgerScripted game = GetGame();
		if (game && game.GetCallqueue())
			game.GetCallqueue().Remove(Tick);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!Replication.IsServer())
			return;

		Regrow(LF_FelledTrees.NowS());
		RetryPending();
		if (LF_FelledTrees.Count() == 0 && Pending().IsEmpty())
			StopTicking();
	}

	//------------------------------------------------------------------------------------------------
	//! Stands every tree whose time has come on the world clock `nowS`: shows it on the server, tells the
	//! clients and clears its key. \return how many trees grew back.
	static int Regrow(int nowS)
	{
		array<string> keys = LF_FelledTrees.Ledger().TakeRegrown(nowS);
		foreach (string key : keys)
		{
			ShowKey(key);
			RemovePending(key);
			LF_FelledTreesReplication.Retract(key);
			LF_Debug.Info(FEATURE, string.Format("tree %1 grew back (%2 still down)", key, LF_FelledTrees.Count()));
		}

		return keys.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stands the map tree a key names (drawn, traceable, its collision layer back).
	//! \return false when the map does not hold that tree here.
	static bool ShowKey(string key)
	{
		IEntity tree = LF_FelledTreesReplication.FindTree(key);
		if (!tree)
		{
			LF_Debug.Warn(FEATURE, "regrow: no map tree to stand again for " + key);
			return false;
		}

		LF_FelledTrees.Show(tree, key);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes the map tree a key names out of the world again (a load of the ledger) and tells the
	//! clients. A tree not streamed in yet is hidden by the tick when it appears.
	//! \return true when the tree was found and hidden now.
	static bool HideKey(string key)
	{
		IEntity tree = LF_FelledTreesReplication.FindTree(key);
		if (tree)
			LF_FelledTrees.Hide(tree, key);
		else if (Pending().Find(key) < 0)
			Pending().Insert(key);

		LF_FelledTreesReplication.Publish(key);
		return tree != null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void RemovePending(string key)
	{
		if (!s_aPendingHide)
			return;

		int at = s_aPendingHide.Find(key);
		if (at >= 0)
			s_aPendingHide.Remove(at);
	}

	//------------------------------------------------------------------------------------------------
	//! Tries a bounded batch of loaded keys whose tree was not there; the rest wait for the next tick.
	protected static void RetryPending()
	{
		if (!s_aPendingHide || s_aPendingHide.IsEmpty())
			return;

		int tries = LF_Utils.MinInt(s_aPendingHide.Count(), RETRY_PER_PASS);
		for (int i = 0; i < tries; i++)
		{
			string key = s_aPendingHide[0];
			s_aPendingHide.Remove(0);
			if (!LF_FelledTrees.Ledger().IsFelled(key, LF_FelledTrees.NowS()))
				continue;

			IEntity tree = LF_FelledTreesReplication.FindTree(key);
			if (!tree)
			{
				s_aPendingHide.Insert(key);
				continue;
			}

			LF_FelledTrees.Hide(tree, key);
			LF_Debug.Info(FEATURE, "regrow: hid a loaded tree that streamed in late: " + key);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		StopTicking();
		s_aPendingHide = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_TreeRegrow's statics (LF_StaticReset finds it by reflection). The ledger it
//! works on is cleared by LF_FelledTrees's own hook.
class LF_TreeRegrow_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_TreeRegrow";
	}

	override void Apply()
	{
		LF_TreeRegrow.ResetStatics();
	}
}
