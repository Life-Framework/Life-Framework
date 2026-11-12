// LF_Garage.c — garage it to keep it (decision 2026-11-07).
//
// A vehicle survives a restart only when it is stored in a garage. Parking at the garage takes
// the car off the street into its record; the key stays in the owner's pocket. Retrieve places
// it on the garage's own pad. At restart every owned vehicle that was not stored is at the
// impound lot (LF_VehicleOwnership.IsImpounded), and the impound desk hands it back for the
// configured fee in carried cash. Reading the garage, or the impound desk, opens a ledger card
// that lists the reader's vehicles: model, plate, and where each one is.
//
// The lot sells; the garage and the impound desk do not. The old catalog screen here duplicated
// the lot's buy path and was unreachable from the world, so it is gone.

//------------------------------------------------------------------------------------------------
//! Garage and impound values. Loaded from Configs/Vehicles/LF_Garage.conf.
[BaseContainerProps(configRoot: true)]
class LF_GarageConfig
{
	static const ResourceName CONFIG_PATH = "{6A1E5C0D2B7F4E93}Configs/Vehicles/LF_Garage.conf";
	//! Used when the config does not resolve, so the desk still works and the error is logged.
	static const int FALLBACK_FEE = 250;

	[Attribute("250", UIWidgets.Auto, "Impound fee in carried cash to collect one vehicle. 0 releases for free")]
	int m_iImpoundFee;

	protected static ref LF_GarageConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_GarageConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Garage", string.Format("garage config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_GarageConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Garage", string.Format("garage config has an unexpected root class: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static int ImpoundFee()
	{
		LF_GarageConfig config = Get();
		if (!config)
			return FALLBACK_FEE;

		return config.m_iImpoundFee;
	}
}

//------------------------------------------------------------------------------------------------
class LF_Garage
{
	//! How close the player stands to a desk or counter for a server-side action to count.
	static const float DESK_REACH = 5.0;

