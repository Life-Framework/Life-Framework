//------------------------------------------------------------------------------------------------
//! "Inspect vehicle" (#3433): an on-duty officer at the inspection desk (or on a car itself) stamps
//! the car beside it with a sticker valid for LF_Inspection.VALID_DAYS game days. The server re-checks
//! the officer, the reach and the car; the client only hides what the server would refuse.
class LF_InspectVehicleAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Vehicles"))
			return false;

		return LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user)))
		{
			SetCannotPerformReason("#LF-Inspection_OnDutyOnly");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Vehicles", "LF_InspectVehicleAction"))
			return;

		if (!Replication.IsServer() || !pUserEntity)
			return;

		int playerId = LF_Inspection.PlayerIdOf(pUserEntity);
		if (!LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(pUserEntity)))
		{
			LF_Debug.Warn(LF_Inspection.FEATURE, string.Format("inspect refused: player %1 is not an on-duty officer", playerId));
			return;
		}

		IEntity car = LF_Inspection.FindCar(pOwnerEntity);
		if (!car)
		{
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Inspection_Inspect", "#LF-Inspection_NoCar", 4.0, LF_ENotificationType.WARNING);
			return;
		}

		if (vector.Distance(pUserEntity.GetOrigin(), pOwnerEntity.GetOrigin()) > LF_Inspection.REACH_METRES)
		{
			LF_Debug.Warn(LF_Inspection.FEATURE, string.Format("inspect refused: player %1 is not at the desk", playerId));
			return;
		}

		string vehicleId = LF_Inspection.IdOf(car);
		int today = LF_Inspection.Today();
		if (vehicleId.IsEmpty() || today < 0)
		{
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Inspection_Inspect", "#LF-Inspection_CannotInspect", 4.0, LF_ENotificationType.WARNING);
			return;
		}

		int expiry = LF_Inspection.Inspect(vehicleId, today);
		if (expiry < 0)
		{
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Inspection_Inspect", "#LF-Inspection_CannotInspect", 4.0, LF_ENotificationType.WARNING);
			return;
		}

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Inspection_Inspect", "#LF-Inspection_Stamped", 5.0, LF_ENotificationType.SUCCESS, LF_Inspection.VALID_DAYS.ToString());
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Inspection_Inspect";
		return true;
	}
}
