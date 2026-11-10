//------------------------------------------------------------------------------------------------
//! An on-duty officer searches a vehicle's storage for chain goods (#419). Offer checks read the
//! replicated duty bit; the search runs on the server, which re-checks the officer against the
//! real account and the reach and rolls the find itself (LF_ChainSearch).
class LF_VehicleSearchAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_PlayerInteraction.IsOnDutyPolice(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_PlayerInteraction.IsOnDutyPolice(user))
		{
			SetCannotPerformReason("#EL-Confiscate_OnDutyOnly");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		if (!LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(pUserEntity)))
		{
			LF_Debug.Warn("Police", string.Format("vehicle search refused: %1 is not an on-duty officer", pUserEntity));
			return;
		}

		if (vector.Distance(pUserEntity.GetOrigin(), pOwnerEntity.GetOrigin()) > LF_ChainSearch.VEHICLE_REACH)
		{
			LF_Debug.Warn("Police", "vehicle search refused: officer is out of reach");
			return;
		}

		// A repeat inside the cooldown rolls nothing and reads clean (#472).
		int seized = 0;
		if (LF_ChainSearch.TryBeginSearch(LF_ChainSearch.VehicleKey(pOwnerEntity), GetGame().GetWorld().GetWorldTime()))
			seized = LF_ChainSearch.SearchVehicle(pOwnerEntity, Math.RandomFloat01());
		if (seized > 0)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Search_Found", "#EL-Police_Title", 3.0);
		else
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Search_Clean", "#EL-Police_Title", 3.0);
	}
}
