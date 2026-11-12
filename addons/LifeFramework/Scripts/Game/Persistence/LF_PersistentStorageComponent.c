//------------------------------------------------------------------------------------------------
//! Placed storage whose contents survive a restart under a stable key, not an EntityID. The
//! entity can move to another layer or position: as long as it keeps its key, the next boot
//! puts the same items back in it.
//!
//! STORE. The contents are an LF_StorageSnapshot record (owner key = the storage key, slot
//! SLOT_PLACED), so they ride the existing "carried" payload of LF_PersistenceComponentSerializer
//! with no new save version.
//!
//! CAPTURE. On every item added to or removed from the owner's storages (coalesced to one capture
//! per frame), and for every placed storage at each save point (CaptureAllPlaced), which also
//! catches a stack whose quantity changed in place.
//!
//! RESTORE. A frame after the entity initializes, and again a frame after LF_StorageSnapshot.ApplyAll
//! loads a save (RestoreAllPlaced). LF_StorageSnapshot.Apply reconciles, so restoring twice spawns
//! nothing the second time. With no saved record the storage is left as it is.
//!
//! The owner needs a ScriptedInventoryStorageManagerComponent (Apply inserts through it) and no
//! native Persistence component, so the engine does not save the contents a second time.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Persistence", description: "Placed storage whose contents survive a restart under a stable key.")]
class LF_PersistentStorageComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_PersistentStorageComponent : ScriptComponent
{
	static const string SLOT_PLACED = "placed";
	static const string FEATURE = "PlacedStorage";

	[Attribute("", UIWidgets.EditBox, desc: "Stable key the contents are saved under, e.g. hideout:Roadmap_Hideout. Keep it when the entity moves. Empty uses the entity name.")]
	protected string m_sStorageKey;

	//! Storage key -> placed entity. EntityID, not IEntity: entities can be deleted at any time.
	protected static ref map<string, EntityID> s_mPlaced;

	protected bool m_bApplying;

	//------------------------------------------------------------------------------------------------
	string GetStorageKey()
	{
		if (!m_sStorageKey.IsEmpty())
			return m_sStorageKey;

		if (GetOwner())
			return GetOwner().GetName();

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Adds every placed, registered storage entity that still exists to result (server only).
	static void GetPlaced(notnull array<IEntity> result)
	{
		if (!s_mPlaced || !GetGame() || !GetGame().GetWorld())
			return;

		foreach (string key, EntityID id : s_mPlaced)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(id);
			if (entity)
				result.Insert(entity);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer() || !GetGame() || !GetGame().InPlayMode())
			return;

		string key = GetStorageKey();
		if (key.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("placed storage %1 at %2 has no storage key and no entity name; its contents will not persist", LF_Utils.GetPrefabName(owner), owner.GetOrigin()));
			return;
		}

		if (!s_mPlaced)
			s_mPlaced = new map<string, EntityID>();

		IEntity registered = Resolve(key);
		if (registered && registered != owner)
		{
			LF_Debug.Error(FEATURE, string.Format("second placed storage with key %1 at %2 ignored; the first one owns the key", key, owner.GetOrigin()));
			return;
		}

		ScriptedInventoryStorageManagerComponent manager = ScriptedInventoryStorageManagerComponent.Cast(owner.FindComponent(ScriptedInventoryStorageManagerComponent));
		if (!manager)
		{
			LF_Debug.Error(FEATURE, string.Format("placed storage %1 has no ScriptedInventoryStorageManagerComponent; its contents will not persist", key));
			return;
		}

		s_mPlaced.Set(key, owner.GetID());
		manager.m_OnItemAddedInvoker.Insert(OnContentsChanged);
		manager.m_OnItemRemovedInvoker.Insert(OnContentsChanged);
		LF_Debug.Info(FEATURE, string.Format("placed storage %1 registered at %2", key, owner.GetOrigin()));

