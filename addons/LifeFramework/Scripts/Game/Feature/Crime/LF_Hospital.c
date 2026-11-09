//------------------------------------------------------------------------------------------------
//! Hospital robbery. The till uses the cash-robbery gates (civilian, cooldown,
//! minimum police on duty) and pays MoneyStack. The cabinet is a forced-entry
//! container: BreachCabinet calls LF_Breach.Arm and, once that lock is open,
//! spawns the configured medicine prefabs. A missing prefab is logged and skipped.
//!
//! ALERTS. CanRobTill and CanBreach do not notify. The till alerts once inside
//! RobTill. The cabinet alerts once inside LF_Breach.Arm. A menu refresh cannot
//! fire either.
//!
//! CASH. The payout is LF_MoneyUtils.PREFAB_CASH. Nothing in this feature deposits
//! to LF_BankAccount. The ATM stays the only cash-to-account boundary.
//------------------------------------------------------------------------------------------------
class LF_HospitalTillState : Managed
{
	string m_sTargetId;
	int m_iPayout;
	float m_fCooldown;
	int m_iMinPolice;
	float m_fLastRobTime;
	int m_iAlerts;
	int m_iCashDelivered;
}

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
	static const string TILL_ID = "Roadmap_Hospital_Till";
	static const string CABINET_ID = "Roadmap_Hospital_Cabinet";
	static const int DEFAULT_PAYOUT = 200;
	static const float DEFAULT_COOLDOWN = 60;

	static const ResourceName LOOT_FIELD_DRESSING = "{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et";
	static const ResourceName LOOT_MORPHINE = "{0D9A5DCF89AE7AA9}Prefabs/Items/Medicine/MorphineInjection_01/MorphineInjection_01.et";
	static const ResourceName LOOT_SALINE = "{00E36F41CA310E2A}Prefabs/Items/Medicine/SalineBag_01/SalineBag_US_01.et";
	static const ResourceName LOOT_TOURNIQUET = "{D70216B1B2889129}Prefabs/Items/Medicine/Tourniquet_01/Tourniquet_US_01.et";

	protected static ref map<string, ref LF_HospitalTillState> s_mTills;
	protected static ref map<string, ref LF_HospitalCabinetState> s_mCabinets;

	//------------------------------------------------------------------------------------------------
	static ResourceName CashPrefab()
	{
		return LF_MoneyUtils.PREFAB_CASH;
	}

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
	//! True while a successful rob is still inside cooldownSeconds. A negative lastRobTime
	//! means the till has not been robbed. A cooldown of 0 never blocks.
	static bool CooldownActive(float lastRobTime, float now, float cooldownSeconds)
	{
		if (cooldownSeconds <= 0)
			return false;

		if (lastRobTime < 0)
			return false;

		return (now - lastRobTime) < cooldownSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static void ConfigureTill(string targetId, int payout, float cooldownSeconds, int minPolice)
	{
		if (targetId.IsEmpty())
		{
			LF_Debug.Error("Hospital", "till configure refused: empty target id");
			return;
		}

		if (payout < 1)
		{
			LF_Debug.Error("Hospital", string.Format("till configure refused: %1 payout %2", targetId, payout));
			return;
		}

		LF_HospitalTillState till = Tills().Get(targetId);
		if (!till)
		{
			till = new LF_HospitalTillState();
			till.m_sTargetId = targetId;
			Tills().Set(targetId, till);
		}

		till.m_iPayout = payout;
		till.m_fCooldown = cooldownSeconds;
		till.m_iMinPolice = minPolice;
		till.m_fLastRobTime = -1;
		till.m_iAlerts = 0;
		till.m_iCashDelivered = 0;
		LF_Debug.Log("Hospital", string.Format("till configured %1 payout=%2 cooldown=%3 police=%4", targetId, payout, cooldownSeconds, minPolice));
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
	//! Offer check. Does not alert and does not start the cooldown.
	static bool CanRobTill(string targetId, string playerUid)
	{
		return TillBlockReason(targetId, playerUid).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	static string TillBlockReason(string targetId, string playerUid)
	{
		LF_HospitalTillState till = Tills().Get(targetId);
		if (!till)
			return "#EL-Hospital_TillNotConfigured";

		if (playerUid.IsEmpty())
			return "#EL-Hospital_TillCivilianOnly";

		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (accountManager)
		{
			LF_PlayerAccount account = accountManager.GetAccount(playerUid);
			if (account && account.GetFaction() != LF_Faction.CIVILIAN)
				return "#EL-Hospital_TillCivilianOnly";
		}

		if (CooldownActive(till.m_fLastRobTime, Now(), till.m_fCooldown))
			return "#EL-Hospital_TillCooldown";

		if (CountPoliceOnDuty() < till.m_iMinPolice)
			return "#EL-Hospital_TillNeedPolice";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Pays MoneyStack, raises wanted, and alerts once. Does not deposit to the bank.
	//! A missing body still counts the rob: the stack is spawned at the till.
	static bool RobTill(string targetId, string playerUid, IEntity player)
	{
		if (targetId.IsEmpty() || playerUid.IsEmpty())
		{
			LF_Debug.Error("Hospital", "till refused: target id or player uid is empty");
			return false;
		}

		string block = TillBlockReason(targetId, playerUid);
		if (!block.IsEmpty())
		{
			LF_Debug.Log("Hospital", string.Format("till refused %1: %2", targetId, block));
			return false;
		}

		LF_HospitalTillState till = Tills().Get(targetId);
		till.m_fLastRobTime = Now();

		int paid = PayCash(till, player);
		till.m_iCashDelivered = till.m_iCashDelivered + paid;

		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(playerUid);
		if (account)
		{
			account.IncreaseWantedLevel(1);
			LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
			if (accountManager)
				accountManager.SaveAndReleaseAccount(account);
		}

		till.m_iAlerts = till.m_iAlerts + 1;
		// The alarm sounds at the cabinet and on-duty officers are radioed (LF_Dispatch).
		LF_Dispatch.Report(playerUid, player, LF_WantedReports.OFFENCE_HOSPITAL, TargetOrigin(targetId));
		LF_Debug.Log("Hospital", string.Format("till robbed %1 cash=%2 delivered=%3", targetId, CashPrefab(), paid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static int GetTillAlertCount(string targetId)
	{
		LF_HospitalTillState till = Tills().Get(targetId);
		if (!till)
			return 0;

		return till.m_iAlerts;
	}

	//------------------------------------------------------------------------------------------------
	static int GetCashDelivered(string targetId)
	{
		LF_HospitalTillState till = Tills().Get(targetId);
		if (!till)
			return 0;

		return till.m_iCashDelivered;
	}

	//------------------------------------------------------------------------------------------------
	static int GetConfiguredPayout(string targetId)
	{
		LF_HospitalTillState till = Tills().Get(targetId);
		if (!till)
			return 0;

		return till.m_iPayout;
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
	//! On-duty police currently in the world. Does not notify them.
	static int CountPoliceOnDuty()
	{
		int count = 0;
		LF_PlayerControllerManagerCompat playerManager = LF_PlayerControllerManagerCompat.GetInstance();
		if (!playerManager)
			return 0;

		array<int> playerIds = {};
		playerManager.GetAllPlayerIds(playerIds);
		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return 0;

		foreach (int playerId : playerIds)
		{
			PlayerController pc = playerManager.GetPlayerController(playerId);
			if (!pc)
				continue;

			IEntity controlled = pc.GetControlledEntity();
			if (!controlled)
				continue;

			string playerUid = LF_Utils.GetPlayerUID(controlled);
			LF_PlayerAccount account = accountManager.GetAccount(playerUid);
			if (account && account.GetFaction() == LF_Faction.POLICE && account.IsOnDuty())
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static int PayCash(LF_HospitalTillState till, IEntity player)
	{
		if (player)
		{
			int paid = LF_MoneyUtils.AddCash(player, till.m_iPayout);
			if (paid != till.m_iPayout)
			{
				LF_Debug.Error("Hospital", string.Format("till payout short: paid %1 of %2 at %3", paid, till.m_iPayout, till.m_sTargetId));
				if (paid > 0)
					LF_MoneyUtils.RemoveCash(player, paid);
				return 0;
			}

			return paid;
		}

		vector origin = TargetOrigin(till.m_sTargetId);
		origin = origin + Vector(0, 0.9, 0);
		IEntity stack = LF_Utils.SpawnEntityPrefab(CashPrefab(), origin);
		if (!stack)
		{
			LF_Debug.Error("Hospital", string.Format("till payout skipped: MoneyStack did not spawn at %1", till.m_sTargetId));
			return 0;
		}

		LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(stack);
		if (!quantity || !quantity.SetQuantity(till.m_iPayout))
		{
			LF_Debug.Error("Hospital", string.Format("till payout skipped: MoneyStack quantity refused at %1", till.m_sTargetId));
			SCR_EntityHelper.DeleteEntityAndChildren(stack);
			return 0;
		}

		return till.m_iPayout;
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
	protected static float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	protected static vector TargetOrigin(string targetId)
	{
		vector origin = "0 0 0";
		if (!GetGame() || !GetGame().GetWorld())
			return origin;

		IEntity entity = GetGame().GetWorld().FindEntityByName(targetId);
		if (!entity)
			return origin;

		return entity.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_HospitalTillState> Tills()
	{
		if (!s_mTills)
			s_mTills = new map<string, ref LF_HospitalTillState>();

		return s_mTills;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_HospitalCabinetState> Cabinets()
	{
		if (!s_mCabinets)
			s_mCabinets = new map<string, ref LF_HospitalCabinetState>();

		return s_mCabinets;
	}
}
