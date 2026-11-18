//------------------------------------------------------------------------------------------------
//! Gate for a vehicle the base game restricts to a DLC (#3304). The Z7211 tractor is Rally DLC
//! content and vanilla refuses its driver seat to a player without the DLC, so the lot must not
//! charge for one that player cannot drive.
//!
//! DlcManager answers for the licence of the machine it runs on, never for a remote player: vanilla
//! runs every DLC check on the acting client (SCR_GetInUserAction, SCR_InventoryStorageManagerComponent,
//! and the arsenal's "DLC checks can only be performed locally" in
//! SCR_ResourcePlayerControllerInventoryComponent.RpcAsk_ArsenalRequestItem). So the buyer's client
//! asks its own DlcManager (LocalOwns) and sends the answer with the buy; the server refuses before
//! any charge when the answer is no. The answer is a hint: a client that lies buys a tractor whose
//! driver seat vanilla still refuses it. A base-game vehicle is never asked, so today's lot is unchanged.
//------------------------------------------------------------------------------------------------
class LF_VehicleDlcGate
{
	protected static const string FEATURE = "Vehicle";
	static const string NEEDS_DLC_KEY = "#LF-Vehicle_NeedsRallyDlc";

	//! The Rally tractor the lot sells, and the locked child the buyer gets (EL_Z7211_red_CIV.et).
	static const ResourceName Z7211 = "{939AC713213C2BA1}Prefabs/Vehicles/Wheeled/Z7211/Z7211_red_CIV.et";
	static const ResourceName Z7211_LOCKED = "{E1A7211BE1A00001}Prefabs/Vehicles/Wheeled/Z7211/EL_Z7211_red_CIV.et";

	//! Every prefab of the Rally tractor lives under this folder (tools/validation/dlc-owned-prefabs.txt).
	protected static const string RALLY_TRACTOR_FOLDER = "/Wheeled/Z7211/";

	//------------------------------------------------------------------------------------------------
	//! True for a vehicle prefab that needs a DLC. Pure.
	static bool IsDlcVehicle(ResourceName prefab)
	{
		string path = prefab;
		return path.Contains(RALLY_TRACTOR_FOLDER);
	}

	//------------------------------------------------------------------------------------------------
	//! Client (the buyer's machine, or a listen host buying for itself): does this machine's licence
	//! cover the prefab? True for a base-game vehicle without asking. No DlcManager answers no.
	static bool LocalOwns(ResourceName prefab)
	{
		if (!IsDlcVehicle(prefab))
			return true;

		DlcManager manager = DlcManager.Get();
		if (!manager)
		{
			LF_Debug.Error(FEATURE, string.Format("DLC vehicle %1: no DlcManager to ask, sent as not owned", prefab));
			return false;
		}

		// null asks for the local instance (DlcManager.c), which is the only answer the engine gives.
		return manager.CanUseResource(null, prefab, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, before any charge: the rule with the client's answer passed in. False carries the
	//! refusal key to show. Pure.
	static bool Allows(ResourceName prefab, bool buyerOwns, out string reasonKey)
	{
		if (!IsDlcVehicle(prefab) || buyerOwns)
			return true;

		reasonKey = NEEDS_DLC_KEY;
		LF_Debug.Info(FEATURE, string.Format("purchase refused: %1 needs a DLC the buyer's client says it does not own, nothing charged", prefab));
		return false;
	}
}
