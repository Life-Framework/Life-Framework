//------------------------------------------------------------------------------------------------
//! One item in a storage snapshot. The snapshot is a flat list: m_iParent is the index of the
//! item this one sits in (-1 = the snapshot root, e.g. the character), m_iStorage is the index of
//! the storage component on that parent (FindComponents order), m_iSlot the slot id in it.
class LF_StorageItemRecord : Managed
{
	string m_sPrefab;
	int m_iQuantity = 1;
	int m_iParent = -1;
	int m_iStorage;
	int m_iSlot = -1;

	//------------------------------------------------------------------------------------------------
	static LF_StorageItemRecord Create(string prefab, int quantity, int parent, int storage, int slot)
	{
		LF_StorageItemRecord record();
		record.m_sPrefab = prefab;
		record.m_iQuantity = quantity;
		record.m_iParent = parent;
		record.m_iStorage = storage;
		record.m_iSlot = slot;
		return record;
	}
}

//------------------------------------------------------------------------------------------------
//! A named inventory tree owned by a stable key. One owner can hold several named snapshots
//! (SLOT_CARRIED today; a police duty / civilian stash pair later) without a format change.
class LF_StorageSnapshotRecord : Managed
{
	static const int VERSION = 1;

	int m_iVersion = VERSION;
	string m_sOwnerKey;
	string m_sSlot;
	ref array<ref LF_StorageItemRecord> m_aItems = {};

	//------------------------------------------------------------------------------------------------
	static LF_StorageSnapshotRecord Create(string ownerKey, string slot)
	{
		LF_StorageSnapshotRecord record();
		record.m_iVersion = VERSION;
		record.m_sOwnerKey = ownerKey;
		record.m_sSlot = slot;
		record.m_aItems = {};
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Sum of quantities of the prefab across the whole tree.
	int CountPrefab(string prefab)
	{
		int total;
		foreach (LF_StorageItemRecord item : m_aItems)
		{
			if (item && item.m_sPrefab == prefab)
				total += item.m_iQuantity;
		}

		return total;
	}
}

//------------------------------------------------------------------------------------------------
//! Persisted inventory trees keyed by (owner key, slot name). The player's carried inventory is
//! keyed by the character persistence id under SLOT_CARRIED.
//!
//! CAPTURE. Walks every storage component of the root, slot by slot, recursively, and records
//! prefab + quantity + position in the tree. Never stores IEntity or EntityID.
//!
//! APPLY. Reconciles a live root against a snapshot: an occupant that matches a record (same
//! storage, same prefab) is kept and its quantity set; an occupant no record claims is deleted;
//! a record nothing matches is spawned and inserted. Applying twice leaves the same tree
//! (idempotent), and a prefab's default contents (a jacket's default pocket item, a weapon's
//! default magazine) never come back on top of what the player actually had. A record whose
//! prefab does not load logs EL_Debug.Error and is skipped with its children.
//!
//! DEATH. "Death drops what you carry": the body keeps the items, the account does not. The
//! carried snapshot is dropped at death and a dead character is never captured, so the restart
//! cannot hand the items back next to the lootable body.
//!
//! PERSISTENCE. ExportAll / ApplyAll are the "carried" payload of EL_PersistenceComponentSerializer
//! (save version 6+). CaptureAllPlayers runs at the start of that payload write (autosave, manual
//! and shutdown save points); the disconnect path captures the leaving player first.
//------------------------------------------------------------------------------------------------
class LF_StorageSnapshot
{
	static const string SLOT_CARRIED = "carried";
	static const string FEATURE = "CarriedInventory";

	protected static ref map<string, ref LF_StorageSnapshotRecord> s_mSnapshots;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_StorageSnapshotRecord> Snapshots()
	{
		if (!s_mSnapshots)
			s_mSnapshots = new map<string, ref LF_StorageSnapshotRecord>();

		return s_mSnapshots;
	}

	//------------------------------------------------------------------------------------------------
	protected static string MakeKey(string ownerKey, string slot)
	{
		return ownerKey + "|" + slot;
	}

	//------------------------------------------------------------------------------------------------
	static bool Has(string ownerKey, string slot)
	{
		return Get(ownerKey, slot) != null;
	}

	//------------------------------------------------------------------------------------------------
	static LF_StorageSnapshotRecord Get(string ownerKey, string slot)
	{
		if (ownerKey.IsEmpty() || slot.IsEmpty())
			return null;

		return Snapshots().Get(MakeKey(ownerKey, slot));
	}

