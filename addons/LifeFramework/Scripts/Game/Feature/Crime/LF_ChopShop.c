//------------------------------------------------------------------------------------------------
//! Chop shop yard (#415). A stolen car is driven into a yard, taken apart by hand at a bench and
//! turned into cash and parts, for a fraction of what it is worth. Same hands-on shape as the chain
//! stations (#417): LOAD starts the job, WORK is a held action that adds one unit (so more hands
//! finish sooner), COLLECT pays out. The server decides the payout, the refusal and the removal.
//!
//! Only a hot car is taken: its stolen flag (#412) must be on. That flag is set when an engine is
//! started without its key on a car whose ignition is locked (a hotwire), so "hot" means a car
//! that was stolen, not one that was merely found: a base-game road car with no ignition lock
//! never becomes hot (#601). A refused car is left exactly as it was: nothing is stripped, locked
//! or stopped before the checks pass. A keyed owner cannot chop their own
//! car. The payout is the vehicle value (capped) times a percent, paid in carried cash through
//! the ledger. Collecting deletes the car with an effect, scraps its ownership record (no impound
//! lot would hand it back), closes any stolen report (#414), files a crime report (evidence at the
//! yard and witnesses) and publishes LF_OnVehicleChopped.
//!
//! Session state: a batch in progress and the cooldown are not saved.
//------------------------------------------------------------------------------------------------
enum LF_EChopCheck
{
	NONE,
	NO_YARD,
	NO_VEHICLE,
	OCCUPIED,
	NOT_STOLEN,
	KEYED_OWNER,
	COOLDOWN,
	BUSY,
	WRONG_STATE,
	TOO_FAR,
	TOO_FAST,
	VEHICLE_GONE
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_ChopValue
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Vehicle prefab (the spawned one, for example EL_S105_beige.et)", "et")]
	ResourceName m_Prefab;

	[Attribute("0", UIWidgets.Auto, "What the vehicle is worth, before the payout percent")]
	int m_iValue;
}

//------------------------------------------------------------------------------------------------
//! Chop shop numbers. Loaded from Configs/Crime/LF_ChopShop.conf; a pack overrides the file.
[BaseContainerProps(configRoot: true)]
class LF_ChopShopConfig
{
	static const ResourceName CONFIG_PATH = "{3F7A1C5E9B2D4608}Configs/Crime/LF_ChopShop.conf";
	static const ResourceName PARTS_PREFAB = "{7A94C1E05D3B2F68}Prefabs/Items/Resources/LF_CarParts.et";
	static const ResourceName EFFECT = "{66DAEB775AA8CA4C}Particles/Props/Dest_Prop_Metal_Medium.ptc";
	static const int FALLBACK_PERCENT = 30;
	static const int FALLBACK_CAP = 3000;
	static const int FALLBACK_VALUE = 800;
	static const int FALLBACK_WORK = 6;
	static const int FALLBACK_COOLDOWN = 300;
	static const int FALLBACK_PARTS = 2;

	[Attribute("30", UIWidgets.Auto, "Percent of the (capped) vehicle value the yard pays")]
	int m_iPayoutPercent;

	[Attribute("3000", UIWidgets.Auto, "No vehicle counts for more than this, so a truck is not an endless income")]
	int m_iValueCap;

	[Attribute("800", UIWidgets.Auto, "Value of a vehicle not in the list")]
	int m_iDefaultValue;

	[Attribute("6", UIWidgets.Auto, "Units of work one car takes; each worker adds one per held action")]
	int m_iWorkUnits;

	[Attribute("300", UIWidgets.Auto, "Seconds a yard takes no new car after a payout")]
	int m_iCooldownSeconds;

	[Attribute("2", UIWidgets.Auto, "Spare parts items handed over with the cash")]
	int m_iPartsCount;

	[Attribute("", UIWidgets.Object, "Per-vehicle values")]
	ref array<ref LF_ChopValue> m_aValues;

