//------------------------------------------------------------------------------------------------
//! A locked house's open door closes itself (#2472, part 3 of #2435). The owner who walks out and
//! leaves the door open does not leave the house open behind them: anyone may close it (the gate in
//! LF_DoorUserAction.c), and a server-side timer closes it after the delay. The lock never changes;
//! an unlocked house's door stays where people left it, and a hideout door keeps its own rules.
//!
//! The server arms the timer when a house door is opened (LF_DoorUserAction.PerformAction). The tick
//! re-reads the lock and the door when it fires, so a house locked after the door was opened is still
//! covered, and a door someone already closed is left alone.
//!
//! DELAY. The owner sets it: `houses.doorCloseDelaySeconds` (LF_HousesConfig, #2505). The default is
//! 20 seconds; 0 turns the auto-close off; a value over LF_HousesConfig.DOOR_CLOSE_MAX is clamped. A /configreload
//! applies to the next door that opens; a timer already running keeps the delay it was armed with.
//------------------------------------------------------------------------------------------------
class LF_HouseDoorClose
{
	protected static const float OPEN_SHARE = 0.5;	// lf-fixed: the same threshold the vanilla door action uses for "open"

	//------------------------------------------------------------------------------------------------
	//! \return The seconds a locked house's door may stay open (the owner's `houses.doorCloseDelaySeconds`); 0 means it never closes itself.
	static float GetDelaySeconds()
	{
		return LF_HousesConfig.DoorCloseDelaySeconds();
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the door's control value says open (the vanilla action names it "Close" then).
	static bool IsOpen(IEntity door)
	{
		if (!door)
			return false;

		BaseDoorComponent doorComponent = BaseDoorComponent.Cast(door.FindComponent(BaseDoorComponent));
		if (!doorComponent)
			return false;

		return Math.AbsFloat(doorComponent.GetControlValue()) >= OPEN_SHARE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: schedules the close for a house door that was just opened.
	static void Arm(IEntity door)
	{
		if (!Replication.IsServer() || !door)
			return;

		float delay = GetDelaySeconds();
		if (delay <= 0)
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;

		// No Remove(Tick): the queue holds every door's tick under one function. A door opened again
		// before its first tick fires is closed by that earlier tick, which is the safe side.
		queue.CallLater(Tick, delay * 1000, false, door);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: closes the door when it is still open on a locked house (Houses on). Called by the timer
	//! and by the test directly.
	//! \return True when this call closed the door.
	static bool Tick(IEntity door)
	{
		if (!Replication.IsServer() || !door || !IsOpen(door))
			return false;

		LF_LockComponent doorLock = LF_Component<LF_LockComponent>.Find(door);
		if (!doorLock || !doorLock.IsLocked() || !LF_Systems.IsEnabled(LF_LockComponent.SYSTEM))
			return false;

		BaseDoorComponent doorComponent = BaseDoorComponent.Cast(door.FindComponent(BaseDoorComponent));
		doorComponent.SetControlValue(0, RplId.Invalid());
		LF_Debug.Info("Houses", string.Format("locked house door closed itself after %1 s: house=%2", GetDelaySeconds(), doorLock.GetHouseIdentifier()));
		return true;
	}
}
