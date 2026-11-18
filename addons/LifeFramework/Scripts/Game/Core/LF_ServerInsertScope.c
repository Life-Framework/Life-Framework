//------------------------------------------------------------------------------------------------
//! Server: open while the server itself puts a freshly spawned item into a storage (the save
//! restore, LF_InventoryUtils.AddAmount). Such an item has no carrier, exactly like an item lying
//! on the ground that a client asks to insert, and CanStoreItem gets no instigator to tell them
//! apart. A storage with its own access rule (the hideout stash, a locked car) takes a carrier-less
//! item only while this scope is open for that item (#2584, #2809).
//!
//! The scope is keyed to the one item being inserted, never a global switch (#2835): Begin(item)
//! returns its id and End(id) closes it, so the bypass covers that item only. A script error between
//! Begin and End skips End; the sweep queued on Begin runs at the end of the frame, closes whatever
//! is still open and logs it, so an aborted insert can never leave a locked storage open.
//------------------------------------------------------------------------------------------------
class LF_ServerInsertScope
{
	protected static ref array<EntityID> s_aOpen;
	protected static bool s_bSweepQueued;

	//------------------------------------------------------------------------------------------------
	//! Opens the scope for this item. \return The id to pass to End.
	static EntityID Begin(IEntity item)
	{
		if (!item)
		{
			LF_Debug.Warn("Inventory", "server insert scope: Begin without an item, nothing opened");
			return EntityID.INVALID;
		}

		if (!s_aOpen)
			s_aOpen = {};

		EntityID id = item.GetID();
		s_aOpen.Insert(id);
		QueueSweep();
		return id;
	}

	//------------------------------------------------------------------------------------------------
	//! Closes the scope Begin opened. Takes the id, not the entity: the insert may have merged the
	//! item into a stack and deleted it.
	static void End(EntityID id)
	{
		if (!s_aOpen || id == EntityID.INVALID)
			return;

		int index = s_aOpen.Find(id);
		if (index >= 0)
			s_aOpen.Remove(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the insert of this item running now is the server's own.
	static bool IsOpenFor(IEntity item)
	{
		if (!item || !s_aOpen)
			return false;

		return s_aOpen.Find(item.GetID()) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	static int OpenCount()
	{
		if (!s_aOpen)
			return 0;

		return s_aOpen.Count();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsSweepQueued()
	{
		return s_bSweepQueued;
	}

	//------------------------------------------------------------------------------------------------
	//! Frame end: every Begin has met its End by now; anything still open was left by an aborted insert.
	static void SweepAborted()
	{
		s_bSweepQueued = false;
		if (!s_aOpen || s_aOpen.IsEmpty())
			return;

		LF_Debug.Error("Inventory", string.Format("server insert scope left open for %1 item(s) by an aborted insert; closed", s_aOpen.Count()));
		s_aOpen.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected static void QueueSweep()
	{
		if (s_bSweepQueued)
			return;

		ArmaReforgerScripted game = GetGame();
		if (!game || !game.GetCallqueue())
			return;

		game.GetCallqueue().CallLater(LF_ServerInsertScope.SweepAborted, 0, false);
		s_bSweepQueued = true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aOpen = null;
		s_bSweepQueued = false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ServerInsertScope's statics (LF_StaticReset finds it by reflection).
class LF_ServerInsertScope_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ServerInsertScope";
	}

	override void Apply()
	{
		LF_ServerInsertScope.ResetStatics();
	}
}
