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
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pUserEntity)
			return;

		EL_VehicleLotComponent lot = EL_Component<EL_VehicleLotComponent>.Find(pOwnerEntity);
		if (!lot)
		{
			EL_Debug.Error("Vehicle", "buy action has no lot component");
			return;
		}

		string uid = EL_Utils.GetPlayerUID(pUserEntity);
		if (uid.IsEmpty())
		{
			EL_Debug.Error("Vehicle", "buy rejected: player UID is unavailable");
			return;
		}

		ResourceName prefab = m_sPrefab;
		if (prefab.IsEmpty())
		{
			EL_VehicleOffer first = lot.GetOfferAt(0);
			if (!first)
			{
				EL_Debug.Error("Vehicle", "buy rejected: the lot catalog is empty");
				return;
			}

			prefab = first.GetPrefab();
		}

		string vehicleId;
		int cashCharged;
		string reasonKey;
		bool bought = lot.Buy(uid, prefab, pUserEntity, vehicleId, cashCharged, reasonKey);
		Notify(pUserEntity, "#EL-Vehicle_Buy", reasonKey);
		if (!bought)
			EL_Debug.Log("Vehicle", string.Format("buy action rejected uid=%1", uid));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#EL-Vehicle_Buy";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool LotReady(IEntity user)
	{
		if (!user || !GetOwner())
			return false;

		if (!EL_VehicleGarageComponent.IsPositionNearAnyGarage(user.GetOrigin()))
		{
			SetCannotPerformReason("#EL-Vehicle_NoSpawn");
			return false;
		}

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

		if (!EL_VehicleGarageComponent.IsPositionNearAnyGarage(user.GetOrigin()))
		{
			SetCannotPerformReason("#EL-Vehicle_NoSpawn");
			return false;
		}

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
			EL_Debug.Error("Vehicle", "store rejected: player UID is unavailable");
			return;
		}

		string vehicleId = EL_VehicleOwnership.LatestInWorld(uid);
		if (vehicleId.IsEmpty() || !EL_VehicleOwnership.Store(vehicleId, uid))
		{
			EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Store", "#EL-Vehicle_NotOwner");
			return;
		}

		EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Store", "#EL-Vehicle_Bought");
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

		string vehicleId = EL_VehicleOwnership.LatestStored(uid);
		if (vehicleId.IsEmpty() || !EL_VehicleOwnership.Retrieve(vehicleId, uid))
		{
			EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Retrieve", "#EL-Vehicle_NotOwner");
			return;
		}

		EL_NotificationManagerComponent.NotifyPlayer(PlayerId(pUserEntity), "#EL-Vehicle_Retrieve", "#EL-Vehicle_Bought");
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
