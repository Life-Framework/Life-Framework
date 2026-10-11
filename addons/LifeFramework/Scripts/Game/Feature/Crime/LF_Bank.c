//------------------------------------------------------------------------------------------------
//! Bank vault robbery. A configured vault calls LF_Breach.Arm, then puts MoneyStack
//! cash into bags. The haul is physical cash. EL_BankAccount is never deposited
//! and never withdrawn here; the ATM stays the only cash-to-account boundary.
//!
//! GATES. Civilian-only, minimum police on duty, and cooldown use the same
//! comparisons as EL_RobAction. The server config chooses the bag count and the
//! cash in each bag. There is no client request for a larger haul.
//!
//! ON OPEN. Fuse 0 spawns the bags before Rob returns. A positive fuse waits
//! until Advance opens the lock. A second Rob inside the cooldown spawns nothing.
//! When the cooldown has elapsed and the lock can be armed again, the vault refills.
//!
//! BAGS. A spawned bag starts on the ground. CarryBag marks it in hand. DropBag
//! puts it back. The cash stays on the bag either way. Export / Apply restore
//! that ground loot and do not write a bank balance.
//------------------------------------------------------------------------------------------------
class LF_BankBagRecord : Managed
{
	string m_sBagId;
	int m_iCash;
	bool m_bOnGround;
}

//------------------------------------------------------------------------------------------------
class LF_BankRecord : Managed
{
	static const int VERSION = 1;

	string m_sVaultId;
	ref array<ref LF_BankBagRecord> m_aBags;

	//------------------------------------------------------------------------------------------------
	static LF_BankRecord Create(string vaultId)
	{
		LF_BankRecord record();
		record.m_sVaultId = vaultId;
		record.m_aBags = new array<ref LF_BankBagRecord>();
		return record;
	}

	//------------------------------------------------------------------------------------------------
	bool IsValidFor(string vaultId)
	{
		if (m_sVaultId.IsEmpty() || vaultId.IsEmpty())
			return false;

		return m_sVaultId == vaultId;
	}
}

//------------------------------------------------------------------------------------------------
class LF_BankBagState : Managed
{
	string m_sBagId;
	int m_iCash;
	bool m_bOnGround;
	string m_sCarrierUid;
}

//------------------------------------------------------------------------------------------------
class LF_BankVaultState : Managed
{
	string m_sVaultId;
	string m_sBagEntity;
	int m_iBagCount;
	int m_iCashPerBag;
	float m_fCooldownSeconds;
	int m_iMinPolice;
	bool m_bSpawnPending;
	bool m_bCooldownRunning;
	float m_fAdvancedSeconds;
	float m_fSampledAt;
	ref array<ref LF_BankBagState> m_aBags;
}

//------------------------------------------------------------------------------------------------
class LF_Bank
{
	static const ResourceName BAG_PREFAB = "{6BF776E22ECF67E7}Prefabs/Items/Core/HandCarryItem_Base.et";
	static const ResourceName DOOR_PREFAB = "{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et";
	static const ResourceName SHELL_PREFAB = "{D1859197E1AB4E8A}Prefabs/Structures/Services/EL_Bank.et";
	static const ResourceName CRATE_MESH = "{55991E1823C60AB3}Assets/Props/Crates/Crate_01.xob";

	protected static ref map<string, ref LF_BankVaultState> s_mVaults;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_BankVaultState> Vaults()
	{
		if (!s_mVaults)
			s_mVaults = new map<string, ref LF_BankVaultState>();

		return s_mVaults;
	}

	//------------------------------------------------------------------------------------------------
	//! A request above the configured bag count is cut down to that count.
	//! A negative request or a negative config contributes nothing.
	static int CappedBagCount(int configured, int requested)
	{
		if (configured < 0)
			configured = 0;

		if (requested < 0)
			requested = 0;

		if (requested > configured)
			return configured;

		return requested;
	}

