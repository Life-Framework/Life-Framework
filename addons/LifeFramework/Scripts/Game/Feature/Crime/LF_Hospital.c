//------------------------------------------------------------------------------------------------
//! Hospital cabinet robbery. The cabinet is a forced-entry container: BreachCabinet calls
//! LF_Breach.Arm and, once that lock is open, spawns the configured medicine prefabs. A missing
//! prefab is logged and skipped.
//!
//! The hospital till is not here: a till is an LF_RobAction (cooldown, minimum police, cash payout,
//! dispatch) like every other cash robbery. The till script that used to live in this class had only
//! test callers and counted its cooldown in milliseconds against seconds (#2419).
//!
//! ALERTS. CanBreach does not notify. The cabinet alerts once inside LF_Breach.Arm. A menu refresh
//! cannot fire it.
//------------------------------------------------------------------------------------------------
class LF_HospitalCabinetState : Managed
{
	string m_sTargetId;
	ref array<ResourceName> m_aConfigured;
	ref array<ResourceName> m_aDelivered;
	bool m_bDelivered;
}

//------------------------------------------------------------------------------------------------
class LF_Hospital
{
	//! LF_Places kind of a placed till or cabinet (LF_HospitalTillComponent, LF_HospitalCabinetComponent).
	static const string PLACE_KIND = "HospitalSite";

	static const ResourceName LOOT_FIELD_DRESSING = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";
	static const ResourceName LOOT_MORPHINE = "{0D9A5DCF89AE7AA9}Prefabs/Items/Medicine/MorphineInjection_01/MorphineInjection_01.et";
	static const ResourceName LOOT_SALINE = "{00E36F41CA310E2A}Prefabs/Items/Medicine/SalineBag_01/SalineBag_US_01.et";
	static const ResourceName LOOT_TOURNIQUET = "{D70216B1B2889129}Prefabs/Items/Medicine/Tourniquet_01/Tourniquet_US_01.et";

	protected static ref map<string, ref LF_HospitalCabinetState> s_mCabinets;

	//------------------------------------------------------------------------------------------------
	static void FillDefaultLoot(notnull array<ResourceName> loot)
	{
		loot.Insert(LOOT_FIELD_DRESSING);
		loot.Insert(LOOT_MORPHINE);
		loot.Insert(LOOT_SALINE);
		loot.Insert(LOOT_TOURNIQUET);
	}

