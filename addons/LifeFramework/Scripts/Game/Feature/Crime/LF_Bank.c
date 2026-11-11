//------------------------------------------------------------------------------------------------
//! Bank vault robbery. A configured vault calls LF_Breach.Arm, then puts MoneyStack
//! cash into bags. The haul is physical cash. LF_BankAccount is never deposited
//! and never withdrawn here; the ATM stays the only cash-to-account boundary.
//!
//! GATES. Civilian-only, minimum police on duty, and cooldown use the same
//! comparisons as LF_RobAction. The server config chooses the bag count and the
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
	string m_sTools;
	float m_fJamChance;
	bool m_bSpawnPending;
	//! Heist (#274, #275): what the open target leaves lying. Empty keeps the bank pattern.
	string m_sLootKind;
	string m_sOffence;
	string m_sLoot;
	//! Kits left in stock (armory, #275). Negative means unlimited.
	int m_iStock = -1;
	int m_iStockMax;
	float m_fRefillSeconds;
	float m_fRefillAdvanced;
	float m_fRefillSampledAt;
	string m_sLooterUid;
	vector m_vLooterAt;
	ref array<EntityID> m_aLooseBags = {};
	bool m_bCooldownRunning;
	float m_fAdvancedSeconds;
	float m_fSampledAt;
	ref array<ref LF_BankBagState> m_aBags;
}

