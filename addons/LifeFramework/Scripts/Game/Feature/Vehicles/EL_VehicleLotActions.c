//------------------------------------------------------------------------------------------------
class EL_VehicleBuyAction : ScriptedUserAction
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, desc: "Catalog prefab this action buys. Empty buys the first offer.", "et")]
	protected ResourceName m_sPrefab;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return LotReady(user);
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the lot sheet: every car, its price, and Buy or what is missing (LF_VehicleLotSheet.c).
	//! The purchase itself happens when the player presses Buy on a row.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pUserEntity)
			return;

		if (!LF_Garage.WithinReach(pUserEntity, pOwnerEntity))
		{
			EL_Debug.Warn("Vehicle", "lot sheet refused: the player is not at the lot counter");
			return;
		}

		LF_VehicleLot.Show(pUserEntity, pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Lot_Browse";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool LotReady(IEntity user)
	{
		if (!user || !GetOwner())
			return false;

		if (!EL_Component<EL_VehicleLotComponent>.Find(GetOwner()))
		{
			SetCannotPerformReason("#EL-Vehicle_NotSold");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Notify(IEntity user, string title, string message)
	{
		if (message.IsEmpty())
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		int playerId = playerManager.GetPlayerIdFromControlledEntity(user);
		if (playerId <= 0)
			return;

		EL_NotificationManagerComponent.NotifyPlayer(playerId, title, message);
	}
}

//------------------------------------------------------------------------------------------------
class EL_VehicleExtraKeyAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user || !GetOwner())
			return false;

		return EL_Component<EL_VehicleLotComponent>.Find(GetOwner()) != null;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pUserEntity)
			return;

		EL_VehicleLotComponent lot = EL_Component<EL_VehicleLotComponent>.Find(pOwnerEntity);
		if (!lot)
		{
			EL_Debug.Error("Vehicle", "extra key action has no lot component");
			return;
		}

		if (!LF_Garage.WithinReach(pUserEntity, pOwnerEntity))
		{
			EL_Debug.Warn("Vehicle", "extra key refused: the player is not at the lot counter");
			return;
		}

		string uid = EL_Utils.GetPlayerUID(pUserEntity);
		if (uid.IsEmpty())
		{
			EL_Debug.Error("Vehicle", "extra key rejected: player UID is unavailable");
			return;
		}

		int cashCharged;
		if (!lot.IssueExtraKey(uid, pUserEntity, cashCharged))
		{
			EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_ExtraKey", "#EL-Vehicle_NotOwner");
			return;
		}

		EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_ExtraKey", "#EL-Vehicle_Bought");
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Vehicle_ExtraKey";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected int PlayerId(IEntity user)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || !user)
			return 0;

		return playerManager.GetPlayerIdFromControlledEntity(user);
	}
}

