//------------------------------------------------------------------------------------------------
//! Harvest and Inspect on the plant itself (#3177). A crop's stage mesh is a local entity on each
//! machine, so its action cannot reach the server through the entity. The acting player's controller
//! sends the plant id (never a position or a crop); the server looks the row up in LF_Plants, checks
//! the caller's own position against the reach, and does the work. A second request for a plant that
//! is already harvested finds no row, so a crop is paid once.
class LF_PlantHarvestRpc
{
	//------------------------------------------------------------------------------------------------
	//! Server: one Harvest or Inspect request from a character. Tells the player the outcome and logs
	//! every refusal. spoken is the key the player was told (for a not-ripe plant, its stage words).
	static LF_EPlantResult Serve(IEntity user, string plantId, int mode, out string spoken)
	{
		spoken = "";
		if (!Replication.IsServer())
			return LF_EPlantResult.NOT_SERVER;

		if (mode != LF_EPlantActionMode.HARVEST && mode != LF_EPlantActionMode.INSPECT)
		{
			LF_Debug.Warn(LF_Plants.FEATURE, string.Format("plant request refused: mode %1 is not harvest or inspect (%2)", mode, plantId));
			return LF_EPlantResult.BAD_CALLER;
		}

		if (!user)
		{
			LF_Debug.Error(LF_Plants.FEATURE, string.Format("plant request for %1 refused: no character behind it", plantId));
			return LF_EPlantResult.BAD_CALLER;
		}

		string what = typename.EnumToString(LF_EPlantActionMode, mode);
		LF_PlantRow row = LF_Plants.Find(plantId);
		if (!row)
		{
			LF_Debug.Warn(LF_Plants.FEATURE, string.Format("%1 %2 refused for %3: no such plant", what, plantId, LF_Utils.GetPlayerUID(user)));
			spoken = "#LF-Plant_NoPlant";
			LF_Utils.NotifyEntity(user, spoken, "#LF-Plant_Title", 4.0);
			return LF_EPlantResult.NO_PLANT;
		}

		if (vector.Distance(user.GetOrigin(), row.m_vPosition) > LF_Plants.REACH)
		{
			LF_Debug.Warn(LF_Plants.FEATURE, string.Format("%1 %2 refused for %3: out of reach (%4 m)", what, plantId, LF_Utils.GetPlayerUID(user), vector.Distance(user.GetOrigin(), row.m_vPosition)));
			spoken = "#LF-Plant_TooFar";
			LF_Utils.NotifyEntity(user, spoken, "#LF-Plant_Title", 4.0);
			return LF_EPlantResult.TOO_FAR;
		}

		LF_EPlantResult result;
		if (mode == LF_EPlantActionMode.INSPECT)
		{
			result = LF_PlantAction.Inspect(user, plantId);
		}
		else
		{
			result = LF_Plants.Harvest(user, plantId);
			if (result == LF_EPlantResult.NOT_RIPE)
			{
				// Not ripe: say where it is in words instead of a bare "not yet".
				string waterKey;
				if (!LF_Plants.Describe(plantId, spoken, waterKey))
					spoken = "#LF-Plant_NotRipe";
			}
			else
			{
				spoken = LF_PlantAction.MessageKey(result, LF_EPlantPlace.OK);
			}

			if (!spoken.IsEmpty())
				LF_Utils.NotifyEntity(user, spoken, "#LF-Plant_Title", 4.0);
		}

		if (result != LF_EPlantResult.OK && result != LF_EPlantResult.CLEARED)
			LF_Debug.Info(LF_Plants.FEATURE, string.Format("%1 %2 refused for %3 (%4)", what, plantId, LF_Utils.GetPlayerUID(user), typename.EnumToString(LF_EPlantResult, result)));

		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! The player-controller leg of #3177: the stage mesh's action asks through the acting player's own
//! controller, the one entity a client may RPC on.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client (or listen host): ask the server to harvest or inspect a plant by id.
	void LF_AskPlant(string plantId, int mode)
	{
		Rpc(RpcAsk_LF_PlantHarvest, plantId, mode);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PlantHarvest(string plantId, int mode)
	{
		IEntity user = GetControlledEntity();
		if (!user)
		{
			LF_Debug.Warn(LF_Plants.FEATURE, string.Format("plant request for %1 refused for player %2: no character", plantId, GetPlayerId()));
			return;
		}

		string spoken;
		LF_PlantHarvestRpc.Serve(user, plantId, mode, spoken);
	}
}
