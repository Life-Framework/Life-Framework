//------------------------------------------------------------------------------------------------
//! One hook that drops process-global (static) feature state when a world ends, so the next
//! world loaded in the same process (Workbench play restart, server mission change) starts clean.
//!
//! Called from the LF_GameModeRoleplay destructor (world teardown), next to
//! LF_PlayerAccountManager.Reset. It is NOT called at game start: placed breach targets, jail
//! cells, garages, and spawn points register in their OnPostInit, which runs before the game
//! mode starts; a start-time reset would wipe them.
//------------------------------------------------------------------------------------------------
class LF_StaticReset
{
	protected static int s_iRuns;

	//------------------------------------------------------------------------------------------------
	static void ResetAll()
	{
		LF_Breach.ResetStatics();
		LF_Jail.ResetStatics();
		LF_JailEscort.ResetStatics();
		LF_JailCellComponent.ResetStatics();
		LF_JailArrestableComponent.ResetStatics();
		LF_Contraband.Reset();
		LF_PoliceRanks.ResetStatics();
		LF_VehicleGarageComponent.ResetStatics();
		LF_VehicleSpawnPointComponent.ResetStatics();
		LF_QuantityComponent.ResetStatics();
		LF_StorageSnapshot.ResetStatics();
		LF_PersistentStorageComponent.ResetStatics();
		LF_Progression.ResetStatics();
		LF_Introductions.ResetStatics();
		LF_KnownNames.ResetStatics();
		LF_Dispatch.ResetStatics();
		LF_EventBus.ResetStatics();
		LF_FineTickets.ResetStatics();
		LF_Ledger.ResetStatics();
		LF_OrgInvites.Reset();

		s_iRuns = s_iRuns + 1;
		LF_Debug.Log("StaticReset", string.Format("feature statics reset (run %1)", s_iRuns));
	}
}
