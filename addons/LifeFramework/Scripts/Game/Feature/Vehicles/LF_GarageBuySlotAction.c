//------------------------------------------------------------------------------------------------
//! Garage desk: buy one more garage slot with carried cash (#2656). The request carries nothing:
//! the server reads the buyer, checks the desk reach, the purchase rate limit and that the buyer is
//! alive and conscious, then LF_GarageSlotStore.Buy charges the next slot's price.
class LF_GarageBuySlotAction : ScriptedUserAction
{
	//! The Vehicles system is off (#1076): no prompt.
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled("Vehicles");
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user || !GetOwner())
			return false;

		return LF_Component<LF_VehicleGarageComponent>.Find(GetOwner()) != null;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Vehicles", "LF_GarageBuySlotAction"))
			return;

		if (!Replication.IsServer() || !pUserEntity)
			return;

		int playerId = PlayerId(pUserEntity);
		if (!LF_Component<LF_VehicleGarageComponent>.Find(pOwnerEntity))
		{
			LF_Debug.Error(LF_GarageSlotStore.FEATURE, "slot buy action has no LF_VehicleGarageComponent");
			return;
		}

		if (!LF_Garage.WithinReach(pUserEntity, pOwnerEntity))
		{
			LF_Debug.Warn(LF_GarageSlotStore.FEATURE, "slot buy refused: the player is not at the garage desk");
			return;
		}

		if (!LF_RateLimit.AllowCharacter(pUserEntity, LF_ERequestKind.PURCHASE, "garage slot"))
			return;

		// A dead or downed body still carries its cash; only a living, conscious buyer pays.
		if (!LF_Utils.IsAliveState(pUserEntity))
		{
			LF_Debug.Warn(LF_GarageSlotStore.FEATURE, string.Format("slot buy refused for player %1: not alive and conscious", playerId));
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Garage_BuySlot", "#LF-Garage_SlotNotAlive", 4.0, LF_ENotificationType.WARNING);
			return;
		}

		string uid = LF_Utils.GetPlayerUID(pUserEntity);
		int price;
		LF_EGarageSlotBuy result = LF_GarageSlotStore.Buy(pUserEntity, uid, price);
		if (result == LF_EGarageSlotBuy.BOUGHT)
		{
			LF_PlayerInteraction.PlayHandoverSound(pUserEntity, LF_PlayerInteraction.SOUND_HANDOVER);
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Garage_BuySlot", "#LF-Garage_SlotBought", 5.0, LF_ENotificationType.SUCCESS, LF_GarageSlots.CapOf(uid).ToString());
			return;
		}

		string reasonKey = "#LF-Garage_SlotFailed";
		string param = "";
		if (result == LF_EGarageSlotBuy.AT_MAX)
		{
			reasonKey = "#LF-Garage_SlotsMax";
			param = LF_GarageConfig.MaxSlots().ToString();
		}
		else if (result == LF_EGarageSlotBuy.SHORT_CASH)
		{
			reasonKey = "#LF-Garage_SlotShortCash";
			param = price.ToString();
		}

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Garage_BuySlot", reasonKey, 4.0, LF_ENotificationType.WARNING, param);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Garage_BuySlot";
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