	//------------------------------------------------------------------------------------------------
	//! True while elapsedSeconds is still short of cooldownSeconds.
	//! A cooldown of 0 does not block. Matches EL_RobAction's elapsed < cooldown check.
	static bool CooldownBlocks(float cooldownSeconds, float elapsedSeconds)
	{
		if (cooldownSeconds <= 0)
			return false;

		return elapsedSeconds < cooldownSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! EL_RobAction allows the action when the account is missing, and refuses
	//! every faction other than civilian.
	static bool CivilianAllowed(EL_PlayerAccount account)
	{
		if (!account)
			return true;

		return account.GetFaction() == EL_Faction.CIVILIAN;
	}

	//------------------------------------------------------------------------------------------------
	//! True when policeOnDuty meets minPolice. A minimum of 0 is open with nobody on duty.
	static bool PoliceOnDutyAllows(int policeOnDuty, int minPolice)
	{
		return policeOnDuty >= minPolice;
	}

	//------------------------------------------------------------------------------------------------
	//! Registers a vault and its breach target. bagEntity is the world entity
	//! filled with MoneyStack cash; empty means the bags stay in this record only.
	static void Configure(string vaultId, int bagCount, int cashPerBag, float cooldownSeconds, int minPolice, float fuseSeconds, string bagEntity)
	{
		if (vaultId.IsEmpty())
		{
			EL_Debug.Error("Bank", "configure refused: empty vault id");
			return;
		}

		if (bagCount <= 0 || cashPerBag <= 0)
			EL_Debug.Error("Bank", string.Format("configure degraded: vault %1 bag count %2 cash %3", vaultId, bagCount, cashPerBag));

		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
		{
			vault = new LF_BankVaultState();
			vault.m_sVaultId = vaultId;
			vault.m_aBags = new array<ref LF_BankBagState>();
			Vaults().Set(vaultId, vault);
		}

		vault.m_iBagCount = bagCount;
		vault.m_iCashPerBag = cashPerBag;
		vault.m_fCooldownSeconds = cooldownSeconds;
		vault.m_iMinPolice = minPolice;
		vault.m_sBagEntity = bagEntity;
		LF_Breach.Configure(vaultId, fuseSeconds, "");
		EL_Debug.Log("Bank", string.Format("configured %1 bags=%2 cash=%3 cooldown=%4 police=%5", vaultId, bagCount, cashPerBag, cooldownSeconds, minPolice));
	}

	//------------------------------------------------------------------------------------------------
	static int GetConfiguredBagCount(string vaultId)
	{
		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
			return 0;

		return vault.m_iBagCount;
	}

	//------------------------------------------------------------------------------------------------
	static int GetCashPerBag(string vaultId)
	{
		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
			return 0;

		return vault.m_iCashPerBag;
	}

	//------------------------------------------------------------------------------------------------
	static int GetSpawnedBagCount(string vaultId)
	{
		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault || !vault.m_aBags)
			return 0;

		return vault.m_aBags.Count();
	}