	//------------------------------------------------------------------------------------------------
	static void Set(notnull LF_StorageSnapshotRecord record)
	{
		if (record.m_sOwnerKey.IsEmpty() || record.m_sSlot.IsEmpty())
		{
			EL_Debug.Error(FEATURE, "snapshot refused: empty owner key or slot");
			return;
		}

		Snapshots().Set(MakeKey(record.m_sOwnerKey, record.m_sSlot), record);
	}

	//------------------------------------------------------------------------------------------------
	static bool Clear(string ownerKey, string slot)
	{
		if (!s_mSnapshots || ownerKey.IsEmpty())
			return false;

		string key = MakeKey(ownerKey, slot);
		if (!s_mSnapshots.Contains(key))
			return false;

		s_mSnapshots.Remove(key);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Death drops what you carry: the account loses the carried snapshot, the body keeps the items.
	static void OnCharacterDied(string characterId)
	{
		if (characterId.IsEmpty())
			return;

		if (Clear(characterId, SLOT_CARRIED))
			EL_Debug.Info(FEATURE, string.Format("carried snapshot dropped at death for character %1", characterId));
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_StorageSnapshotRecord> ExportAll()
	{
		array<ref LF_StorageSnapshotRecord> records = new array<ref LF_StorageSnapshotRecord>();
		if (!s_mSnapshots)
			return records;

		foreach (string key, LF_StorageSnapshotRecord record : s_mSnapshots)
		{
			if (record)
				records.Insert(record);
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the whole store (idempotent).
	static void ApplyAll(array<ref LF_StorageSnapshotRecord> records)
	{
		s_mSnapshots = new map<string, ref LF_StorageSnapshotRecord>();
		if (!records)
			return;

		foreach (LF_StorageSnapshotRecord record : records)
		{
			if (!record || record.m_sOwnerKey.IsEmpty() || record.m_sSlot.IsEmpty())
			{
				EL_Debug.Error(FEATURE, "load skipped a snapshot without owner key or slot");
				continue;
			}

			if (!record.m_aItems)
				record.m_aItems = {};

			Snapshots().Set(MakeKey(record.m_sOwnerKey, record.m_sSlot), record);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mSnapshots = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Whether the character entity is alive (a dead body is never captured).
	static bool IsAlive(IEntity character)
	{
		if (!character)
			return false;

		// The damage manager is destroyed the moment the character dies; the controller's life
		// state follows a frame later. Either one means dead.
		DamageManagerComponent damage = DamageManagerComponent.Cast(character.FindComponent(DamageManagerComponent));
		if (damage && damage.IsDestroyed())
			return false;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		if (controller && controller.GetLifeState() == ECharacterLifeState.DEAD)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Captures a living character's carried inventory under its character id. A dead character
	//! drops the snapshot instead.
	//! \return True when a snapshot was stored.
	static bool CaptureCarried(string characterId, IEntity character)
	{
		if (characterId.IsEmpty() || !character)
			return false;

		if (!IsAlive(character))
		{
			OnCharacterDied(characterId);
			return false;
		}

		LF_StorageSnapshotRecord record = Capture(characterId, SLOT_CARRIED, character);
		if (!record)
			return false;

		Set(record);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: refreshes the carried snapshot of every connected player with a living character.
	//! \return Number of characters captured.
	static int CaptureAllPlayers()
	{
		if (!Replication.IsServer() || !GetGame())
			return 0;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		EL_PlayerAccountManager accounts = EL_PlayerAccountManager.GetInstance();
		if (!playerManager || !accounts)
			return 0;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		int captured;
		foreach (int playerId : playerIds)
		{
			if (CapturePlayer(playerId))
				captured++;
		}

		return captured;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: captures one player's carried inventory (disconnect, save point).
	static bool CapturePlayer(int playerId)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		EL_PlayerAccountManager accounts = EL_PlayerAccountManager.GetInstance();
		if (!playerManager || !accounts)
			return false;

		IEntity controlled = playerManager.GetPlayerControlledEntity(playerId);
		if (!controlled || !SCR_ChimeraCharacter.Cast(controlled))
			return false;

		string uid = EL_Utils.GetPlayerUID(playerId);
		if (uid.IsEmpty())
			return false;

		EL_PlayerAccount account = accounts.GetAccount(uid);
		if (!account || !account.GetActiveCharacter())
			return false;

		return CaptureCarried(account.GetActiveCharacter().GetId(), controlled);
	}

	//------------------------------------------------------------------------------------------------
	//! Builds a snapshot of the root's storage tree without storing it.
	static LF_StorageSnapshotRecord Capture(string ownerKey, string slot, IEntity root)
	{
		if (!root)
		{
			EL_Debug.Error(FEATURE, string.Format("capture refused for %1/%2: no entity", ownerKey, slot));
			return null;
		}

		LF_StorageSnapshotRecord record = LF_StorageSnapshotRecord.Create(ownerKey, slot);
		array<IEntity> visited = {};
		CaptureChildren(root, -1, record.m_aItems, visited);
		return record;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CollectStorages(IEntity owner, notnull array<BaseInventoryStorageComponent> storages)
	{
		array<Managed> components = {};
		owner.FindComponents(BaseInventoryStorageComponent, components);
		foreach (Managed component : components)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(component);
			if (storage)
				storages.Insert(storage);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void CaptureChildren(IEntity owner, int parentIndex, notnull array<ref LF_StorageItemRecord> items, notnull array<IEntity> visited)
	{
		array<BaseInventoryStorageComponent> storages = {};
		CollectStorages(owner, storages);
		for (int storageIndex = 0; storageIndex < storages.Count(); storageIndex++)
		{
			BaseInventoryStorageComponent storage = storages[storageIndex];
			int slots = storage.GetSlotsCount();
			for (int slot = 0; slot < slots; slot++)
			{
				IEntity item = storage.Get(slot);
				if (!item || visited.Find(item) != -1)
					continue;

				visited.Insert(item);
				string prefab = EL_Utils.GetPrefabName(item);
				if (prefab.IsEmpty())
				{
					EL_Debug.Warn(FEATURE, string.Format("capture skipped an item without a prefab in %1", EL_Utils.GetPrefabName(owner)));
					continue;
				}

				int quantity = 1;
				EL_QuantityComponent quantityComponent = EL_QuantityComponent.Cast(item.FindComponent(EL_QuantityComponent));
				if (quantityComponent)
					quantity = quantityComponent.GetQuantity();

				items.Insert(LF_StorageItemRecord.Create(prefab, quantity, parentIndex, storageIndex, slot));
				CaptureChildren(item, items.Count() - 1, items, visited);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: makes the root's storage tree equal the snapshot. Safe to call twice.
	//! \return Number of items spawned (0 on a second apply of the same snapshot).
	static int Apply(IEntity root, LF_StorageSnapshotRecord record)
	{
		if (!root || !record || !record.m_aItems)
		{
			EL_Debug.Error(FEATURE, "apply refused: no entity or no snapshot");
			return 0;
		}

		InventoryStorageManagerComponent manager = EL_InventoryUtils.GetResponsibleStorageManager(root);
		if (!manager)
		{
			EL_Debug.Error(FEATURE, string.Format("apply refused for %1/%2: %3 has no inventory storage manager", record.m_sOwnerKey, record.m_sSlot, EL_Utils.GetPrefabName(root)));
			return 0;
		}

		array<IEntity> resolved = {};
		resolved.Resize(record.m_aItems.Count());
		int spawned = ApplyChildren(manager, root, -1, record.m_aItems, resolved);
		EL_Debug.Info(FEATURE, string.Format("applied %1/%2: %3 record(s), %4 spawned", record.m_sOwnerKey, record.m_sSlot, record.m_aItems.Count(), spawned));
		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	protected static int ApplyChildren(InventoryStorageManagerComponent manager, IEntity owner, int parentIndex, array<ref LF_StorageItemRecord> items, array<IEntity> resolved)
	{
		array<BaseInventoryStorageComponent> storages = {};
		CollectStorages(owner, storages);

		// Current occupants of the owner's storages.
		array<IEntity> occupants = {};
		array<int> occupantStorage = {};
		array<int> occupantSlot = {};
		for (int storageIndex = 0; storageIndex < storages.Count(); storageIndex++)
		{
			BaseInventoryStorageComponent storage = storages[storageIndex];
			int slots = storage.GetSlotsCount();
			for (int slot = 0; slot < slots; slot++)
			{
				IEntity occupant = storage.Get(slot);
				if (!occupant || occupants.Find(occupant) != -1)
					continue;

				occupants.Insert(occupant);
				occupantStorage.Insert(storageIndex);
				occupantSlot.Insert(slot);
			}
		}

		array<bool> claimed = {};
		claimed.Resize(occupants.Count());
		array<int> children = {};
		array<int> pending = {};
		for (int index = 0; index < items.Count(); index++)
		{
			LF_StorageItemRecord item = items[index];
			if (!item || item.m_iParent != parentIndex)
				continue;

			children.Insert(index);
			int match = FindOccupant(item, occupants, occupantStorage, occupantSlot, claimed, true);
			if (match == -1)
				match = FindOccupant(item, occupants, occupantStorage, occupantSlot, claimed, false);

			if (match == -1)
			{
				pending.Insert(index);
				continue;
			}

			claimed[match] = true;
			resolved[index] = occupants[match];
			SetQuantity(occupants[match], item);
		}

		// What the snapshot does not list is not carried: default prefab contents included.
		for (int occupantIndex = 0; occupantIndex < occupants.Count(); occupantIndex++)
		{
			if (claimed[occupantIndex])
				continue;

			IEntity extra = occupants[occupantIndex];
			if (!manager.TryDeleteItem(extra))
				SCR_EntityHelper.DeleteEntityAndChildren(extra);
		}

		int spawned;
		foreach (int pendingIndex : pending)
		{
			IEntity created = SpawnInto(manager, owner, storages, items[pendingIndex]);
			if (!created)
				continue;

			resolved[pendingIndex] = created;
			spawned++;
		}

		foreach (int childIndex : children)
		{
			IEntity child = resolved[childIndex];
			if (child)
				spawned += ApplyChildren(manager, child, childIndex, items, resolved);
		}

		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	protected static int FindOccupant(LF_StorageItemRecord item, array<IEntity> occupants, array<int> occupantStorage, array<int> occupantSlot, array<bool> claimed, bool exactSlot)
	{
		for (int index = 0; index < occupants.Count(); index++)
		{
			if (claimed[index] || occupantStorage[index] != item.m_iStorage)
				continue;

			if (exactSlot && occupantSlot[index] != item.m_iSlot)
				continue;

			if (EL_Utils.GetPrefabName(occupants[index]) == item.m_sPrefab)
				return index;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetQuantity(IEntity entity, LF_StorageItemRecord item)
	{
		EL_QuantityComponent quantity = EL_QuantityComponent.Cast(entity.FindComponent(EL_QuantityComponent));
		if (!quantity || item.m_iQuantity < 1 || quantity.GetQuantity() == item.m_iQuantity)
			return;

		int amount = item.m_iQuantity;
		int max = quantity.GetMaxQuantity();
		if (max > 0 && amount > max)
		{
			EL_Debug.Warn(FEATURE, string.Format("restored %1 quantity %2 clamped to max %3", item.m_sPrefab, amount, max));
			amount = max;
		}

		quantity.SetQuantity(amount);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity SpawnInto(InventoryStorageManagerComponent manager, IEntity owner, array<BaseInventoryStorageComponent> storages, LF_StorageItemRecord item)
	{
		Resource resource = Resource.Load(item.m_sPrefab);
		if (item.m_sPrefab.IsEmpty() || !resource || !resource.IsValid())
		{
			EL_Debug.Error(FEATURE, string.Format("restore skipped missing prefab '%1' (and anything inside it)", item.m_sPrefab));
			return null;
		}

		EntitySpawnParams spawnParams();
		spawnParams.Transform[3] = owner.GetOrigin();
		IEntity entity = GetGame().SpawnEntityPrefabEx(item.m_sPrefab, false, null, spawnParams);
		if (!entity)
		{
			EL_Debug.Error(FEATURE, string.Format("restore skipped '%1': spawn returned nothing", item.m_sPrefab));
			return null;
		}

		SetQuantity(entity, item);

		// A restored stack keeps its own slot instead of merging into a neighbour.
		bool isStack = entity.FindComponent(EL_QuantityComponent) != null;
		if (isStack)
			EL_QuantityComponent.SetTransferIntent(entity, true);

		bool inserted;
		if (item.m_iStorage >= 0 && item.m_iStorage < storages.Count())
		{
			BaseInventoryStorageComponent storage = storages[item.m_iStorage];
			inserted = manager.TryInsertItemInStorage(entity, storage, item.m_iSlot);
			if (!inserted)
				inserted = manager.TryInsertItemInStorage(entity, storage, -1);
		}

		if (!inserted)
			inserted = manager.TryInsertItem(entity);

		if (!inserted)
		{
			if (isStack)
				EL_QuantityComponent.RemoveTransferIntent(entity);

			EL_Debug.Warn(FEATURE, string.Format("restore could not insert '%1' into %2 - deleting", item.m_sPrefab, EL_Utils.GetPrefabName(owner)));
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return null;
		}

		return entity;
	}
}
