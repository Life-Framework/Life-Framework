//------------------------------------------------------------------------------------------------
//! The U key (LF_VehicleLockToggle). Seated: toggles the lock of the vehicle you sit in. On foot:
//! toggles your nearest keyed vehicle within reach. The client only asks; the server finds the
//! vehicle, checks the key and the reach, and plays the sound (LF_VehicleLockComponent.TryToggleBy).
//! The request rides the player's own controller, which the client owns.
modded class SCR_PlayerController
{
	protected bool m_bLF_LockKeyListening;

	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		LF_RegisterLockKeyListener();
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_RegisterLockKeyListener()
	{
		if (m_bLF_LockKeyListening || GetGame().GetPlayerController() != this)
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		input.AddActionListener("LF_VehicleLockToggle", EActionTrigger.DOWN, LF_OnLockKey);
		m_bLF_LockKeyListening = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnLockKey()
	{
		LF_AskVehicleLockToggle();
	}

	//------------------------------------------------------------------------------------------------
	//! Client entry (also the harness): ask the server to toggle the lock.
	void LF_AskVehicleLockToggle()
	{
		if (Replication.IsServer())
		{
			LF_ServerVehicleLockToggle();
			return;
		}

		Rpc(RpcAsk_LF_VehicleLockToggle);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_VehicleLockToggle()
	{
		LF_ServerVehicleLockToggle();
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ServerVehicleLockToggle()
	{
		IEntity user = GetControlledEntity();
		if (!user)
			return;

		LF_VehicleLockComponent vehicleLock = LF_VehicleLockComponent.FindForKeyPress(user);
		if (!vehicleLock)
		{
			LF_Debug.Log("VehicleLock", "key press: no keyed vehicle in reach");
			LF_NotificationManagerComponent.NotifyPlayer(GetPlayerId(), "#EL-VehicleLock_Title", "#LF-VehicleLock_NoTarget");
			return;
		}

		vehicleLock.TryToggleBy(user, "key");
	}
}
