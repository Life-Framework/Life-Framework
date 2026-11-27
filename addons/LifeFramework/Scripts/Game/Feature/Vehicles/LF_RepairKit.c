//------------------------------------------------------------------------------------------------
//! Why a "Repair with kit" was refused (#3332).
enum LF_ERepairKitCheck
{
	NONE,
	NO_VEHICLE,
	NO_KIT,
	NOT_DAMAGED,
	WRECKED,
	TOO_FAR,
	COOLDOWN,
	BUSY
}

//------------------------------------------------------------------------------------------------
//! The numbers of one repair kit, set on the prefab: how much health a repair restores, how long the
//! work takes and how long the same car refuses another kit. A pack's kit child changes them.
[ComponentEditorProps(category: "LifeFramework/Feature/Vehicles", description: "Repair kit: how much one use restores, the work time and the per-car cooldown.")]
class LF_RepairKitComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_RepairKitComponent : ScriptComponent
{
	[Attribute("0.4", UIWidgets.Slider, desc: "Health one repair restores, as a share of full health (0.4 = 40 %).", params: "0.05 1 0.05", category: "Repair kit")]
	protected float m_fRestoreScaled;

	[Attribute("8", UIWidgets.Auto, desc: "Seconds the repair takes.", params: "0 120", category: "Repair kit")]
	protected float m_fWorkSeconds;

	[Attribute("90", UIWidgets.Auto, desc: "Seconds before the same car takes another kit (stops chain-repairing in a fight).", params: "0 3600", category: "Repair kit")]
	protected int m_iCooldownSeconds;

