//------------------------------------------------------------------------------------------------
//! "Check the sticker" (#3433): an on-duty officer reads the inspection sticker of the car beside the
//! desk (or the car itself): valid, expired or none. Read only; the answer is a toast to the officer.
class LF_CheckStickerAction : ScriptedUserAction
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
		if (LF_Systems.Refuses("Vehicles", "LF_CheckStickerAction"))
			return;

		if (!Replication.IsServer() || !pUserEntity)
			return;

		int playerId = LF_Inspection.PlayerIdOf(pUserEntity);
		if (!LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(pUserEntity)))
		{
			LF_Debug.Warn(LF_Inspection.FEATURE, string.Format("sticker check refused: player %1 is not an on-duty officer", playerId));
			return;
		}

		IEntity car = LF_Inspection.FindCar(pOwnerEntity);
		if (!car)
		{
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Inspection_Check", "#LF-Inspection_NoCar", 4.0, LF_ENotificationType.WARNING);
			return;
		}

		int today = LF_Inspection.Today();
		if (today < 0)
		{
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Inspection_Check", "#LF-Inspection_CannotInspect", 4.0, LF_ENotificationType.WARNING);
			return;
		}

		LF_EInspectionStatus status = LF_Inspection.Check(LF_Inspection.IdOf(car), today);
		string message = "#LF-Inspection_None";
		LF_ENotificationType type = LF_ENotificationType.WARNING;
		if (status == LF_EInspectionStatus.VALID)
		{
			message = "#LF-Inspection_Valid";
			type = LF_ENotificationType.SUCCESS;
		}
		else if (status == LF_EInspectionStatus.EXPIRED)
		{
			message = "#LF-Inspection_Expired";
		}

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Inspection_Check", message, 5.0, type);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Inspection_Check";
		return true;
	}
}