	protected static ref LF_ChopShopConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static LF_ChopShopConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("ChopShop", string.Format("chop shop config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_ChopShopConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("ChopShop", string.Format("chop shop config has an unexpected root class: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static int Percent()
	{
		LF_ChopShopConfig config = Get();
		if (!config)
			return FALLBACK_PERCENT;

		return config.m_iPayoutPercent;
	}

	//------------------------------------------------------------------------------------------------
	static int Cap()
	{
		LF_ChopShopConfig config = Get();
		if (!config)
			return FALLBACK_CAP;

		return config.m_iValueCap;
	}

	//------------------------------------------------------------------------------------------------
	static int WorkUnits()
	{
		LF_ChopShopConfig config = Get();
		if (!config || config.m_iWorkUnits < 1)
			return FALLBACK_WORK;

		return config.m_iWorkUnits;
	}

	//------------------------------------------------------------------------------------------------
	static int CooldownSeconds()
	{
		LF_ChopShopConfig config = Get();
		if (!config)
			return FALLBACK_COOLDOWN;

		return config.m_iCooldownSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static int PartsCount()
	{
		LF_ChopShopConfig config = Get();
		if (!config)
			return FALLBACK_PARTS;

		return config.m_iPartsCount;
	}

	//------------------------------------------------------------------------------------------------
	//! The worth of a vehicle by its prefab; the default when the prefab is not listed.
	static int ValueOf(string prefabName)
	{
		LF_ChopShopConfig config = Get();
		if (!config)
			return FALLBACK_VALUE;

		foreach (LF_ChopValue entry : config.m_aValues)
		{
			if (entry && entry.m_Prefab == prefabName)
				return entry.m_iValue;
		}

		return config.m_iDefaultValue;
	}
}

//------------------------------------------------------------------------------------------------
//! The pure rules. No entities, no time source.
class LF_ChopShopRules
{
	//------------------------------------------------------------------------------------------------
	//! The payout: the value capped, times the percent. Never more than the value.
	static int Payout(int value, int cap, int percent)
	{
		if (value <= 0 || percent <= 0)
			return 0;

		int counted = value;
		if (cap > 0)
			counted = Math.Min(value, cap);

		return Math.Clamp(counted * percent / 100, 0, counted);
	}

	//------------------------------------------------------------------------------------------------
	static bool OnCooldown(float now, float lastPayout, int cooldownSeconds)
	{
		if (lastPayout < 0)
			return false;

		return now - lastPayout < cooldownSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a worker may start a car on a bench.
	//! \param stolen The vehicle stolen flag (a car taken without its key).
	//! \param isOwned The vehicle has an owner on record.
	//! \param workerHasKey The worker holds a key that fits it.
	static LF_EChopCheck CheckLoad(bool hasVehicle, bool occupied, bool stolen, bool isOwned, bool workerHasKey, bool onCooldown, bool busy)
	{
		if (busy)
			return LF_EChopCheck.BUSY;

		if (!hasVehicle)
			return LF_EChopCheck.NO_VEHICLE;

		if (occupied)
			return LF_EChopCheck.OCCUPIED;

		if (isOwned && workerHasKey)
			return LF_EChopCheck.KEYED_OWNER;

		if (!stolen)
			return LF_EChopCheck.NOT_STOLEN;

		if (onCooldown)
			return LF_EChopCheck.COOLDOWN;

		return LF_EChopCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_EChopCheck check)
	{
		switch (check)
		{
			case LF_EChopCheck.NO_YARD: return "#LF-Chop_NoYard";
			case LF_EChopCheck.NO_VEHICLE: return "#LF-Chop_NoVehicle";
			case LF_EChopCheck.OCCUPIED: return "#LF-Chop_Occupied";
			case LF_EChopCheck.NOT_STOLEN: return "#LF-Chop_NotStolen";
			case LF_EChopCheck.KEYED_OWNER: return "#LF-Chop_KeyedOwner";
			case LF_EChopCheck.COOLDOWN: return "#LF-Chop_Cooldown";
			case LF_EChopCheck.BUSY: return "#LF-Chop_Busy";
			case LF_EChopCheck.TOO_FAR: return "#LF-Chop_TooFar";
			case LF_EChopCheck.VEHICLE_GONE: return "#LF-Chop_Gone";
		}

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Crime", description: "A chop shop bench: take a stolen car apart for cash and parts. Actions: LF_ChopAction.")]
class LF_ChopShopComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_ChopShopComponent : ScriptComponent
{
	static const string FEATURE = "ChopShop";

	[Attribute("", UIWidgets.EditBox, "Stable id of this yard, for logs and the bus")]
	protected string m_sShopId;

	[Attribute("4", UIWidgets.Auto, "Metres from the bench a worker must stand")]
	protected float m_fReach;

	[Attribute("10", UIWidgets.Auto, "Metres from the bench a car must stand to be taken in (the drop area)")]
	protected float m_fDropRadius;

	[Attribute("1200", UIWidgets.Auto, "Fastest a worker work repeats, in milliseconds")]
	protected int m_iMinTickMs;

	//! -1 empty, 0 to 99 being worked, 100 ready. Server writes, clients read.
	[RplProp()]
	protected int m_iPercent = -1;

	protected ref LF_StationProgress m_Progress = new LF_StationProgress();
	protected ref map<string, float> m_mLastTick = new map<string, float>();
	protected EntityID m_Vehicle = EntityID.INVALID;
	protected string m_sVehicleId;
	protected string m_sPlate;
	protected string m_sOwnerUid;
	protected string m_sPrefabName;
	protected int m_iPayout;
	protected float m_fLastPayout = -1;

	//------------------------------------------------------------------------------------------------
	string GetShopId()
	{
		return m_sShopId;
	}

	//------------------------------------------------------------------------------------------------
	//! -1 empty, 0 to 99 being worked, 100 ready. Same answer on the server and on a client.
	int GetPercent()
	{
		if (Replication.IsServer())
			return m_Progress.Percent();

		return m_iPercent;
	}

	//------------------------------------------------------------------------------------------------
	LF_StationProgress GetProgress()
	{
		return m_Progress;
	}

	//------------------------------------------------------------------------------------------------
	int GetPayout()
	{
		return m_iPayout;
	}

	//------------------------------------------------------------------------------------------------
	protected void Sync()
	{
		int percent = m_Progress.Percent();
		if (percent == m_iPercent)
			return;

		m_iPercent = percent;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	protected void Reset()
	{
		m_Progress.Clear();
		m_Vehicle = EntityID.INVALID;
		m_sVehicleId = string.Empty;
		m_sPlate = string.Empty;
		m_sOwnerUid = string.Empty;
		m_sPrefabName = string.Empty;
		m_iPayout = 0;
		m_mLastTick.Clear();
		Sync();
	}

	//------------------------------------------------------------------------------------------------
	protected string WorkerKey(IEntity user)
	{
		string uid = LF_Utils.GetPlayerUID(user);
		if (!uid.IsEmpty())
			return uid;

		return user.GetID().ToString();
	}

	//------------------------------------------------------------------------------------------------
	protected bool InReach(IEntity user)
	{
		return user && vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) <= m_fReach;
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest car with a lock component in the drop area.
	protected IEntity FindDropVehicle()
	{
		array<IEntity> nearby = LF_Utils.GetNearbyVehicles(GetOwner().GetOrigin(), m_fDropRadius);
		IEntity best;
		float bestDistance = 999999;
		foreach (IEntity vehicle : nearby)
		{
			if (!LF_Component<LF_VehicleLockComponent>.Find(vehicle))
				continue;

			float distance = vector.Distance(vehicle.GetOrigin(), GetOwner().GetOrigin());
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = vehicle;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a worker starts the car in the drop area.
	LF_EChopCheck ServerLoad(IEntity user, float nowSeconds)
	{
		if (!InReach(user))
			return LF_EChopCheck.TOO_FAR;

		IEntity vehicle = FindDropVehicle();
		LF_VehicleLockComponent vehicleLock;
		string vehicleId;
		bool stolen;
		bool isOwned;
		bool hasKey;
		bool occupied;
		if (vehicle)
		{
			vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(vehicle);
			vehicleId = vehicleLock.GetVehicleIdentifier();
			stolen = vehicleLock.IsStolen();
			isOwned = !LF_VehicleOwnership.GetOwner(vehicleId).IsEmpty();
			hasKey = vehicleLock.UserHasValidKey(user);
			occupied = LF_StolenVehicles.HasOccupant(vehicle);
		}

		bool busy = m_Progress.IsLoaded();
		bool cooling = LF_ChopShopRules.OnCooldown(nowSeconds, m_fLastPayout, LF_ChopShopConfig.CooldownSeconds());
		LF_EChopCheck check = LF_ChopShopRules.CheckLoad(vehicle != null, occupied, stolen, isOwned, hasKey, cooling, busy);
		if (check != LF_EChopCheck.NONE)
		{
			if (vehicle)
				LF_Debug.Info(FEATURE, string.Format("%1 refused %2: stolen=%3 owned=%4 key=%5 (%6)", m_sShopId, vehicleId, stolen, isOwned, hasKey, typename.EnumToString(LF_EChopCheck, check)));

			return check;
		}

		m_Vehicle = vehicle.GetID();
		m_sVehicleId = vehicleId;
		m_sPlate = LF_StolenVehicles.PlateOf(vehicle);
		m_sOwnerUid = LF_VehicleOwnership.GetOwner(vehicleId);
		EntityPrefabData prefabData = vehicle.GetPrefabData();
		if (prefabData)
			m_sPrefabName = prefabData.GetPrefabName();

		m_iPayout = LF_ChopShopRules.Payout(LF_ChopShopConfig.ValueOf(m_sPrefabName), LF_ChopShopConfig.Cap(), LF_ChopShopConfig.Percent());

		// The car is held for the job: engine off, doors locked.
		VehicleControllerComponent controller = LF_Component<VehicleControllerComponent>.Find(vehicle);
		if (controller)
			controller.StopEngine();

		vehicleLock.SetLocked(true);
		m_Progress.Load(LF_ChopShopConfig.WorkUnits());
		Sync();

		string uid = LF_Utils.GetPlayerUID(user);
		LF_Debug.Info(FEATURE, string.Format("%1 loaded %2 plate='%3' by %4: payout %5, needs %6 work", m_sShopId, m_sVehicleId, m_sPlate, uid, m_iPayout, m_Progress.GetNeeded()));
		return LF_EChopCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one finished held action adds one unit of work, whoever the worker is.
	LF_EChopCheck ServerWork(IEntity user, float nowSeconds)
	{
		if (!m_Progress.IsLoaded() || m_Progress.IsDone())
			return LF_EChopCheck.WRONG_STATE;

		if (!InReach(user))
			return LF_EChopCheck.TOO_FAR;

		string key = WorkerKey(user);
		float last = -100000;
		m_mLastTick.Find(key, last);
		if ((nowSeconds - last) * 1000 < m_iMinTickMs)
		{
			LF_Debug.Warn(FEATURE, string.Format("%1: work from %2 refused, it came faster than a hold can finish", m_sShopId, key));
			return LF_EChopCheck.TOO_FAST;
		}

		m_mLastTick.Set(key, nowSeconds);
		bool finished = m_Progress.AddWork(1);
		Sync();
		LF_Debug.Info(FEATURE, string.Format("%1 worked by %2: %3/%4", m_sShopId, key, m_Progress.GetWork(), m_Progress.GetNeeded()));
		if (finished)
			LF_Debug.Info(FEATURE, string.Format("%1 ready: %2 is stripped", m_sShopId, m_sVehicleId));

		return LF_EChopCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a worker takes the cash and the parts. The car goes.
	LF_EChopCheck ServerCollect(IEntity user, float nowSeconds)
	{
		if (!m_Progress.IsDone())
			return LF_EChopCheck.WRONG_STATE;

		if (!InReach(user))
			return LF_EChopCheck.TOO_FAR;

		IEntity vehicle = GetGame().GetWorld().FindEntityByID(m_Vehicle);
		if (!vehicle || vector.Distance(vehicle.GetOrigin(), GetOwner().GetOrigin()) > m_fDropRadius + 5)
		{
			LF_Debug.Warn(FEATURE, string.Format("%1: %2 left the yard before it was collected; the job is dropped", m_sShopId, m_sVehicleId));
			Reset();
			return LF_EChopCheck.VEHICLE_GONE;
		}

		string uid = LF_Utils.GetPlayerUID(user);
		int paid = LF_MoneyUtils.AddCash(user, m_iPayout, "chopshop");
		int parts;
		InventoryStorageManagerComponent inventory = LF_Component<InventoryStorageManagerComponent>.Find(user);
		for (int i = 0; i < LF_ChopShopConfig.PartsCount(); i++)
		{
			if (inventory && LF_InventoryUtils.AddItem(inventory, LF_ChopShopConfig.PARTS_PREFAB))
				parts++;
			else
				LF_Debug.Warn(FEATURE, string.Format("%1: a part did not fit in the inventory of %2", m_sShopId, uid));
		}

		vector origin = vehicle.GetOrigin();
		string vehicleId = m_sVehicleId;
		string plate = m_sPlate;
		string ownerUid = m_sOwnerUid;

		LF_StolenVehicles.Clear(vehicleId, "chopped");
		LF_VehicleOwnership.Scrap(vehicleId);
		PlayEffect(user, origin);
		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		m_fLastPayout = nowSeconds;
		Reset();

		LF_Debug.Info(FEATURE, string.Format("%1: %2 plate='%3' chopped by %4: paid %5, %6 part(s), owner %7", m_sShopId, vehicleId, plate, uid, paid, parts, ownerUid));
		LF_EventBus.Publish(LF_OnVehicleChopped.Create(uid, m_sShopId, vehicleId, plate, ownerUid, paid, parts));
		// Evidence: a crime report opens a scene at the yard and lets a witness describe the chopper.
		LF_WantedReports.Report(uid, user, LF_WantedReports.OFFENCE_CHOP);
		return LF_EChopCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected void PlayEffect(IEntity user, vector origin)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
		if (character)
			character.LF_BroadcastChopEffect(origin);
	}
}

//------------------------------------------------------------------------------------------------
enum LF_EChopMode
{
	LOAD,
	WORK,
	COLLECT
}

//------------------------------------------------------------------------------------------------
//! The three things a worker does at the bench, like the chain stations (LF_ChainStationAction):
//! LOAD starts the car in the drop area, WORK is a held action, COLLECT takes the cash and parts.
class LF_ChopAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "What this action does at the bench", "", ParamEnumArray.FromEnum(LF_EChopMode))]
	protected LF_EChopMode m_eMode;

	[Attribute("SOUND_PICK_UP", UIWidgets.EditBox, "Sound event on the bench SCR_SoundDataComponent, played when the action lands")]
	protected string m_sSound;

	protected LF_ChopShopComponent m_Shop;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Shop = LF_Component<LF_ChopShopComponent>.Find(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Shop)
			return false;

		int percent = m_Shop.GetPercent();
		if (m_eMode == LF_EChopMode.LOAD)
			return percent == LF_StationProgress.EMPTY;

		if (m_eMode == LF_EChopMode.WORK)
			return percent >= 0 && percent < LF_StationProgress.READY;

		return percent == LF_StationProgress.READY;
	}

	//------------------------------------------------------------------------------------------------
	override void OnActionStart(IEntity pUserEntity)
	{
		if (m_eMode != LF_EChopMode.WORK || !pUserEntity || pUserEntity != LF_Utils.GetLocalCharacter())
			return;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(pUserEntity.FindComponent(CharacterControllerComponent));
		if (controller && controller.CanPlayItemGesture())
			controller.TryPlayItemGesture(EItemGesture.EItemGesturePickUp);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!m_Shop || !pUserEntity)
		{
			LF_Debug.Error(LF_ChopShopComponent.FEATURE, "bench action performed without a bench or a user");
			return;
		}

		if (!Replication.IsServer())
			return;

		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		LF_EChopCheck result;
		if (m_eMode == LF_EChopMode.LOAD)
			result = m_Shop.ServerLoad(pUserEntity, now);
		else if (m_eMode == LF_EChopMode.WORK)
			result = m_Shop.ServerWork(pUserEntity, now);
		else
			result = m_Shop.ServerCollect(pUserEntity, now);

		if (result != LF_EChopCheck.NONE)
		{
			LF_Debug.Info(LF_ChopShopComponent.FEATURE, string.Format("%1 refused %2 (%3)", m_Shop.GetShopId(), typename.EnumToString(LF_EChopMode, m_eMode), typename.EnumToString(LF_EChopCheck, result)));
			string key = LF_ChopShopRules.RefusalKey(result);
			if (!key.IsEmpty())
				LF_Utils.NotifyEntity(pUserEntity, key, "#LF-Chop_Title", 4.0);

			return;
		}

		if (m_eMode == LF_EChopMode.COLLECT)
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Chop_Paid", "#LF-Chop_Title", 4.0);

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pUserEntity);
		if (character && !m_sSound.IsEmpty())
			character.LF_BroadcastStationSound("LF_ChopAction", m_sSound);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (m_eMode != LF_EChopMode.WORK || !m_Shop)
			return false;

		int percent = m_Shop.GetPercent();
		if (percent < 33)
			outName = "#LF-Chop_Work_Fresh";
		else if (percent < 80)
			outName = "#LF-Chop_Work_Mid";
		else
			outName = "#LF-Chop_Work_Late";

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The yard effect, sent through the acting character (a replicated entity every nearby machine
//! streams): each rendering machine spawns the particle locally, so nothing replicates.
modded class SCR_ChimeraCharacter
{
	//------------------------------------------------------------------------------------------------
	//! Server: play the chop effect at a position on every machine.
	void LF_BroadcastChopEffect(vector position)
	{
		Rpc(RpcDo_LF_ChopEffect, position);
		RpcDo_LF_ChopEffect(position);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Broadcast)]
	protected void RpcDo_LF_ChopEffect(vector position)
	{
		// A dedicated server renders nothing.
		if (RplSession.Mode() == RplMode.Dedicated)
			return;

		ParticleEffectEntitySpawnParams params();
		params.Transform[3] = position;
		ParticleEffectEntity.SpawnParticleEffect(LF_ChopShopConfig.EFFECT, params);
	}
}