	//------------------------------------------------------------------------------------------------
	float GetRestoreScaled()
	{
		return Math.Clamp(m_fRestoreScaled, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	float GetWorkSeconds()
	{
		return Math.Max(m_fWorkSeconds, 0);
	}

	//------------------------------------------------------------------------------------------------
	int GetCooldownMs()
	{
		return Math.ClampInt(m_iCooldownSeconds, 0, 3600) * 1000;
	}
}

//------------------------------------------------------------------------------------------------
//! One running repair. The user and the car are held by id, never by reference.
class LF_RepairKitJob : Managed
{
	EntityID m_VehicleId;
	EntityID m_UserId;
	float m_fSeconds;
	float m_fElapsed;
}

//------------------------------------------------------------------------------------------------
//! A repair kit mends a damaged car (#3332, #648). The player holds "Repair with kit" beside the car;
//! after the work time the server raises every hit zone of the car by the kit's share (capped at full
//! health), consumes one kit and starts the car's cooldown. The server decides everything: the kit
//! is taken only when the repair applies, so a refused or interrupted job costs nothing. There is no
//! parts system: the repair is a flat share of health on the whole car.
class LF_RepairKit
{
	static const string FEATURE = "RepairKit";
	static const ResourceName KIT_PREFAB = "{3F5A9C1E7B2D4806}Prefabs/Items/Vehicles/LF_RepairKit.et";
	static const float TICK_SECONDS = 0.5; // lf-fixed: job timer step, not a server tunable
	//! The user must stay this close to the car (metres) or the job is dropped.
	static const float REACH_METRES = 5.0; // lf-fixed: a work reach, not a server tunable
	//! A car at or above this share of full health is not damaged enough to need a kit.
	static const float FULL_ABOVE = 0.995; // lf-fixed: float slack around full health

	//! Test seam: a value >= 0 replaces the kit's work time.
	static float s_fSecondsOverride = -1;

	protected static ref map<string, int> s_mLastRepair;
	protected static map<string, int> LzLastRepair()
	{
		if (!s_mLastRepair)
			s_mLastRepair = new map<string, int>();
		return s_mLastRepair;
	}
	protected static ref array<ref LF_RepairKitJob> s_aJobs;
	protected static array<ref LF_RepairKitJob> LzJobs()
	{
		if (!s_aJobs)
			s_aJobs = new array<ref LF_RepairKitJob>();
		return s_aJobs;
	}

	//------------------------------------------------------------------------------------------------
	//! The pure rule. lastMs < 0 means never repaired. health is the car's scaled health, 0..1.
	static LF_ERepairKitCheck Rules(float health, bool wrecked, bool hasKit, float distance, int lastMs, int nowMs, int cooldownMs, bool busy)
	{
		if (wrecked)
			return LF_ERepairKitCheck.WRECKED;

		if (health >= FULL_ABOVE)
			return LF_ERepairKitCheck.NOT_DAMAGED;

		if (!hasKit)
			return LF_ERepairKitCheck.NO_KIT;

		if (distance > REACH_METRES)
			return LF_ERepairKitCheck.TOO_FAR;

		if (busy)
			return LF_ERepairKitCheck.BUSY;

		if (lastMs >= 0 && nowMs - lastMs < cooldownMs)
			return LF_ERepairKitCheck.COOLDOWN;

		return LF_ERepairKitCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the health after a repair of `restore`, never above full.
	static float Restored(float health, float restore)
	{
		return Math.Clamp(health + restore, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_ERepairKitCheck check)
	{
		if (check == LF_ERepairKitCheck.NO_KIT)
			return "#LF-RepairKit_NeedKit";

		if (check == LF_ERepairKitCheck.NOT_DAMAGED)
			return "#LF-RepairKit_NotDamaged";

		if (check == LF_ERepairKitCheck.WRECKED)
			return "#LF-RepairKit_Wrecked";

		if (check == LF_ERepairKitCheck.TOO_FAR)
			return "#LF-RepairKit_TooFar";

		if (check == LF_ERepairKitCheck.COOLDOWN)
			return "#LF-RepairKit_Cooldown";

		if (check == LF_ERepairKitCheck.BUSY)
			return "#LF-RepairKit_Busy";

		return "#LF-RepairKit_NotDamaged";
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		LzLastRepair();
		s_mLastRepair.Clear();
		LzJobs();
		s_aJobs.Clear();
		s_fSecondsOverride = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The car's scaled health (its default hit zone), or 1 when it has no damage manager.
	static float HealthOf(IEntity vehicle)
	{
		DamageManagerComponent damage = LF_Component<DamageManagerComponent>.Find(vehicle);
		if (!damage)
			return 1;

		return damage.GetHealthScaled();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsWrecked(IEntity vehicle)
	{
		DamageManagerComponent damage = LF_Component<DamageManagerComponent>.Find(vehicle);
		return damage && damage.IsDestroyed();
	}

	//------------------------------------------------------------------------------------------------
	protected static string KeyOf(notnull IEntity vehicle)
	{
		RplComponent rpl = RplComponent.Cast(vehicle.FindComponent(RplComponent));
		if (rpl)
			return "v:" + rpl.Id().ToString();

		return "e:" + vehicle.GetID().ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! The first kit the character carries, or null.
	static IEntity KitOf(IEntity user)
	{
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(user);
		if (!inventory)
			return null;

		array<IEntity> kits = LF_InventoryUtils.FindItemsByPrefab(inventory, KIT_PREFAB);
		foreach (IEntity kit : kits)
		{
			if (LF_Component<LF_RepairKitComponent>.Find(kit))
				return kit;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static bool HasKit(IEntity user)
	{
		if (!user)
			return false;

		return LF_InventoryUtils.GetAmount(user, KIT_PREFAB) >= 1;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsBusy(IEntity vehicle)
	{
		if (!vehicle)
			return false;

		EntityID id = vehicle.GetID();
		foreach (LF_RepairKitJob job : LzJobs())
		{
			if (job.m_VehicleId == id)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Read-only check, used by the action's prompt and by the server before and after the work time.
	static LF_ERepairKitCheck Check(IEntity vehicle, IEntity user, int nowMs, bool ignoreBusy = false)
	{
		if (!vehicle || !user)
			return LF_ERepairKitCheck.NO_VEHICLE;

		int lastMs = -1;
		LzLastRepair();
		s_mLastRepair.Find(KeyOf(vehicle), lastMs);
		int cooldownMs = 0;
		IEntity kit = KitOf(user);
		LF_RepairKitComponent kitComponent = LF_Component<LF_RepairKitComponent>.Find(kit);
		if (kitComponent)
			cooldownMs = kitComponent.GetCooldownMs();

		bool busy = !ignoreBusy && IsBusy(vehicle);
		float distance = vector.Distance(user.GetOrigin(), vehicle.GetOrigin());
		return Rules(HealthOf(vehicle), IsWrecked(vehicle), kit != null, distance, lastMs, nowMs, cooldownMs, busy);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: raises the car now and consumes one kit, when the checks pass. No work time here; Begin
	//! is the timed path and calls this when the time is up.
	//! \return NONE when the car was repaired.
	static LF_ERepairKitCheck Try(IEntity vehicle, IEntity user, int nowMs, bool ignoreBusy = false)
	{
		LF_ERepairKitCheck check = Check(vehicle, user, nowMs, ignoreBusy);
		if (check != LF_ERepairKitCheck.NONE)
		{
			LF_Debug.Info(FEATURE, string.Format("repair refused: %1", typename.EnumToString(LF_ERepairKitCheck, check)));
			return check;
		}

		LF_RepairKitComponent kit = LF_Component<LF_RepairKitComponent>.Find(KitOf(user));
		if (!kit)
		{
			LF_Debug.Error(FEATURE, "repair refused: the kit has no LF_RepairKitComponent");
			return LF_ERepairKitCheck.NO_KIT;
		}

		// The kit is taken first and only the repair that applies reaches here; a kit that cannot be
		// taken (it left the inventory) repairs nothing.
		if (LF_InventoryUtils.RemoveAmount(user, KIT_PREFAB, 1) < 1)
		{
			LF_Debug.Warn(FEATURE, "repair refused: the kit could not be taken from the inventory");
			return LF_ERepairKitCheck.NO_KIT;
		}

		float restore = kit.GetRestoreScaled();
		float before = HealthOf(vehicle);
		Raise(vehicle, restore);
		LzLastRepair();
		s_mLastRepair.Set(KeyOf(vehicle), nowMs);
		LF_Debug.Info(FEATURE, string.Format("%1 repaired: %2 -> %3", KeyOf(vehicle), before, HealthOf(vehicle)));
		return LF_ERepairKitCheck.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Every hit zone of the car rises by `restore`, capped at full health.
	protected static void Raise(IEntity vehicle, float restore)
	{
		DamageManagerComponent damage = LF_Component<DamageManagerComponent>.Find(vehicle);
		if (!damage)
			return;

		array<HitZone> zones = {};
		damage.GetAllHitZonesInHierarchy(zones);
		foreach (HitZone zone : zones)
		{
			if (zone)
				zone.SetHealthScaled(Restored(zone.GetHealthScaled(), restore));
		}

		HitZone main = damage.GetDefaultHitZone();
		if (main && zones.Find(main) == -1)
			main.SetHealthScaled(Restored(main.GetHealthScaled(), restore));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: starts the timed repair. The job runs for the kit's work time and is dropped when the
	//! user walks off, goes down, loses the kit or the car is gone.
	static bool Begin(IEntity vehicle, IEntity user)
	{
		if (!Replication.IsServer())
			return false;

		if (!vehicle || !user)
		{
			LF_Debug.Error(FEATURE, "begin refused: vehicle or user is null");
			return false;
		}

		if (LF_CuffEnforcement.Refuse(user, "repair kit"))
			return false;

		int nowMs = GetGame().GetWorld().GetWorldTime();
		LF_ERepairKitCheck check = Check(vehicle, user, nowMs);
		if (check != LF_ERepairKitCheck.NONE)
		{
			LF_Debug.Info(FEATURE, string.Format("begin refused: %1", typename.EnumToString(LF_ERepairKitCheck, check)));
			LF_Utils.NotifyEntity(user, RefusalKey(check), "#LF-RepairKit_Title", 3.0);
			return false;
		}

		LF_RepairKitJob job = new LF_RepairKitJob();
		job.m_VehicleId = vehicle.GetID();
		job.m_UserId = user.GetID();
		LF_RepairKitComponent kit = LF_Component<LF_RepairKitComponent>.Find(KitOf(user));
		if (kit)
			job.m_fSeconds = kit.GetWorkSeconds();
		if (s_fSecondsOverride >= 0)
			job.m_fSeconds = s_fSecondsOverride;

		LzJobs();
		s_aJobs.Insert(job);
		LF_Utils.NotifyEntity(user, "#LF-RepairKit_Started", "#LF-RepairKit_Title", 3.0);
		if (job.m_fSeconds <= 0)
			Finish(job);
		else
			GetGame().GetCallqueue().CallLater(Tick, TICK_SECONDS * 1000, false, job);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick(LF_RepairKitJob job)
	{
		LzJobs();
		if (!job || s_aJobs.Find(job) == -1)
			return;

		IEntity vehicle = GetGame().GetWorld().FindEntityByID(job.m_VehicleId);
		IEntity user = GetGame().GetWorld().FindEntityByID(job.m_UserId);
		if (!vehicle || !user)
		{
			LzJobs();
			s_aJobs.RemoveItem(job);
			LF_Debug.Info(FEATURE, "repair dropped: the car or the user is gone");
			return;
		}

		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(user);
		if (controller && controller.GetLifeState() != ECharacterLifeState.ALIVE)
		{
			Drop(job, user, "the user is down");
			return;
		}

		if (LF_Restraint.IsRestrained(user))
		{
			Drop(job, user, "the user is cuffed");
			return;
		}

		if (vector.Distance(user.GetOrigin(), vehicle.GetOrigin()) > REACH_METRES)
		{
			Drop(job, user, "the user walked away");
			return;
		}

		if (!HasKit(user))
		{
			Drop(job, user, "the kit left the inventory");
			return;
		}

		job.m_fElapsed = job.m_fElapsed + TICK_SECONDS;
		if (job.m_fElapsed >= job.m_fSeconds)
		{
			Finish(job);
			return;
		}

		GetGame().GetCallqueue().CallLater(Tick, TICK_SECONDS * 1000, false, job);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Drop(LF_RepairKitJob job, IEntity user, string reason)
	{
		LzJobs();
		s_aJobs.RemoveItem(job);
		LF_Debug.Info(FEATURE, string.Format("repair dropped: %1", reason));
		if (user)
			LF_Utils.NotifyEntity(user, "#LF-RepairKit_Interrupted", "#LF-RepairKit_Title", 3.0);
	}

	//------------------------------------------------------------------------------------------------
	//! The work time is up: the server re-checks everything, then repairs and consumes the kit.
	protected static void Finish(LF_RepairKitJob job)
	{
		IEntity vehicle = GetGame().GetWorld().FindEntityByID(job.m_VehicleId);
		IEntity user = GetGame().GetWorld().FindEntityByID(job.m_UserId);
		LzJobs();
		s_aJobs.RemoveItem(job);
		if (!vehicle || !user)
			return;

		int nowMs = GetGame().GetWorld().GetWorldTime();
		LF_ERepairKitCheck result = Try(vehicle, user, nowMs, true);
		if (result == LF_ERepairKitCheck.NONE)
			LF_Utils.NotifyEntity(user, "#LF-RepairKit_Done", "#LF-RepairKit_Title", 4.0);
		else
			LF_Utils.NotifyEntity(user, RefusalKey(result), "#LF-RepairKit_Title", 3.0);
	}
}

//------------------------------------------------------------------------------------------------
//! "Repair with kit": shows on a damaged car for a player who carries a kit. The server re-checks.
class LF_RepairKitAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Vehicles"))
			return false;

		IEntity vehicle = GetOwner();
		if (!vehicle || LF_RepairKit.IsWrecked(vehicle))
			return false;

		return LF_RepairKit.HealthOf(vehicle) < LF_RepairKit.FULL_ABOVE;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		LF_ERepairKitCheck check = LF_RepairKit.Check(GetOwner(), user, GetGame().GetWorld().GetWorldTime());
		if (check == LF_ERepairKitCheck.NONE)
			return true;

		SetCannotPerformReason(LF_RepairKit.RefusalKey(check));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Vehicles", "LF_RepairKitAction"))
			return;

		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		LF_RepairKit.Begin(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-RepairKit_Action";
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_RepairKit's statics (LF_StaticReset finds it by reflection).
class LF_RepairKit_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_RepairKit";
	}

	override void Apply()
	{
		LF_RepairKit.ResetStatics();
	}
}