		// The storages register with the manager during init; restore once they are there.
		GetGame().GetCallqueue().CallLater(RestoreFromInit, 0);
	}

	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(RestoreFromInit);
			GetGame().GetCallqueue().Remove(CaptureQueued);
		}

		string key = GetStorageKey();
		if (s_mPlaced && owner && s_mPlaced.Contains(key) && s_mPlaced.Get(key) == owner.GetID())
			s_mPlaced.Remove(key);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void RestoreFromInit()
	{
		Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnContentsChanged(IEntity item, BaseInventoryStorageComponent storage)
	{
		if (m_bApplying || !Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue.GetRemainingTime(CaptureQueued) == -1)
			queue.CallLater(CaptureQueued, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void CaptureQueued()
	{
		Capture();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stores the current contents under the storage key.
	bool Capture()
	{
		IEntity owner = GetOwner();
		string key = GetStorageKey();
		if (!owner || key.IsEmpty())
			return false;

		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Capture(key, SLOT_PLACED, owner);
		if (!record)
			return false;

		LF_StorageSnapshot.Set(record);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: makes the contents equal the saved record. No record leaves the storage as it is.
	//! \return Number of items spawned (0 when the contents already match).
	int Restore()
	{
		IEntity owner = GetOwner();
		string key = GetStorageKey();
		if (!owner || key.IsEmpty() || !Replication.IsServer())
			return 0;

		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Get(key, SLOT_PLACED);
		if (!record)
			return 0;

		m_bApplying = true;
		int spawned = LF_StorageSnapshot.Apply(owner, record);
		m_bApplying = false;

		LF_Debug.Info(FEATURE, string.Format("placed storage %1 restored: %2 record(s), %3 spawned", key, record.m_aItems.Count(), spawned));
		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The placed entity registered under the key, or null.
	static IEntity Resolve(string key)
	{
		if (!s_mPlaced || key.IsEmpty() || !s_mPlaced.Contains(key) || !GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(s_mPlaced.Get(key));
	}

	//------------------------------------------------------------------------------------------------
	static LF_PersistentStorageComponent Find(string key)
	{
		IEntity owner = Resolve(key);
		if (!owner)
			return null;

		return LF_PersistentStorageComponent.Cast(owner.FindComponent(LF_PersistentStorageComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Server, save point: refreshes the record of every placed storage.
	//! \return Number of placed storages captured.
	static int CaptureAllPlaced()
	{
		if (!s_mPlaced || !Replication.IsServer())
			return 0;

		int captured;
		foreach (string key, EntityID id : s_mPlaced)
		{
			LF_PersistentStorageComponent component = Find(key);
			if (!component)
			{
				LF_Debug.Warn(FEATURE, string.Format("save skipped placed storage %1: the entity is gone, its last record is kept", key));
				continue;
			}

			if (component.Capture())
				captured++;
		}

		return captured;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, after a load: puts every placed storage back to its record.
	//! \return Number of items spawned across all placed storages.
	static int RestoreAllPlaced()
	{
		if (!s_mPlaced || !Replication.IsServer())
			return 0;

		int spawned;
		foreach (string key, EntityID id : s_mPlaced)
		{
			LF_PersistentStorageComponent component = Find(key);
			if (component)
				spawned += component.Restore();
		}

		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	//! A frame after a load, so the restore never spawns entities inside the deserialize pass.
	static void ScheduleRestoreAllPlaced()
	{
		if (!s_mPlaced || s_mPlaced.IsEmpty())
			return;

		if (!GetGame() || !GetGame().GetCallqueue())
		{
			RestoreAllPlaced();
			return;
		}

		GetGame().GetCallqueue().Remove(RestoreAllPlacedQueued);
		GetGame().GetCallqueue().CallLater(RestoreAllPlacedQueued, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected static void RestoreAllPlacedQueued()
	{
		RestoreAllPlaced();
	}

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset). Placed storages register again on the next world load.
	static void ResetStatics()
	{
		s_mPlaced = null;
	}
}