//------------------------------------------------------------------------------------------------
class LF_Bank
{
	static const ResourceName BAG_PREFAB = "{65DE48DCC1D4DD01}Prefabs/Items/Roleplay/LF_MoneyBag.et";
	static const ResourceName DOOR_PREFAB = "{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et";
	static const ResourceName SHELL_PREFAB = "{D1859197E1AB4E8A}Prefabs/Structures/Services/EL_Bank.et";
	static const ResourceName CRATE_MESH = "{55991E1823C60AB3}Assets/Props/Crates/Crate_01.xob";
	static const ResourceName DRILL_PREFAB = "{4FF102B4CE9B0ED3}Prefabs/Items/Fence/LF_Drill.et";
	//! How far to the robber's right a left drill rests (metres): the bench drill's base is about 0.8 m long.
	static const float TOOL_REST_SIDE = 1.0;
	static const string TOOL_DRILL = "drill";
	static const string TOOL_CHARGE = "charge";
	static const ResourceName KEYCARD_PREFAB = "{F341AA386FA695E4}Prefabs/Items/Fence/LF_Keycard.et";
	static const string TOOL_KEYCARD = "keycard";

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
	//! A cooldown of 0 does not block. Matches LF_RobAction's elapsed < cooldown check.
	static bool CooldownBlocks(float cooldownSeconds, float elapsedSeconds)
	{
		if (cooldownSeconds <= 0)
			return false;

		return elapsedSeconds < cooldownSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! LF_RobAction allows the action when the account is missing, and refuses
	//! every faction other than civilian.
	static bool CivilianAllowed(LF_PlayerAccount account)
	{
		if (!account)
			return true;

		return account.GetFaction() == LF_Faction.CIVILIAN;
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
			LF_Debug.Error("Bank", "configure refused: empty vault id");
			return;
		}

		if (bagCount <= 0 || cashPerBag <= 0)
			LF_Debug.Error("Bank", string.Format("configure degraded: vault %1 bag count %2 cash %3", vaultId, bagCount, cashPerBag));

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
		LF_Debug.Log("Bank", string.Format("configured %1 bags=%2 cash=%3 cooldown=%4 police=%5", vaultId, bagCount, cashPerBag, cooldownSeconds, minPolice));
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the fence tools this vault needs (#273): a comma list of "drill" and "charge", any one
	//! opens it. Empty means no tool is required. jamChance is the drill's chance (0..1) to jam.
	static void ConfigureTools(string vaultId, string tools, float jamChance)
	{
		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
		{
			LF_Debug.Error("Bank", string.Format("tools refused: vault %1 is not configured", vaultId));
			return;
		}

		vault.m_sTools = tools;
		vault.m_fJamChance = jamChance;
		LF_Debug.Log("Bank", string.Format("tools %1 tools=[%2] jam=%3", vaultId, tools, jamChance));
	}

	//------------------------------------------------------------------------------------------------
	//! Makes the vault a heist target (#274 cash van, #275 police armory). kind is "bags" (money bags
	//! spill behind it) or "weapons" (the comma list of prefabs, serial-stamped, lies in front).
	//! offence is the dispatch key, loud forces the loud alarm. stock is how many hauls the target
	//! holds (negative: unlimited); one is restored each refillSeconds.
	static void ConfigureHeist(string vaultId, string kind, string loot, string offence, bool loud, int stock, float refillSeconds)
	{
		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
		{
			LF_Debug.Error("Bank", string.Format("heist refused: vault %1 is not configured", vaultId));
			return;
		}

		vault.m_sLootKind = kind;
		vault.m_sLoot = loot;
		vault.m_sOffence = offence;
		vault.m_iStock = stock;
		vault.m_iStockMax = stock;
		vault.m_fRefillSeconds = refillSeconds;
		vault.m_fRefillAdvanced = 0;
		vault.m_fRefillSampledAt = NowSeconds();
		LF_Breach.SetAlarm(vaultId, offence, loud);
		LF_Debug.Log("Bank", string.Format("heist %1 kind=%2 stock=%3 refill=%4 loud=%5", vaultId, kind, stock, refillSeconds, loud));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stocks the target afresh and locks it (the van reached its next stop). Loose bags the
	//! last robbery left lying are removed; carried ones are in a pocket and stay.
	static void Restock(string vaultId)
	{
		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
			return;

		for (int i = 0; i < vault.m_aLooseBags.Count(); i++)
		{
			IEntity bag = GetGame().GetWorld().FindEntityByID(vault.m_aLooseBags[i]);
			if (bag && !bag.GetParent())
				SCR_EntityHelper.DeleteEntityAndChildren(bag);
		}

		vault.m_aLooseBags.Clear();
		vault.m_aBags.Clear();
		vault.m_bCooldownRunning = false;
		vault.m_bSpawnPending = false;
		LF_Breach.Reset(vaultId);
		LF_Debug.Info("Bank", string.Format("restocked %1", vaultId));
	}

	//------------------------------------------------------------------------------------------------
	//! Hauls left in stock; negative when the target is not limited.
	static int GetStock(string vaultId)
	{
		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
			return -1;

		RefillStock(vault, 0);
		return vault.m_iStock;
	}

	//------------------------------------------------------------------------------------------------
	//! Money bags this vault left lying on the ground that nobody has picked up.
	static int GetLooseBagCount(string vaultId)
	{
		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
			return 0;

		int count = 0;
		for (int i = 0; i < vault.m_aLooseBags.Count(); i++)
		{
			IEntity bag = GetGame().GetWorld().FindEntityByID(vault.m_aLooseBags[i]);
			if (bag && !bag.GetParent())
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! The loose bag at index, for tests; null when it is gone.
	static IEntity GetLooseBag(string vaultId, int index)
	{
		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault || index < 0 || index >= vault.m_aLooseBags.Count())
			return null;

		return GetGame().GetWorld().FindEntityByID(vault.m_aLooseBags[index]);
	}

	//------------------------------------------------------------------------------------------------
	//! Restores kits to stock, one per refill interval. extraSeconds is test time.
	protected static void RefillStock(LF_BankVaultState vault, float extraSeconds)
	{
		if (vault.m_iStock < 0 || vault.m_fRefillSeconds <= 0)
			return;

		float now = NowSeconds();
		float elapsed = vault.m_fRefillAdvanced + extraSeconds;
		if (now >= vault.m_fRefillSampledAt)
			elapsed = elapsed + (now - vault.m_fRefillSampledAt);

		int steps = (int)(elapsed / vault.m_fRefillSeconds);
		vault.m_fRefillSampledAt = now;
		vault.m_fRefillAdvanced = elapsed - steps * vault.m_fRefillSeconds;
		if (steps <= 0 || vault.m_iStock >= vault.m_iStockMax)
			return;

		vault.m_iStock = Math.Min(vault.m_iStockMax, vault.m_iStock + steps);
		LF_Debug.Info("Bank", string.Format("%1 refilled to %2 of %3", vault.m_sVaultId, vault.m_iStock, vault.m_iStockMax));
	}

	//------------------------------------------------------------------------------------------------
	//! Money bag fill shared with the heist spill.
	static bool FillBag(IEntity bag, int cash)
	{
		return WriteWorldBagCash(bag, cash);
	}

	//------------------------------------------------------------------------------------------------
	//! A roll in [0,1) jams the drill when it falls under the chance.
	static bool Jams(float chance, float roll)
	{
		return roll < chance;
	}

	//------------------------------------------------------------------------------------------------
	//! The tool the robber would use on this vault: "drill", "charge", or "" for none. A drill wins
	//! over a charge so the charge is kept. toolItem is the drill or fence charge found, if any.
	static string PickTool(LF_BankVaultState vault, IEntity robber, out IEntity toolItem)
	{
		toolItem = null;
		if (!vault || !robber)
			return "";

		array<string> tools = {};
		vault.m_sTools.Split(",", tools, true);
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		if (!inventory)
			return "";

		if (tools.Find(TOOL_DRILL) != -1)
		{
			array<IEntity> drills = LF_InventoryUtils.FindItemsByPrefab(inventory, DRILL_PREFAB);
			if (!drills.IsEmpty())
			{
				toolItem = drills[0];
				return TOOL_DRILL;
			}
		}

		if (tools.Find(TOOL_KEYCARD) != -1)
		{
			array<IEntity> cards = LF_InventoryUtils.FindItemsByPrefab(inventory, KEYCARD_PREFAB);
			if (!cards.IsEmpty())
			{
				toolItem = cards[0];
				return TOOL_KEYCARD;
			}
		}

		if (tools.Find(TOOL_CHARGE) != -1 && LF_Breach.HasCharge(robber))
		{
			array<IEntity> charges = LF_InventoryUtils.FindItemsByPrefab(inventory, LF_Breach.FENCE_CHARGE_PREFAB);
			if (!charges.IsEmpty())
				toolItem = charges[0];

			return TOOL_CHARGE;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Offer check: true when the robber holds a tool this vault accepts (or none is required).
	static bool HasTool(string vaultId, IEntity robber)
	{
		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault || vault.m_sTools.IsEmpty())
			return true;

		IEntity item;
		return !PickTool(vault, robber, item).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	protected static string SerialOf(IEntity item)
	{
		if (!item)
			return "";

		LF_SerialComponent serial = LF_SerialComponent.Cast(item.FindComponent(LF_SerialComponent));
		if (!serial)
			return "";

		return serial.GetSerial();
	}

	//------------------------------------------------------------------------------------------------
	protected static vector VaultOrigin(string vaultId)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return vector.Zero;

		IEntity entity = GetGame().GetWorld().FindEntityByName(vaultId);
		if (!entity)
			return vector.Zero;

		vector transform[4];
		entity.GetWorldTransform(transform);
		return transform[3];
	}

	//------------------------------------------------------------------------------------------------
	//! Where a left tool rests: TOOL_REST_SIDE m to the robber's right, level with their feet. It used to be the
	//! robber's own origin, so the bench drill spawned under them and they stood inside it while they worked.
	static vector ToolRestSpot(vector robberOrigin, vector robberRight)
	{
		vector side = Vector(robberRight[0], 0, robberRight[2]);
		if (side.LengthSq() < 0.0001)
			side = "1 0 0";
		return robberOrigin + side.Normalized() * TOOL_REST_SIDE;
	}

	//------------------------------------------------------------------------------------------------
	//! The drill stays at the vault. It leaves the robber's inventory, lies by the door, and the
	//! scene evidence hook marks it for the officer to bag; its serial still names the buyer.
	protected static void LeaveTool(string vaultId, IEntity robber, IEntity tool)
	{
		if (!tool || !robber)
			return;

		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(tool.FindComponent(InventoryItemComponent));
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		if (itemComponent && itemComponent.GetParentSlot() && inventory)
		{
			if (!inventory.TryRemoveItemFromStorage(tool, itemComponent.GetParentSlot().GetStorage()))
			{
				LF_Debug.Error("Bank", string.Format("tool not left at %1: it would not leave the inventory", vaultId));
				return;
			}
		}

		vector robberTransform[4];
		robber.GetWorldTransform(robberTransform);
		vector origin = ToolRestSpot(robber.GetOrigin(), robberTransform[0]);

		// On the surface there (the verge, the plinth by the vault), not 10 cm up to drop and tip over; upright.
		vector rest = LF_Dispatch.RestingSpot(origin + "0 0.3 0", tool);
		IEntity left = SetDown(tool, rest, robber.GetAngles()[1]);
		LF_Evidence.OnDropped(left, System.GetTickCount());
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a tool that just left an inventory down at a spot, upright, at its prefab's size. An item out of an
	//! inventory is back at scale 1 (the inventory keeps no root scale), so the 0.4 drill stood there as a 1.2 m
	//! bench drill: a scaled tool is set down as a fresh copy carrying the same serial instead.
	//! \return The tool lying at the spot (the copy, or the same entity for an unscaled prefab).
	static IEntity SetDown(notnull IEntity tool, vector at, float yaw)
	{
		ResourceName prefab = LF_Utils.GetPrefabName(tool);
		float scale = LF_Utils.PrefabScale(Resource.Load(prefab));
		if (Math.AbsFloat(scale - 1) < 0.001)
		{
			tool.SetOrigin(at);
			tool.SetAngles(Vector(0, yaw, 0));
			return tool;
		}

		IEntity copy = LF_Utils.SpawnEntityPrefab(prefab, at, Vector(yaw, 0, 0));
		if (!copy)
		{
			LF_Debug.Error("Bank", string.Format("tool copy did not spawn (%1); the tool lies at scale 1", prefab));
			tool.SetOrigin(at);
			tool.SetAngles(Vector(0, yaw, 0));
			return tool;
		}

		LF_SerialComponent from = LF_SerialComponent.Cast(tool.FindComponent(LF_SerialComponent));
		LF_SerialComponent to = LF_SerialComponent.Cast(copy.FindComponent(LF_SerialComponent));
		if (from && to && !from.GetSerial().IsEmpty() && !to.SetSerial(from.GetSerial()))
			LF_Debug.Error("Bank", string.Format("left tool lost its serial %1", from.GetSerial()));

		SCR_EntityHelper.DeleteEntityAndChildren(tool);
		return copy;
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
			LF_Debug.Error("Bank", "carry refused: empty player uid");
			return false;
		}

		LF_BankBagState bag = BagAt(vaultId, index);
		if (!bag)
		{
			LF_Debug.Error("Bank", string.Format("carry refused: vault %1 has no bag %2", vaultId, index));
			return false;
		}

		bag.m_bOnGround = false;
		bag.m_sCarrierUid = playerUid;
		LF_Debug.Log("Bank", string.Format("bag %1 carried by %2 cash=%3", bag.m_sBagId, playerUid, bag.m_iCash));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a carried bag back on the ground. The cash stays in the world.
	static bool DropBag(string vaultId, int index)
	{
		LF_BankBagState bag = BagAt(vaultId, index);
		if (!bag)
		{
			LF_Debug.Error("Bank", string.Format("drop refused: vault %1 has no bag %2", vaultId, index));
			return false;
		}

		bag.m_bOnGround = true;
		bag.m_sCarrierUid = "";
		LF_Debug.Log("Bank", string.Format("bag %1 dropped cash=%2", bag.m_sBagId, bag.m_iCash));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Arms the vault for playerUid when the robbery gates pass, then spawns the
	//! configured bags once the lock is open. Returns false when a gate refuses,
	//! the breach does not arm, or the vault is not configured.
	static bool Rob(string vaultId, string playerUid, IEntity robber = null, float jamRoll = -1)
	{
		if (vaultId.IsEmpty() || playerUid.IsEmpty())
		{
			LF_Debug.Error("Bank", "rob refused: vault id or player uid is empty");
			return false;
		}

		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
		{
			LF_Debug.Error("Bank", string.Format("rob refused: vault %1 is not configured", vaultId));
			return false;
		}

		if (!GatesPass(vault, playerUid))
			return false;

		vault.m_sLooterUid = playerUid;
		if (robber)
			vault.m_vLooterAt = robber.GetOrigin();

		// A player-driven rob needs one of the vault's fence tools. A script-driven rob (no robber
		// entity: tests, events) skips the check, as Arm skips the charge.
		string tool = "";
		IEntity toolItem = null;
		if (robber && !vault.m_sTools.IsEmpty())
		{
			tool = PickTool(vault, robber, toolItem);
			if (tool.IsEmpty())
			{
				LF_Debug.Warn("Bank", string.Format("rob refused: %1 carries none of the tools [%2] for %3", playerUid, vault.m_sTools, vaultId));
				LF_EventBus.Publish(LF_OnVaultBreach.Create(playerUid, vaultId, "", "", "refused"));
				return false;
			}
		}

		string serial = SerialOf(toolItem);
		IEntity charger = robber;
		if (tool == TOOL_KEYCARD)
			charger = null;

		if (tool == TOOL_DRILL)
		{
			charger = null;
			float roll = jamRoll;
			if (roll < 0)
				roll = Math.RandomFloat01();

			if (Jams(vault.m_fJamChance, roll))
			{
				// The bit snaps: the noise is reported and the drill stays in the wall.
				string jamOffence = vault.m_sOffence;
				if (jamOffence.IsEmpty())
					jamOffence = LF_WantedReports.OFFENCE_BREAK_IN;

				LF_Dispatch.Report(playerUid, robber, jamOffence, VaultOrigin(vaultId), -1, LF_Breach.IsLoud(vaultId));
				LeaveTool(vaultId, robber, toolItem);
				LF_Debug.Info("Bank", string.Format("drill jammed on %1 (serial %2)", vaultId, serial));
				LF_EventBus.Publish(LF_OnVaultBreach.Create(playerUid, vaultId, tool, serial, "jammed"));
				return false;
			}
		}

		if (!LF_Breach.Arm(vaultId, playerUid, charger, robber))
		{
			LF_Debug.Log("Bank", string.Format("rob refused: breach did not arm %1", vaultId));
			return false;
		}

		if (tool == TOOL_DRILL)
		{
			LeaveTool(vaultId, robber, toolItem);
			// The drill shows (#513): the robber works the lock while the drill bites.
			LF_WorkAnim.Begin(robber, VaultOrigin(vaultId), LF_EWorkAnimKind.DRILL, LF_WorkAnim.DRILL_SECONDS, true);
		}

		if (!tool.IsEmpty())
		{
			LF_Debug.Info("Bank", string.Format("%1 breached %2 with a %3 (serial %4)", playerUid, vaultId, tool, serial));
			LF_EventBus.Publish(LF_OnVaultBreach.Create(playerUid, vaultId, tool, serial, "opened"));
		}


		if (LF_Breach.IsLocked(vaultId))
		{
			vault.m_bSpawnPending = true;
			LF_Debug.Log("Bank", string.Format("breach armed; bags wait for the fuse on %1", vaultId));
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

		RefillStock(vault, elapsedSeconds);

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
			LF_Debug.Error("Bank", string.Format("export refused: vault %1 is not configured", vaultId));
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
			LF_Debug.Error("Bank", "apply refused: record is null");
			return false;
		}

		LF_BankVaultState vault = Vaults().Get(record.m_sVaultId);
		if (!vault || !record.IsValidFor(vault.m_sVaultId))
		{
			LF_Debug.Error("Bank", "apply refused: record does not match a configured vault");
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

		LF_Debug.Log("Bank", string.Format("apply-save %1 bags=%2", vault.m_sVaultId, vault.m_aBags.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Offer check. Does not arm, does not spawn, and does not raise wanted.
	//! Empty when the player may rob. Otherwise a localization key.
	static string GateReason(string vaultId, string playerUid, IEntity robber = null)
	{
		if (vaultId.IsEmpty() || playerUid.IsEmpty())
			return "#EL-Bank_NotReady";

		LF_BankVaultState vault = Vaults().Get(vaultId);
		if (!vault)
			return "#EL-Bank_NotReady";

		LF_PlayerAccount account = null;
		LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
		if (accountManager)
			account = accountManager.GetAccount(playerUid);

		if (!CivilianAllowed(account))
			return "#EL-Bank_CiviliansOnly";

		if (!PoliceOnDutyAllows(GetPoliceOnDutyCount(), vault.m_iMinPolice))
			return "#EL-Bank_NeedPolice";

		if (vault.m_bCooldownRunning && CooldownBlocks(vault.m_fCooldownSeconds, ElapsedSeconds(vault)))
			return "#EL-Bank_Cooldown";

		RefillStock(vault, 0);
		if (vault.m_iStock == 0)
			return "#LF-Heist_Empty";

		if (!LF_Breach.CanArm(vaultId))
			return "#EL-Breach_AlreadyOpen";

		if (robber && !HasTool(vaultId, robber))
			return "#LF-Bank_NeedTool";

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

		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(bag);
		if (manager)
		{
			int stored = LF_InventoryUtils.GetAmount(manager, LF_MoneyUtils.PREFAB_CASH);
			if (stored > 0)
				return stored;
		}

		IEntity cash = FindCashChild(bag);
		if (!cash)
			return 0;

		LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(cash);
		if (!quantity)
			return 0;

		return quantity.GetQuantity();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool GatesPass(LF_BankVaultState vault, string playerUid)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(playerUid);
		if (!CivilianAllowed(account))
		{
			LF_Debug.Log("Bank", string.Format("rob refused: %1 is not a civilian", playerUid));
			return false;
		}

		int police = CountPoliceOnDuty();
		if (!PoliceOnDutyAllows(police, vault.m_iMinPolice))
		{
			LF_Debug.Log("Bank", string.Format("rob refused: %1 police on duty, need %2", police, vault.m_iMinPolice));
			return false;
		}

		if (vault.m_bCooldownRunning && CooldownBlocks(vault.m_fCooldownSeconds, ElapsedSeconds(vault)))
		{
			LF_Debug.Log("Bank", string.Format("rob refused: vault %1 is on cooldown", vault.m_sVaultId));
			return false;
		}

		RefillStock(vault, 0);
		if (vault.m_iStock == 0)
		{
			LF_Debug.Log("Bank", string.Format("rob refused: %1 is out of stock", vault.m_sVaultId));
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
			LF_Debug.Error("Bank", string.Format("spawn refused: vault %1 has no bag config", vault.m_sVaultId));
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

		PayHeist(vault, count);

		vault.m_bCooldownRunning = true;
		vault.m_fAdvancedSeconds = 0;
		vault.m_fSampledAt = NowSeconds();
		LF_Debug.Log("Bank", string.Format("spawned %1 bags of %2 at %3", count, vault.m_iCashPerBag, vault.m_sVaultId));
	}

	//------------------------------------------------------------------------------------------------
	//! The heist haul (#274, #275): physical bags or serial-stamped weapons at the target, one stock
	//! kit used, one LF_OnHeistLoot on the bus.
	protected static void PayHeist(LF_BankVaultState vault, int bagCount)
	{
		if (vault.m_sLootKind.IsEmpty())
			return;

		IEntity target = GetGame().GetWorld().FindEntityByName(vault.m_sVaultId);
		int amount = 0;
		string serials = "";
		if (vault.m_sLootKind == LF_BankLoot.KIND_BAGS)
		{
			array<IEntity> bags = LF_BankLoot.SpillBags(target, vault.m_vLooterAt, bagCount, vault.m_iCashPerBag);
			foreach (IEntity bag : bags)
			{
				vault.m_aLooseBags.Insert(bag.GetID());
				amount = amount + vault.m_iCashPerBag;
			}
		}
		else if (vault.m_sLootKind == LF_BankLoot.KIND_WEAPONS)
		{
			array<IEntity> weapons = LF_BankLoot.IssueWeapons(target, vault.m_vLooterAt, vault.m_sLoot, vault.m_sLooterUid, serials);
			foreach (IEntity weapon : weapons)
				vault.m_aLooseBags.Insert(weapon.GetID());

			amount = weapons.Count();
		}

		if (vault.m_iStock > 0)
			vault.m_iStock = vault.m_iStock - 1;

		LF_Debug.Info("Bank", string.Format("heist %1 paid %2 %3 to %4 serials=[%5] stock=%6", vault.m_sVaultId, amount, vault.m_sLootKind, vault.m_sLooterUid, serials, vault.m_iStock));
		LF_EventBus.Publish(LF_OnHeistLoot.Create(vault.m_sLooterUid, vault.m_sVaultId, vault.m_sLootKind, amount, serials));
	}

	//------------------------------------------------------------------------------------------------
	protected static void FillNamedBag(string bagEntity, int cash)
	{
		if (!GetGame() || !GetGame().GetWorld())
			return;

		IEntity bag = GetGame().GetWorld().FindEntityByName(bagEntity);
		if (!bag)
		{
			LF_Debug.Error("Bank", string.Format("money bag fill refused: %1 is not in the world", bagEntity));
			return;
		}

		if (!WriteWorldBagCash(bag, cash))
			LF_Debug.Error("Bank", string.Format("money bag fill failed for %1", bagEntity));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool WriteWorldBagCash(IEntity bag, int cash)
	{
		if (!bag || cash <= 0)
		{
			LF_Debug.Error("Bank", "money bag fill refused: bag missing or cash is empty");
			return false;
		}

		Resource money = Resource.Load(LF_MoneyUtils.PREFAB_CASH);
		if (!money || !money.IsValid())
		{
			LF_Debug.Error("Bank", "money bag fill refused: MoneyStack prefab missing");
			return false;
		}

		int paid = LF_MoneyUtils.AddCash(bag, cash);
		if (paid == cash)
			return true;

		if (paid > 0)
			LF_MoneyUtils.RemoveCash(bag, paid);

		IEntity cashEntity = FindCashChild(bag);
		if (!cashEntity && GetGame() && GetGame().GetWorld())
			cashEntity = GetGame().GetWorld().FindEntityByName(bag.GetName() + "_Cash");

		if (!cashEntity)
		{
			cashEntity = LF_Utils.SpawnEntityPrefab(LF_MoneyUtils.PREFAB_CASH, bag.GetOrigin());
			if (!cashEntity)
			{
				LF_Debug.Error("Bank", "money bag fill refused: MoneyStack did not spawn");
				return false;
			}

			bag.AddChild(cashEntity, -1, EAddChildFlags.AUTO_TRANSFORM);
		}

		if (cashEntity.GetParent() != bag)
			bag.AddChild(cashEntity, -1, EAddChildFlags.AUTO_TRANSFORM);

		LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(cashEntity);
		if (!quantity)
		{
			LF_Debug.Error("Bank", "money bag fill refused: stack has no quantity");
			return false;
		}

		if (!quantity.SetQuantity(cash))
		{
			LF_Debug.Error("Bank", string.Format("money bag fill refused: quantity did not become %1", cash));
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
			LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(child);
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
			PlayerController playerController = playerManager.GetPlayerController(playerId);
			if (!playerController)
				continue;

			IEntity controlled = playerController.GetControlledEntity();
			if (!controlled)
				continue;

			string playerUid = LF_Utils.GetPlayerUid(controlled);
			LF_PlayerAccount account = accountManager.GetAccount(playerUid);
			if (account && account.GetFaction() == LF_Faction.POLICE && account.IsOnDuty())
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
