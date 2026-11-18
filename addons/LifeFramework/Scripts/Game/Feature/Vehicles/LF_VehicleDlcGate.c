//------------------------------------------------------------------------------------------------
//! What the DLC check said about a purchase (#3304).
//------------------------------------------------------------------------------------------------
enum LF_EVehicleDlcGate
{
	ALLOWED,
	NEEDS_DLC,
	NO_MANAGER
}

//------------------------------------------------------------------------------------------------
//! The engine side of the check, behind a seam so a LOGIC test can answer for it. The default asks
//! DlcManager, as the vanilla driver-seat block does (SCR_GetInUserAction).
//------------------------------------------------------------------------------------------------
class LF_VehicleDlcChecker
{
	//------------------------------------------------------------------------------------------------
	//! False when the engine has no DlcManager to ask.
	bool Available()
	{
		return DlcManager.Get() != null;
	}

	//------------------------------------------------------------------------------------------------
	//! True when this player owns every DLC the prefab references.
	bool CanUse(IEntity player, ResourceName prefab)
	{
		DlcManager manager = DlcManager.Get();
		if (!manager)
			return false;

		return manager.CanUseResource(player, prefab, true);
	}
}

//------------------------------------------------------------------------------------------------
//! Gate for a vehicle the base game restricts to a DLC (#3304). The Z7211 tractor is Rally DLC
//! content and vanilla refuses its driver seat to a player without the DLC, so the lot must not
//! charge for one that player cannot drive. The server asks before it charges; a refusal charges
//! nothing. A base-game vehicle is never asked, so today's lot is unchanged.
//------------------------------------------------------------------------------------------------
class LF_VehicleDlcGate
{
	protected static const string FEATURE = "Vehicle";
	static const string NEEDS_DLC_KEY = "#LF-Vehicle_NeedsRallyDlc";
	static const string NOT_SOLD_KEY = "#EL-Vehicle_NotSold";

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
	//! The rule, with the engine answer passed in. A base-game vehicle is allowed without asking; a
	//! DLC vehicle needs a checker that can answer, and a buyer it says may use the prefab.
	static LF_EVehicleDlcGate Evaluate(LF_VehicleDlcChecker checker, IEntity buyer, ResourceName prefab)
	{
		if (!IsDlcVehicle(prefab))
			return LF_EVehicleDlcGate.ALLOWED;

		if (!checker || !checker.Available())
		{
			LF_Debug.Error(FEATURE, string.Format("DLC vehicle %1 refused: no DlcManager to ask", prefab));
			return LF_EVehicleDlcGate.NO_MANAGER;
		}

		if (!checker.CanUse(buyer, prefab))
		{
			LF_Debug.Info(FEATURE, string.Format("purchase refused: %1 needs a DLC the buyer does not own, nothing charged", prefab));
			return LF_EVehicleDlcGate.NEEDS_DLC;
		}

		return LF_EVehicleDlcGate.ALLOWED;
	}

	//------------------------------------------------------------------------------------------------
	//! The lot's question, on the server before it charges. False carries the refusal key to show.
	static bool Allows(IEntity buyer, ResourceName prefab, out string reasonKey)
	{
		return AllowsWith(new LF_VehicleDlcChecker(), buyer, prefab, reasonKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Allows against a given checker (the test seam).
	static bool AllowsWith(LF_VehicleDlcChecker checker, IEntity buyer, ResourceName prefab, out string reasonKey)
	{
		LF_EVehicleDlcGate result = Evaluate(checker, buyer, prefab);
		if (result == LF_EVehicleDlcGate.ALLOWED)
			return true;

		if (result == LF_EVehicleDlcGate.NEEDS_DLC)
			reasonKey = NEEDS_DLC_KEY;
		else
			reasonKey = NOT_SOLD_KEY;

		return false;
	}
}
