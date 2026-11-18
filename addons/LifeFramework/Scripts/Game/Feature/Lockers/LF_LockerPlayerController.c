//------------------------------------------------------------------------------------------------
//! Showing an open locker (#2661). The server opens the locker and sends its container to the
//! renter's client; the client opens the inventory on it, and when the inventory closes tells the
//! server, which stores the contents and deletes the transient container. The server also closes a
//! locker on its own when the renter is gone, down or walked away (LF_LockerStorageManagerComponent.WatchTick),
//! so a client that never reports is not a leak.
modded class SCR_PlayerController
{
	protected static const int LOCKER_OPEN_RETRIES = 10; // lf-fixed: a bound on a streaming race, not a game tunable
	protected static const int LOCKER_OPEN_RETRY_MS = 100; // lf-fixed: the same bound, 1 s in all
	protected RplId m_LF_LockerPendingId = RplId.Invalid();
	protected RplId m_LF_LockerContainerId = RplId.Invalid();
	protected SCR_InventoryStorageManagerComponent m_LF_LockerInventory;

	//------------------------------------------------------------------------------------------------
	//! Server: shows the container to this controller's player. The listen host handles it directly.
	void LF_SendOpenLocker(RplId containerId)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_OpenLocker(containerId);
		else
			Rpc(RpcDo_LF_OpenLocker, containerId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_OpenLocker(RplId containerId)
	{
		// A new open replaces any retry still waiting on an older container.
		GetGame().GetCallqueue().Remove(LF_RetryOpenLocker);
		m_LF_LockerPendingId = containerId;
		LF_TryOpenLocker(0);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the server spawned the container in the same call as the RPC, so on a dedicated client it can
	//! arrive before the container streams in (#3369). Retry a bounded number of times before giving up.
	protected void LF_RetryOpenLocker(int attempt)
	{
		LF_TryOpenLocker(attempt);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_TryOpenLocker(int attempt)
	{
		RplId containerId = m_LF_LockerPendingId;
		if (!containerId.IsValid())
			return;

		RplComponent containerRpl = RplComponent.Cast(Replication.FindItem(containerId));
		IEntity user = GetControlledEntity();
		if (!containerRpl || !containerRpl.GetEntity() || !user)
		{
			if (attempt < LOCKER_OPEN_RETRIES)
			{
				GetGame().GetCallqueue().CallLater(LF_RetryOpenLocker, LOCKER_OPEN_RETRY_MS, false, attempt + 1);
				return;
			}

			m_LF_LockerPendingId = RplId.Invalid();
			LF_Debug.Warn("Lockers", string.Format("locker open dropped on the client: the container or the character is gone (after %1 retries)", attempt));
			LF_ReportLockerClosed(containerId);
			return;
		}

		m_LF_LockerPendingId = RplId.Invalid();

		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(user);
		if (!inventory)
		{
			LF_Debug.Error("Lockers", "locker open dropped on the client: the character has no inventory manager");
			LF_ReportLockerClosed(containerId);
			return;
		}

		LF_StopWatchingLocker();
		m_LF_LockerContainerId = containerId;
		m_LF_LockerInventory = inventory;
		inventory.m_OnInventoryOpenInvoker.Insert(LF_OnLockerInventoryToggled);
		inventory.SetStorageToOpen(containerRpl.GetEntity());
		inventory.OpenInventory();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the inventory flips open (true) and closed (false); closed ends the locker.
	protected void LF_OnLockerInventoryToggled(bool open)
	{
		if (open)
			return;

		RplId containerId = m_LF_LockerContainerId;
		LF_StopWatchingLocker();
		LF_ReportLockerClosed(containerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_StopWatchingLocker()
	{
		if (m_LF_LockerInventory)
			m_LF_LockerInventory.m_OnInventoryOpenInvoker.Remove(LF_OnLockerInventoryToggled);

		m_LF_LockerInventory = null;
		m_LF_LockerContainerId = RplId.Invalid();
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_ReportLockerClosed(RplId containerId)
	{
		if (!containerId.IsValid())
			return;

		if (Replication.IsServer())
			RpcAsk_LF_LockerClosed(containerId);
		else
			Rpc(RpcAsk_LF_LockerClosed, containerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the client says its inventory closed. Only the renter's own controller closes their locker.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_LockerClosed(RplId containerId)
	{
		RplComponent containerRpl = RplComponent.Cast(Replication.FindItem(containerId));
		if (!containerRpl || !containerRpl.GetEntity())
			return;

		LF_LockerStorageManagerComponent manager = LF_Component<LF_LockerStorageManagerComponent>.Find(containerRpl.GetEntity());
		if (!manager)
			return;

		if (manager.GetOpener() != GetControlledEntity())
		{
			LF_Debug.Warn("Lockers", string.Format("player %1 tried to close a locker that is not theirs", GetPlayerId()));
			return;
		}

		manager.Release("closed");
	}
}