//------------------------------------------------------------------------------------------------
class EL_VehicleStoreAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		bool near = EL_VehicleGarageComponent.IsPositionNearAnyGarage(user.GetOrigin());
		bool inVehicle = false;
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (character && character.IsInVehicle())
			inVehicle = true;

		if (!LF_Garage.ParkAllowed(true, inVehicle, near))
		{
			if (inVehicle)
				SetCannotPerformReason("#LF-Garage_StepOut");
			else
				SetCannotPerformReason("#EL-Vehicle_NoSpawn");

			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pUserEntity)
			return;

		string uid = EL_Utils.GetPlayerUID(pUserEntity);
		if (uid.IsEmpty())
		{
			EL_Debug.Error("Vehicle", "store rejected: player UID is unavailable");
			return;
		}

		bool inVehicle = false;
		ChimeraCharacter character = ChimeraCharacter.Cast(pUserEntity);
		if (character && character.IsInVehicle())
			inVehicle = true;

		EL_VehicleGarageComponent garage = EL_VehicleGarageComponent.FindAt(pUserEntity.GetOrigin());
		if (!LF_Garage.CanPark(true, inVehicle, garage != null))
		{
			EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Store", "#LF-Garage_StepOut");
			return;
		}

		// Storing takes the car off the street, so only the owner's car parked at this garage counts.
		string vehicleId = EL_VehicleOwnership.OwnedVehicleNear(uid, garage.GetOwner().GetOrigin(), EL_VehicleGarageComponent.INTERACT_RADIUS);
		if (vehicleId.IsEmpty())
		{
			EL_Debug.Warn("Vehicle", string.Format("store refused: no vehicle of %1 is parked at this garage", uid));
			EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Store", "#LF-Garage_VehicleTooFar");
			return;
		}

		IEntity vehicle = EL_VehicleOwnership.GetVehicle(vehicleId);
		if (!vehicle || !EL_VehicleGarageComponent.SharesGarage(garage.GetOwner().GetOrigin(), pUserEntity.GetOrigin(), vehicle.GetOrigin()))
		{
			EL_Debug.Warn("Vehicle", string.Format("store refused: vehicle %1 and player %2 are not at one garage", vehicleId, uid));
			EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Store", "#LF-Garage_VehicleTooFar");
			return;
		}

		LF_Garage.StopAndEject(vehicle);
		if (!EL_VehicleOwnership.Store(vehicleId, uid))
		{
			EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Store", "#EL-Vehicle_NotOwner");
			return;
		}

		LF_PlayerInteraction.PlayGesture(pUserEntity, LF_PlayerInteraction.GESTURE_POINT);
		EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Store", "#LF-Garage_Stored", 5.0, EL_ENotificationType.SUCCESS, EL_VehicleOwnership.GetPlate(vehicleId));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Vehicle_Store";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected int PlayerId(IEntity user)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || !user)
			return 0;

		return playerManager.GetPlayerIdFromControlledEntity(user);
	}
}

//------------------------------------------------------------------------------------------------
class EL_VehicleRetrieveAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		if (!EL_VehicleGarageComponent.IsPositionNearAnyGarage(user.GetOrigin()))
		{
			SetCannotPerformReason("#EL-Vehicle_NoSpawn");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pUserEntity)
			return;

		string uid = EL_Utils.GetPlayerUID(pUserEntity);
		if (uid.IsEmpty())
		{
			EL_Debug.Error("Vehicle", "retrieve rejected: player UID is unavailable");
			return;
		}

		EL_VehicleGarageComponent garage = EL_Component<EL_VehicleGarageComponent>.Find(pOwnerEntity);
		if (!garage || garage.GetPadId().IsEmpty())
		{
			EL_Debug.Error("Vehicle", "retrieve refused: this garage has no pad");
			return;
		}

		if (!LF_Garage.WithinReach(pUserEntity, pOwnerEntity))
		{
			EL_Debug.Warn("Vehicle", "retrieve refused: the player is not at the garage");
			return;
		}

		string vehicleId = EL_VehicleOwnership.LatestStored(uid);
		if (vehicleId.IsEmpty())
		{
			EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Retrieve", "#LF-Garage_NothingStored", 4.0, EL_ENotificationType.WARNING);
			return;
		}

		if (!EL_VehicleOwnership.Retrieve(vehicleId, uid, garage.GetPadId(), pUserEntity))
		{
			EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Retrieve", "#LF-Garage_PadBusy", 4.0, EL_ENotificationType.WARNING);
			return;
		}

		LF_PlayerInteraction.PlayGesture(pUserEntity, LF_PlayerInteraction.GESTURE_POINT);
		EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Retrieve", "#LF-Garage_Retrieved", 5.0, EL_ENotificationType.SUCCESS, EL_VehicleOwnership.GetPlate(vehicleId));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Vehicle_Retrieve";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected int PlayerId(IEntity user)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || !user)
			return 0;

		return playerManager.GetPlayerIdFromControlledEntity(user);
	}
}