	//------------------------------------------------------------------------------------------------
	static bool ParkAllowed(bool isOwner, bool inVehicle, bool garageNearby)
	{
		if (!isOwner)
			return false;

		if (inVehicle)
			return false;

		if (!garageNearby)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool CanPark(bool isOwner, bool inVehicle, bool garageNearby)
	{
		if (!isOwner)
		{
			LF_Debug.Log("Garage", "park refused: not the owner");
			return false;
		}

		if (inVehicle)
		{
			LF_Debug.Log("Garage", "park refused: still in the vehicle");
			return false;
		}

		if (!garageNearby)
		{
			LF_Debug.Log("Garage", "park refused: no garage nearby");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when user stands within DESK_REACH of desk.
	static bool WithinReach(IEntity user, IEntity desk)
	{
		if (!user || !desk)
			return false;

		return vector.Distance(user.GetOrigin(), desk.GetOrigin()) <= DESK_REACH;
	}

	//------------------------------------------------------------------------------------------------
	static void StopAndEject(IEntity vehicle)
	{
		if (!vehicle)
		{
			LF_Debug.Error("Garage", "park skipped: no vehicle");
			return;
		}

		VehicleControllerComponent controller = VehicleControllerComponent.Cast(vehicle.FindComponent(VehicleControllerComponent));
		if (controller)
			controller.StopEngine();

		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartments)
		{
			LF_Debug.Error("Garage", "park skipped: no compartments");
			return;
		}

		array<IEntity> occupants = {};
		compartments.GetOccupants(occupants);
		foreach (IEntity occupant : occupants)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(occupant);
			if (!character)
				continue;

			CompartmentAccessComponent access = character.GetCompartmentAccessComponent();
			if (!access)
				continue;

			if (!access.GetOutVehicle(EGetOutType.TELEPORT, 0, ECloseDoorAfterActions.CLOSE_DOOR, false))
				LF_Debug.Error("Garage", "park eject failed");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the reader's ledger as parallel arrays (LF_VehicleGarageDialog.LedgerRows order).
	static void CollectLedger(string ownerUid, notnull array<string> nameKeys, notnull array<string> plates, notnull array<int> places)
	{
		array<ref LF_VehicleGarageRow> rows = LF_VehicleGarageDialog.LedgerRows(ownerUid);
		foreach (LF_VehicleGarageRow row : rows)
		{
			nameKeys.Insert(row.m_sLabelKey);
			plates.Insert(row.m_sPlate);
			places.Insert(row.m_iPlace);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: open the reader's ledger on their machine.
	static bool ShowLedger(IEntity reader, IEntity desk)
	{
		if (!Replication.IsServer() || !reader)
			return false;

		string uid = LF_Utils.GetPlayerUID(reader);
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Garage", "ledger refused: player UID is unavailable");
			return false;
		}

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(reader);
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!controller)
		{
			LF_Debug.Warn("Garage", "ledger refused: the reader has no player controller");
			return false;
		}

		array<string> nameKeys = {};
		array<string> plates = {};
		array<int> places = {};
		CollectLedger(uid, nameKeys, plates, places);
		int ledgerFee = LF_GarageConfig.ImpoundFee();
		string nextOut = LF_VehicleOwnership.LatestImpounded(uid);
		if (!nextOut.IsEmpty())
			ledgerFee = LF_VehicleOwnership.ImpoundFeeOf(nextOut);
		controller.LF_ShowGarageLedger(nameKeys, plates, places, ledgerFee, LF_NetworkUtils.GetRplId(desk));
		LF_Debug.Info("Garage", string.Format("ledger shown to %1: %2 vehicle(s)", uid, nameKeys.Count()));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The ledger card: one paper sheet, client-only. Closes on Close, MenuBack, death, losing the
//! character, or walking CLOSE_DISTANCE meters from the desk it was read at.
class LF_GarageLedger
{
	static const float CLOSE_DISTANCE = 5;
	static const int PLACE_STORED = 0;
	static const int PLACE_IMPOUNDED = 1;
	static const int PLACE_STREET = 2;

	protected static ref LF_UIScreen s_Screen;

	//------------------------------------------------------------------------------------------------
	//! \return One ROW per vehicle (model, plate, where it is), the fee line when any is impounded.
	static array<ref LF_UIPart> BuildParts(array<string> nameKeys, array<string> plates, array<int> places, int fee)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "LedgerTitle", "#LF-Garage_LedgerTitle"));

		int count = 0;
		if (nameKeys && plates && places && nameKeys.Count() == plates.Count() && nameKeys.Count() == places.Count())
			count = nameKeys.Count();
		else if (nameKeys && nameKeys.Count() > 0)
			LF_Debug.Error("Garage", "ledger rows skipped: name, plate, and place lists differ in length");

		bool anyImpounded = false;
		for (int i = 0; i < count; i++)
		{
			string where = "#LF-Garage_LedgerStored";
			if (places.Get(i) == PLACE_IMPOUNDED)
			{
				where = "#LF-Garage_LedgerImpounded";
				anyImpounded = true;
			}
			else if (places.Get(i) == PLACE_STREET)
			{
				where = "#LF-Garage_LedgerStreet";
			}

			string line = WidgetManager.Translate("#LF-Garage_LedgerRow", WidgetManager.Translate(nameKeys.Get(i)), plates.Get(i), WidgetManager.Translate(where));
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "LedgerRow" + i, line));
		}

		if (count == 0)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "LedgerEmpty", "#LF-Garage_LedgerEmpty"));

		if (anyImpounded)
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "LedgerFee", WidgetManager.Translate("#LF-Garage_LedgerFee", fee.ToString())));

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "LedgerCloseButton", "#LF-Garage_LedgerClose"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open(array<string> nameKeys, array<string> plates, array<int> places, int fee, IEntity desk)
	{
		Hide();
		s_Screen = LF_UIScreen.Open(BuildParts(nameKeys, plates, places, fee));
		if (!s_Screen)
		{
			LF_Debug.Error("Garage", "ledger card failed to open");
			return null;
		}

		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		if (desk)
			s_Screen.CloseBeyond(CLOSE_DISTANCE, desk.GetID());
		else
			LF_Debug.Warn("Garage", "ledger card open without the desk in this client's world; no distance close");

		LF_Debug.Info("Garage", "ledger card open");
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Screen)
			return;

		s_Screen.Close();
		s_Screen = null;
	}
}