	//------------------------------------------------------------------------------------------------
	static int DefaultLootCount()
	{
		return 4;
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName DefaultLootPrefab(int index)
	{
		if (index == 0)
			return LOOT_FIELD_DRESSING;
		if (index == 1)
			return LOOT_MORPHINE;
		if (index == 2)
			return LOOT_SALINE;
		if (index == 3)
			return LOOT_TOURNIQUET;

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	static void ConfigureCabinet(string targetId, array<ResourceName> loot)
	{
		if (targetId.IsEmpty())
		{
			LF_Debug.Error("Hospital", "cabinet configure refused: empty target id");
			return;
		}

		LF_HospitalCabinetState cabinet = Cabinets().Get(targetId);
		if (!cabinet)
		{
			cabinet = new LF_HospitalCabinetState();
			cabinet.m_sTargetId = targetId;
			Cabinets().Set(targetId, cabinet);
		}

		cabinet.m_aConfigured = new array<ResourceName>();
		cabinet.m_aDelivered = new array<ResourceName>();
		cabinet.m_bDelivered = false;

		if (!loot || loot.Count() == 0)
		{
			LF_Debug.Error("Hospital", string.Format("cabinet %1 has no loot list", targetId));
			return;
		}

		foreach (ResourceName prefab : loot)
			cabinet.m_aConfigured.Insert(prefab);

		LF_Debug.Log("Hospital", string.Format("cabinet configured %1 lines=%2", targetId, cabinet.m_aConfigured.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Loads each configured line. A prefab that does not load is logged and omitted.
	static int ResolveLoot(array<ResourceName> configured, notnull array<ResourceName> resolved)
	{
		if (!configured)
		{
			LF_Debug.Error("Hospital", "cabinet loot skipped: list is missing");
			return 0;
		}

		foreach (ResourceName prefab : configured)
		{
			if (prefab.IsEmpty())
			{
				LF_Debug.Error("Hospital", "cabinet loot skipped: empty prefab");
				continue;
			}

			Resource resource = Resource.Load(prefab);
			if (!resource || !resource.IsValid())
			{
				LF_Debug.Error("Hospital", "cabinet loot skipped: prefab missing: " + prefab);
				continue;
			}

			resolved.Insert(prefab);
		}

		return resolved.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Offer check. Does not alert. The cabinet must already be a configured breach target.
	static bool CanBreach(string targetId)
	{
		if (!Cabinets().Contains(targetId))
			return false;

		return LF_Breach.CanArm(targetId);
	}

	//------------------------------------------------------------------------------------------------
	//! Arms the shared breach. When the lock is open afterwards, spawns the loot once.
	static bool BreachCabinet(string targetId, string playerUid, IEntity user = null)
	{
		if (targetId.IsEmpty() || playerUid.IsEmpty())
		{
			LF_Debug.Error("Hospital", "cabinet breach refused: target id or player uid is empty");
			return false;
		}

		LF_HospitalCabinetState cabinet = Cabinets().Get(targetId);
		if (!cabinet)
		{
			LF_Debug.Error("Hospital", string.Format("cabinet breach refused: %1 is not configured", targetId));
			return false;
		}

		if (!LF_Breach.Arm(targetId, playerUid, user))
			return false;

		if (LF_Breach.IsOpen(targetId))
			DeliverLoot(cabinet);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static int GetDeliveredCount(string targetId)
	{
		LF_HospitalCabinetState cabinet = Cabinets().Get(targetId);
		if (!cabinet || !cabinet.m_aDelivered)
			return 0;

		return cabinet.m_aDelivered.Count();
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName GetDeliveredPrefab(string targetId, int index)
	{
		LF_HospitalCabinetState cabinet = Cabinets().Get(targetId);
		if (!cabinet || !cabinet.m_aDelivered)
			return ResourceName.Empty;

		if (index < 0 || index >= cabinet.m_aDelivered.Count())
			return ResourceName.Empty;

		return cabinet.m_aDelivered.Get(index);
	}

	//------------------------------------------------------------------------------------------------
	protected static void DeliverLoot(LF_HospitalCabinetState cabinet)
	{
		if (cabinet.m_bDelivered)
		{
			LF_Debug.Log("Hospital", string.Format("cabinet loot skipped: %1 already delivered", cabinet.m_sTargetId));
			return;
		}

		cabinet.m_bDelivered = true;
		cabinet.m_aDelivered = new array<ResourceName>();

		array<ResourceName> resolved = new array<ResourceName>();
		ResolveLoot(cabinet.m_aConfigured, resolved);

		vector origin = TargetOrigin(cabinet.m_sTargetId);
		for (int i = 0; i < resolved.Count(); i++)
		{
			ResourceName prefab = resolved.Get(i);
			vector pos = origin + Vector(0.4 * i, 0.2, 0);
			IEntity spawned = LF_Utils.SpawnEntityPrefab(prefab, pos);
			if (!spawned)
			{
				LF_Debug.Error("Hospital", "cabinet loot skipped: spawn failed: " + prefab);
				continue;
			}

			cabinet.m_aDelivered.Insert(prefab);
			LF_Debug.Log("Hospital", string.Format("cabinet loot %1 from %2", prefab, cabinet.m_sTargetId));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static vector TargetOrigin(string targetId)
	{
		vector origin = "0 0 0";
		LF_Places.FindOrigin(PLACE_KIND, targetId, origin);
		return origin;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_HospitalCabinetState> Cabinets()
	{
		if (!s_mCabinets)
			s_mCabinets = new map<string, ref LF_HospitalCabinetState>();

		return s_mCabinets;
	}
}
