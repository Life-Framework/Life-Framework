//------------------------------------------------------------------------------------------------
//! An on-duty officer searches a vehicle's storage for chain goods (#419). Offer checks read the
//! replicated duty bit; the search runs on the server, which re-checks the officer against the
//! real account and the reach and rolls the find itself (LF_ChainSearch). Classed contraband (cartons,
//! bottles) is listed on a find and seized by the next search of the same car (LF_VehicleSearchGoods, #4196).
class LF_VehicleSearchAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	//! The gated action this row is (LF_QualificationGate): the quick search needs no qualification.
	protected string GateAction()
	{
		return LF_QualificationGate.ACT_QUICK_SEARCH;
	}

	//------------------------------------------------------------------------------------------------
	//! The repeat-cooldown key; each search has its own, so a quick search does not make the full one read clean.
	protected string CooldownKey(IEntity vehicle)
	{
		return LF_ChainSearch.VehicleKey(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! The search itself; the quick one walks the car's own storage.
	protected int Search(IEntity vehicle, float roll)
	{
		return LF_ChainSearch.SearchVehicle(vehicle, roll);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police"))
			return false;

		return LF_PlayerInteraction.IsOnDutyPolice(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_PlayerInteraction.IsOnDutyPolice(user))
		{
			SetCannotPerformReason(LF_Terms.Text("#EL-Confiscate_OnDutyOnly"));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_VehicleSearchAction"))
			return;

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

		if (!LF_QualificationGate.AllowsUser(pUserEntity, GateAction()))
			return;

		int nowMs = GetGame().GetWorld().GetWorldTime();

		// A car with a find from the last search: this search seizes those goods (#4196).
		if (LF_VehicleSearchGoods.HasPending(pOwnerEntity, nowMs))
		{
			int taken = LF_VehicleSearchGoods.Seize(pOwnerEntity, nowMs);
			if (taken > 0)
				LF_Utils.NotifyEntity(pUserEntity, "#LF-VehicleSearch_Seized", "#EL-Police_Title", 4.0, taken.ToString());
			else
				LF_Utils.NotifyEntity(pUserEntity, "#LF-Search_Clean", "#EL-Police_Title", 3.0);
			return;
		}

		// A repeat inside the cooldown rolls nothing and reads clean (#472).
		int seized = 0;
		int found = 0;
		string listing;
		if (LF_ChainSearch.TryBeginSearch(CooldownKey(pOwnerEntity), nowMs))
		{
			float roll = Math.RandomFloat01();
			seized = Search(pOwnerEntity, roll);
			found = LF_VehicleSearchGoods.Search(pOwnerEntity, roll, nowMs, listing);
		}

		if (found > 0)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-VehicleSearch_Found", "#EL-Police_Title", 6.0, listing);
		else if (seized > 0)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Search_Found", "#EL-Police_Title", 3.0);
		else
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Search_Clean", "#EL-Police_Title", 3.0);
	}
}

//------------------------------------------------------------------------------------------------
//! "Full search" (#3427): the quick search and the car's hidden compartment too (#3434). Offered to
//! every on-duty officer like the quick search; the server refuses it, with a toast, unless the
//! officer holds the detective qualification (LF_QualificationGate).
class LF_VehicleSearchFullAction : LF_VehicleSearchAction
{
	//------------------------------------------------------------------------------------------------
	override protected string GateAction()
	{
		return LF_QualificationGate.ACT_FULL_SEARCH;
	}

	//------------------------------------------------------------------------------------------------
	override protected string CooldownKey(IEntity vehicle)
	{
		return LF_ChainSearch.VehicleKey(vehicle) + ":full";
	}

	//------------------------------------------------------------------------------------------------
	override protected int Search(IEntity vehicle, float roll)
	{
		return LF_VehicleCompartment.SearchFull(vehicle, roll);
	}
}