//------------------------------------------------------------------------------------------------
//! server -> owner: LF_ShowGarageLedger (RpcDo_LF_ShowGarageLedger, RplRcver.Owner). The records
//! are server-only, so the server sends the reader their own rows.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_ShowGarageLedger(array<string> nameKeys, array<string> plates, array<int> places, int fee, RplId desk)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_ShowGarageLedger(nameKeys, plates, places, fee, desk);
		else
			Rpc(RpcDo_LF_ShowGarageLedger, nameKeys, plates, places, fee, desk);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowGarageLedger(array<string> nameKeys, array<string> plates, array<int> places, int fee, RplId desk)
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error("Garage", "ledger card not opened: no workspace on this machine");
			return;
		}

		LF_GarageLedger.Open(nameKeys, plates, places, fee, LF_NetworkUtils.FindEntityByRplId(desk));
	}
}

//------------------------------------------------------------------------------------------------
//! "Read the ledger" at a garage or the impound desk.
class LF_GarageLedgerAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pUserEntity)
			return;

		LF_Garage.ShowLedger(pUserEntity, pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Garage_ReadLedger";
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The impound desk: the pad its released vehicles are placed on. The fee is LF_GarageConfig.
[ComponentEditorProps(category: "EveronLife/Feature/Vehicles", description: "Impound desk. Releases an impounded vehicle onto its pad for the configured fee.")]
class LF_ImpoundDeskComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ImpoundDeskComponent : ScriptComponent
{
	[Attribute("roadmap-impound", UIWidgets.EditBox, desc: "Pad id of the LF_VehicleSpawnPointComponent inside the impound yard.")]
	protected string m_sPadId;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (m_sPadId.IsEmpty())
			LF_Debug.Error("Vehicle", "impound desk has no pad id: collect is refused there");
	}

	//------------------------------------------------------------------------------------------------
	string GetPadId()
	{
		return m_sPadId;
	}
}

//------------------------------------------------------------------------------------------------
//! "Pay the fee and collect" at the impound desk. Releases the owner's most recently impounded
//! vehicle onto the yard pad. The server checks ownership, the impound state, and the cash.
class LF_ImpoundCollectAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pUserEntity)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		LF_ImpoundDeskComponent desk = LF_Component<LF_ImpoundDeskComponent>.Find(pOwnerEntity);
		if (!desk)
		{
			LF_Debug.Error("Vehicle", "impound collect action has no LF_ImpoundDeskComponent");
			return;
		}

		if (!LF_Garage.WithinReach(pUserEntity, pOwnerEntity))
		{
			LF_Debug.Warn("Vehicle", "impound collect refused: the player is not at the desk");
			return;
		}

		string uid = LF_Utils.GetPlayerUID(pUserEntity);
		if (uid.IsEmpty())
		{
			LF_Debug.Error("Vehicle", "impound collect refused: player UID is unavailable");
			return;
		}

		string reasonKey = "#LF-Impound_NothingHere";
		string vehicleId = LF_VehicleOwnership.LatestImpounded(uid);
		int fee = LF_GarageConfig.ImpoundFee();
		if (!vehicleId.IsEmpty())
			fee = LF_VehicleOwnership.ImpoundFeeOf(vehicleId);
		bool collected = false;
		if (!vehicleId.IsEmpty())
			collected = LF_VehicleOwnership.CollectFromImpound(vehicleId, uid, pUserEntity, fee, desk.GetPadId(), reasonKey);
		else
			LF_Debug.Info("Vehicle", string.Format("impound collect refused: nothing impounded for %1", uid));

		if (!collected)
		{
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Impound_Title", reasonKey, 4.0, LF_ENotificationType.WARNING, fee.ToString());
			return;
		}

		LF_PlayerInteraction.PlayGesture(pUserEntity, LF_PlayerInteraction.GESTURE_POINT);
		LF_PlayerInteraction.PlayHandoverSound(pUserEntity, LF_PlayerInteraction.SOUND_HANDOVER);
		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Impound_Title", reasonKey, 5.0, LF_ENotificationType.SUCCESS, LF_VehicleOwnership.GetPlate(vehicleId));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Impound_Collect";
		return true;
	}
}
