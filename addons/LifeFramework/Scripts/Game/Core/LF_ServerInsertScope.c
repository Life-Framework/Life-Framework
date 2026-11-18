//------------------------------------------------------------------------------------------------
//! Server: open while the server itself puts a freshly spawned item into a storage (the save
//! restore, LF_InventoryUtils.AddAmount). Such an item has no carrier, exactly like an item lying
//! on the ground that a client asks to insert, and CanStoreItem gets no instigator to tell them
//! apart. A storage with its own access rule (the hideout stash) takes a carrier-less item only
//! while this scope is open (#2584). Begin and End always come in pairs around one synchronous insert.
//------------------------------------------------------------------------------------------------
class LF_ServerInsertScope
{
	protected static int s_iDepth;

	//------------------------------------------------------------------------------------------------
	static void Begin()
	{
		s_iDepth++;
	}

	//------------------------------------------------------------------------------------------------
	static void End()
	{
		if (s_iDepth > 0)
			s_iDepth--;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the insert running now is the server's own.
	static bool IsOpen()
	{
		return s_iDepth > 0;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iDepth = 0;
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