	//------------------------------------------------------------------------------------------------
	static int GetBagCash(string vaultId, int index)
	{
		LF_BankBagState bag = BagAt(vaultId, index);
		if (!bag)
			return 0;

		return bag.m_iCash;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsBagOnGround(string vaultId, int index)
	{
		LF_BankBagState bag = BagAt(vaultId, index);
		if (!bag)
			return false;

		return bag.m_bOnGround;
	}

	//------------------------------------------------------------------------------------------------
	//! Marks a spawned bag as in the player's hands. The cash stays on the bag.
	static bool CarryBag(string vaultId, int index, string playerUid)
	{
		if (playerUid.IsEmpty())
		{
			EL_Debug.Error("Bank", "carry refused: empty player uid");
			return false;
		}

		LF_BankBagState bag = BagAt(vaultId, index);
		if (!bag)
		{
			EL_Debug.Error("Bank", string.Format("carry refused: vault %1 has no bag %2", vaultId, index));
			return false;
		}

		bag.m_bOnGround = false;
		bag.m_sCarrierUid = playerUid;
		EL_Debug.Log("Bank", string.Format("bag %1 carried by %2 cash=%3", bag.m_sBagId, playerUid, bag.m_iCash));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a carried bag back on the ground. The cash stays in the world.
	static bool DropBag(string vaultId, int index)
	{
		LF_BankBagState bag = BagAt(vaultId, index);
		if (!bag)
		{
			EL_Debug.Error("Bank", string.Format("drop refused: vault %1 has no bag %2", vaultId, index));
			return false;
		}

		bag.m_bOnGround = true;
		bag.m_sCarrierUid = "";
		EL_Debug.Log("Bank", string.Format("bag %1 dropped cash=%2", bag.m_sBagId, bag.m_iCash));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Arms the vault for playerUid when the robbery gates pass, then spawns the
	//! configured bags once the lock is open. Returns false when a gate refuses,
	//! the breach does not arm, or the vault is not configured.
	static bool Rob(string vaultId, string playerUid)
	{
		if (vaultId.IsEmpty() || playerUid.IsEmpty())
		{
			EL_Debug.Error("Bank", "rob refused: vault id or player uid is empty");
			return false;
		}

		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
		{
			EL_Debug.Error("Bank", string.Format("rob refused: vault %1 is not configured", vaultId));
			return false;
		}

		if (!GatesPass(vault, playerUid))
			return false;

		if (!LF_Breach.Arm(vaultId, playerUid))
		{
			EL_Debug.Log("Bank", string.Format("rob refused: breach did not arm %1", vaultId));
			return false;
		}

		if (LF_Breach.IsLocked(vaultId))
		{
			vault.m_bSpawnPending = true;
			EL_Debug.Log("Bank", string.Format("breach armed; bags wait for the fuse on %1", vaultId));
			return true;
		}

		vault.m_bSpawnPending = false;
		SpawnWave(vault);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the cooldown clock and, when a fuse is still running, the breach fuse.
	//! Spawns the pending bags once the lock opens.
	//! \return True when the vault lock is open after the step.
	static bool Advance(string vaultId, float elapsedSeconds)
	{
		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
			return false;

		if (vault.m_bCooldownRunning)
			vault.m_fAdvancedSeconds = vault.m_fAdvancedSeconds + elapsedSeconds;

		bool open = LF_Breach.Advance(vaultId, elapsedSeconds);
		if (vault.m_bSpawnPending && open)
		{
			vault.m_bSpawnPending = false;
			SpawnWave(vault);
		}

		return open;
	}

	//------------------------------------------------------------------------------------------------
	static LF_BankRecord Export(string vaultId)
	{
		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
		{
			EL_Debug.Error("Bank", string.Format("export refused: vault %1 is not configured", vaultId));
			return null;
		}

		LF_BankRecord record = LF_BankRecord.Create(vaultId);
		if (!vault.m_aBags)
			return record;

		foreach (LF_BankBagState bag : vault.m_aBags)
		{
			if (!bag)
				continue;

			LF_BankBagRecord saved();
			saved.m_sBagId = bag.m_sBagId;
			saved.m_iCash = bag.m_iCash;
			saved.m_bOnGround = bag.m_bOnGround;
			record.m_aBags.Insert(saved);
		}

		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes saved bags back onto the vault. Idempotent. Does not deposit or
	//! withdraw. A record for another id is rejected.
	static bool Apply(LF_BankRecord record)
	{
		if (!record)
		{
			EL_Debug.Error("Bank", "apply refused: record is null");
			return false;
		}

		LF_BankVaultState vault = Vaults().Get(record.m_sVaultId);
		if (!vault || !record.IsValidFor(vault.m_sVaultId))
		{
			EL_Debug.Error("Bank", "apply refused: record does not match a configured vault");
			return false;
		}

		vault.m_aBags.Clear();
		if (record.m_aBags)
		{
			foreach (LF_BankBagRecord saved : record.m_aBags)
			{
				if (!saved)
					continue;

				LF_BankBagState bag();
				bag.m_sBagId = saved.m_sBagId;
				bag.m_iCash = saved.m_iCash;
				bag.m_bOnGround = saved.m_bOnGround;
				vault.m_aBags.Insert(bag);
			}
		}

		if (!vault.m_sBagEntity.IsEmpty() && vault.m_aBags.Count() > 0)
			FillNamedBag(vault.m_sBagEntity, vault.m_aBags[0].m_iCash);

		EL_Debug.Log("Bank", string.Format("apply-save %1 bags=%2", vault.m_sVaultId, vault.m_aBags.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Offer check. Does not arm, does not spawn, and does not raise wanted.
	//! Empty when the player may rob. Otherwise a localization key.
	static string GateReason(string vaultId, string playerUid)
	{
		if (vaultId.IsEmpty() || playerUid.IsEmpty())
			return "#EL-Bank_NotReady";

		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
			return "#EL-Bank_NotReady";

		EL_PlayerAccount account = null;
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (accountManager)
			account = accountManager.GetAccount(playerUid);

		if (!CivilianAllowed(account))
			return "#EL-Bank_CiviliansOnly";

		if (!PoliceOnDutyAllows(GetPoliceOnDutyCount(), vault.m_iMinPolice))
			return "#EL-Bank_NeedPolice";

		if (vault.m_bCooldownRunning && CooldownBlocks(vault.m_fCooldownSeconds, ElapsedSeconds(vault)))
			return "#EL-Bank_Cooldown";

		if (!LF_Breach.CanArm(vaultId))
			return "#EL-Breach_AlreadyOpen";

		return "";
	}

	//------------------------------------------------------------------------------------------------
	static int GetPoliceOnDutyCount()
	{
		return CountPoliceOnDuty();
	}

	//------------------------------------------------------------------------------------------------
	//! MoneyStack quantity carried by the named bag entity. Storage cash wins
	//! when the bag has an inventory manager; otherwise the child stack counts.
	static int ReadWorldBagCash(IEntity bag)
	{
		if (!bag)
			return -1;

		InventoryStorageManagerComponent manager = EL_InventoryUtils.GetResponsibleStorageManager(bag);
		if (manager)
		{
			int stored = EL_InventoryUtils.GetAmount(manager, EL_MoneyUtils.PREFAB_CASH);
			if (stored > 0)
				return stored;
		}

		IEntity cash = FindCashChild(bag);
		if (!cash)
			return 0;

		EL_QuantityComponent quantity = EL_Component<EL_QuantityComponent>.Find(cash);
		if (!quantity)
			return 0;

		return quantity.GetQuantity();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool GatesPass(LF_BankVaultState vault, string playerUid)
	{
		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(playerUid);
		if (!CivilianAllowed(account))
		{
			EL_Debug.Log("Bank", string.Format("rob refused: %1 is not a civilian", playerUid));
			return false;
		}

		int police = CountPoliceOnDuty();
		if (!PoliceOnDutyAllows(police, vault.m_iMinPolice))
		{
			EL_Debug.Log("Bank", string.Format("rob refused: %1 police on duty, need %2", police, vault.m_iMinPolice));
			return false;
		}

		if (vault.m_bCooldownRunning && CooldownBlocks(vault.m_fCooldownSeconds, ElapsedSeconds(vault)))
		{
			EL_Debug.Log("Bank", string.Format("rob refused: vault %1 is on cooldown", vault.m_sVaultId));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SpawnWave(LF_BankVaultState vault)
	{
		int count = CappedBagCount(vault.m_iBagCount, vault.m_iBagCount);
		if (count <= 0 || vault.m_iCashPerBag <= 0)
		{
			EL_Debug.Error("Bank", string.Format("spawn refused: vault %1 has no bag config", vault.m_sVaultId));
			return;
		}

		for (int i = 0; i < count; i++)
		{
			LF_BankBagState bag();
			bag.m_sBagId = string.Format("%1:%2", vault.m_sVaultId, vault.m_aBags.Count());
			bag.m_iCash = vault.m_iCashPerBag;
			bag.m_bOnGround = true;
			vault.m_aBags.Insert(bag);
		}

		if (!vault.m_sBagEntity.IsEmpty())
			FillNamedBag(vault.m_sBagEntity, vault.m_iCashPerBag);

		vault.m_bCooldownRunning = true;
		vault.m_fAdvancedSeconds = 0;
		vault.m_fSampledAt = NowSeconds();
		EL_Debug.Log("Bank", string.Format("spawned %1 bags of %2 at %3", count, vault.m_iCashPerBag, vault.m_sVaultId));
	}

	//------------------------------------------------------------------------------------------------
	protected static void FillNamedBag(string bagEntity, int cash)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return;

		IEntity bag = GetGame().GetWorld().FindEntityByName(bagEntity);
		if (!bag)
		{
			EL_Debug.Error("Bank", string.Format("money bag fill refused: %1 is not in the world", bagEntity));
			return;
		}

		if (!WriteWorldBagCash(bag, cash))
			EL_Debug.Error("Bank", string.Format("money bag fill failed for %1", bagEntity));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool WriteWorldBagCash(IEntity bag, int cash)
	{
		if (!bag || cash <= 0)
		{
			EL_Debug.Error("Bank", "money bag fill refused: bag missing or cash is empty");
			return false;
		}

		Resource money = Resource.Load(EL_MoneyUtils.PREFAB_CASH);
		if (!money || !money.IsValid())
		{
			EL_Debug.Error("Bank", "money bag fill refused: MoneyStack prefab missing");
			return false;
		}

		int paid = EL_MoneyUtils.AddCash(bag, cash);
		if (paid == cash)
			return true;

		if (paid > 0)
			EL_MoneyUtils.RemoveCash(bag, paid);

		IEntity cashEntity = FindCashChild(bag);
		if (!cashEntity && GetGame() && GetGame().GetWorld())
			cashEntity = GetGame().GetWorld().FindEntityByName(bag.GetName() + "_Cash");

		if (!cashEntity)
		{
			cashEntity = EL_Utils.SpawnEntityPrefab(EL_MoneyUtils.PREFAB_CASH, bag.GetOrigin());
			if (!cashEntity)
			{
				EL_Debug.Error("Bank", "money bag fill refused: MoneyStack did not spawn");
				return false;
			}

			bag.AddChild(cashEntity, -1, EAddChildFlags.AUTO_TRANSFORM);
		}

		if (cashEntity.GetParent() != bag)
			bag.AddChild(cashEntity, -1, EAddChildFlags.AUTO_TRANSFORM);

		EL_QuantityComponent quantity = EL_Component<EL_QuantityComponent>.Find(cashEntity);
		if (!quantity)
		{
			EL_Debug.Error("Bank", "money bag fill refused: stack has no quantity");
			return false;
		}

		if (!quantity.SetQuantity(cash))
		{
			EL_Debug.Error("Bank", string.Format("money bag fill refused: quantity did not become %1", cash));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindCashChild(IEntity bag)
	{
		if (!bag)
			return null;

		IEntity child = bag.GetChildren();
		while (child)
		{
			EL_QuantityComponent quantity = EL_Component<EL_QuantityComponent>.Find(child);
			if (quantity)
				return child;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static float ElapsedSeconds(LF_BankVaultState vault)
	{
		float worldDelta = 0;
		float now = NowSeconds();
		if (now >= vault.m_fSampledAt)
			worldDelta = now - vault.m_fSampledAt;

		return vault.m_fAdvancedSeconds + worldDelta;
	}

	//------------------------------------------------------------------------------------------------
	protected static float NowSeconds()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime() / 1000.0;
	}

	//------------------------------------------------------------------------------------------------
	protected static int CountPoliceOnDuty()
	{
		int count = 0;
		EL_PlayerControllerManagerCompat playerManager = EL_PlayerControllerManagerCompat.GetInstance();
		if (!playerManager)
			return 0;

		array<int> playerIds = {};
		playerManager.GetAllPlayerIds(playerIds);
		EL_PlayerAccountManager accountManager = EL_PlayerAccountManager.GetInstance();
		if (!accountManager)
			return 0;

		foreach (int playerId : playerIds)
		{
			PlayerController playerController = playerManager.GetPlayerController(playerId);
			if (!playerController)
				continue;

			IEntity controlled = playerController.GetControlledEntity();
			if (!controlled)
				continue;

			string playerUid = EL_Utils.GetPlayerUid(controlled);
			EL_PlayerAccount account = accountManager.GetAccount(playerUid);
			if (account && account.GetFaction() == EL_Faction.POLICE && account.IsOnDuty())
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_BankBagState BagAt(string vaultId, int index)
	{
		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault || !vault.m_aBags)
			return null;

		if (index < 0 || index >= vault.m_aBags.Count())
			return null;

		return vault.m_aBags[index];
	}
}
