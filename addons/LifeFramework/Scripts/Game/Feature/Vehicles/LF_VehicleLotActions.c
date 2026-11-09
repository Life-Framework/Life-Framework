//------------------------------------------------------------------------------------------------
class LF_VehicleBuyAction : ScriptedUserAction
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
			LF_Debug.Warn("Vehicle", "lot sheet refused: the player is not at the lot counter");
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

		if (!LF_Component<LF_VehicleLotComponent>.Find(GetOwner()))
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

		LF_NotificationManagerComponent.NotifyPlayer(playerId, title, message);
	}
}

//------------------------------------------------------------------------------------------------
class LF_VehicleExtraKeyAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user || !GetOwner())
			return false;

		return LF_Component<LF_VehicleLotComponent>.Find(GetOwner()) != null;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pUserEntity)
			return;

		LF_VehicleLotComponent lot = LF_Component<LF_VehicleLotComponent>.Find(pOwnerEntity);
		if (!lot)
		{
			LF_Debug.Error("Vehicle", "extra key action has no lot component");
			return;
		}

		if (!LF_Garage.WithinReach(pUserEntity, pOwnerEntity))
		{
			LF_Debug.Warn("Vehicle", "extra key refused: the player is not at the lot counter");
			return;
		}

		string uid = LF_Utils.GetPlayerUID(pUserEntity);
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Vehicle", "extra key rejected: player UID is unavailable");
			return;
		}

		int cashCharged;
		if (!lot.IssueExtraKey(uid, pUserEntity, cashCharged))
		{
			LF_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_ExtraKey", "#EL-Vehicle_NotOwner");
			return;
		}

		LF_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_ExtraKey", "#EL-Vehicle_Bought");
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
class LF_VehicleStoreAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		bool near = LF_VehicleGarageComponent.IsPositionNearAnyGarage(user.GetOrigin());
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

		string uid = LF_Utils.GetPlayerUID(pUserEntity);
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Vehicle", "store rejected: player UID is unavailable");
			return;
		}

		bool inVehicle = false;
		ChimeraCharacter character = ChimeraCharacter.Cast(pUserEntity);
		if (character && character.IsInVehicle())
			inVehicle = true;

		LF_VehicleGarageComponent garage = LF_VehicleGarageComponent.FindAt(pUserEntity.GetOrigin());
		if (!LF_Garage.CanPark(true, inVehicle, garage != null))
		{
			LF_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Store", "#LF-Garage_StepOut");
			return;
		}

		// Storing takes the car off the street, so only the owner's car parked at this garage counts.
		string vehicleId = LF_VehicleOwnership.OwnedVehicleNear(uid, garage.GetOwner().GetOrigin(), LF_VehicleGarageComponent.INTERACT_RADIUS);
		if (vehicleId.IsEmpty())
		{
			LF_Debug.Warn("Vehicle", string.Format("store refused: no vehicle of %1 is parked at this garage", uid));
			LF_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Store", "#LF-Garage_VehicleTooFar");
			return;
		}

		IEntity vehicle = LF_VehicleOwnership.GetVehicle(vehicleId);
		if (!vehicle || !LF_VehicleGarageComponent.SharesGarage(garage.GetOwner().GetOrigin(), pUserEntity.GetOrigin(), vehicle.GetOrigin()))
		{
			LF_Debug.Warn("Vehicle", string.Format("store refused: vehicle %1 and player %2 are not at one garage", vehicleId, uid));
			LF_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Store", "#LF-Garage_VehicleTooFar");
			return;
		}

		LF_Garage.StopAndEject(vehicle);
		if (!LF_VehicleOwnership.Store(vehicleId, uid))
		{
			LF_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Store", "#EL-Vehicle_NotOwner");
			return;
		}

		LF_PlayerInteraction.PlayGesture(pUserEntity, LF_PlayerInteraction.GESTURE_POINT);
		LF_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Store", "#LF-Garage_Stored", 5.0, LF_ENotificationType.SUCCESS, LF_VehicleOwnership.GetPlate(vehicleId));
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
class LF_VehicleRetrieveAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		if (!LF_VehicleGarageComponent.IsPositionNearAnyGarage(user.GetOrigin()))
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

		string uid = LF_Utils.GetPlayerUID(pUserEntity);
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Vehicle", "retrieve rejected: player UID is unavailable");
			return;
		}

		LF_VehicleGarageComponent garage = LF_Component<LF_VehicleGarageComponent>.Find(pOwnerEntity);
		if (!garage || garage.GetPadId().IsEmpty())
		{
			LF_Debug.Error("Vehicle", "retrieve refused: this garage has no pad");
			return;
		}

		if (!LF_Garage.WithinReach(pUserEntity, pOwnerEntity))
		{
			LF_Debug.Warn("Vehicle", "retrieve refused: the player is not at the garage");
			return;
		}

		string vehicleId = LF_VehicleOwnership.LatestStored(uid);
		if (vehicleId.IsEmpty())
		{
			LF_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Retrieve", "#LF-Garage_NothingStored", 4.0, LF_ENotificationType.WARNING);
			return;
		}

		if (!LF_VehicleOwnership.Retrieve(vehicleId, uid, garage.GetPadId(), pUserEntity))
		{
			LF_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Retrieve", "#LF-Garage_PadBusy", 4.0, LF_ENotificationType.WARNING);
			return;
		}

		LF_PlayerInteraction.PlayGesture(pUserEntity, LF_PlayerInteraction.GESTURE_POINT);
		LF_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Retrieve", "#LF-Garage_Retrieved", 5.0, LF_ENotificationType.SUCCESS, LF_VehicleOwnership.GetPlate(vehicleId));
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
