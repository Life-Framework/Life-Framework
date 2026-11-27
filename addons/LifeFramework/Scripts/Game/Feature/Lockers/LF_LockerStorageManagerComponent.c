//------------------------------------------------------------------------------------------------
//! Why opening a locker was or was not allowed (#2661). Logged and shown to the renter.
enum LF_ELockerOpenResult
{
	OPENED,
	NOT_RENTER,	//!< the character holds no locker at this site
	OUT_OF_REACH,	//!< no locker bank within reach of the character
	BAD_CALLER,	//!< no character, no character key or no site behind the request
	NO_CONTAINER,	//!< the contents container could not be built
	NOT_SERVER	//!< opening is a server decision
}

//------------------------------------------------------------------------------------------------
//! A renter's open locker (#2661, chapter #1038). The contents are not a placed entity: they are an
//! LF_StorageSnapshotRecord (owner = the character record key, slot "locker:<siteId>") riding the
//! existing "carried" payload, so they survive death and a restart with no new save section. Each
//! open builds a transient container (LF_LockerContents.et) at the bank, fills it from that snapshot
//! (LF_StorageSnapshot.Apply) and opens it on the renter's client only. Every change recaptures the
//! snapshot (coalesced to one per frame), and close, disconnect, death, walking away and the save
//! points capture it again before the container is deleted, so the snapshot is current the moment it
//! stops being on screen.
//!
//! One character's locker is another's empty one: the snapshot key holds the character, and the
//! engine's access validation lets only the renter's own inventory touch the container (the same
//! seam as the hideout stash manager, LF_HideoutStashStorageManagerComponent).
//!
//! Duplication guard (#753): the carried snapshot of the opener is captured in the same step as the
//! locker snapshot, on every release, so an item that moved between them is in exactly one of the
//! two when a save is written. A character already open is released (captured) before it opens again.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Lockers", description: "Inventory manager of an open locker's transient contents. Only the renter's own inventory moves items in or out.")]
class LF_LockerStorageManagerComponentClass : ScriptedInventoryStorageManagerComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_LockerStorageManagerComponent : ScriptedInventoryStorageManagerComponent
{
	static const string FEATURE = "Lockers";
	static const string SLOT_PREFIX = "locker:";
	static const ResourceName CONTENTS_PREFAB = "{87647039A774C064}Prefabs/Props/Lockers/LF_LockerContents.et";

	//! How far past the bank's reach the renter may walk before the locker closes under them (metres).
	static const float WALK_AWAY_SLACK = 3.0;	// lf-fixed: slack past the bank reach, not a tunable
	//! Seconds between checks that the renter is still there.
	static const int WATCH_MS = 1500;	// lf-fixed: an internal poll interval, not gameplay

	//! "<siteId>|<characterKey>" -> the open container. EntityID, not IEntity: entities can be deleted at any time.
	protected static ref map<string, EntityID> s_mOpen;

	protected string m_sSiteId;
	protected string m_sCharacterKey;
	protected EntityID m_OpenerId;
	protected float m_fReach;
	protected vector m_vBankPos;
	protected bool m_bApplying;
	protected bool m_bReleased;

	//------------------------------------------------------------------------------------------------
	//! The snapshot slot a site's contents live under (per character: the owner key is the character).
	static string SlotFor(string siteId)
	{
		return SLOT_PREFIX + siteId;
	}

	//------------------------------------------------------------------------------------------------
	protected static string OpenKey(string siteId, string characterKey)
	{
		return siteId + "|" + characterKey;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, EntityID> Open()
	{
		if (!s_mOpen)
			s_mOpen = new map<string, EntityID>();

		return s_mOpen;
	}

	//------------------------------------------------------------------------------------------------
	//! The open container of this character at this site, or null.
	static LF_LockerStorageManagerComponent FindOpen(string siteId, string characterKey)
	{
		if (!s_mOpen || !GetGame() || !GetGame().GetWorld())
			return null;

		EntityID id;
		if (!s_mOpen.Find(OpenKey(siteId, characterKey), id))
			return null;

		IEntity entity = GetGame().GetWorld().FindEntityByID(id);
		if (!entity)
			return null;

		return LF_LockerStorageManagerComponent.Cast(entity.FindComponent(LF_LockerStorageManagerComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Lockers open right now.
	static int OpenCount()
	{
		if (!s_mOpen)
			return 0;

		return s_mOpen.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the acting character opens their locker at the nearest bank within that bank's reach.
	//! The bank is found from the character's own position and the renter from their own record key;
	//! nothing a client sends picks either.
	//! \param[out] container The transient container, set when the result is OPENED.
	static LF_ELockerOpenResult OpenNearest(IEntity user, out IEntity container)
	{
		if (!Replication.IsServer())
			return LF_ELockerOpenResult.NOT_SERVER;

		if (!user)
		{
			LF_Debug.Error(FEATURE, "open refused: no character behind the request");
			return LF_ELockerOpenResult.BAD_CALLER;
		}

		LF_LockerBankComponent bank = LF_LockerBankComponent.FindNearest(user.GetOrigin());
		if (!bank)
		{
			LF_Debug.Info(FEATURE, string.Format("open refused at %1: no locker bank within reach", user.GetOrigin()));
			return LF_ELockerOpenResult.OUT_OF_REACH;
		}

		return OpenAtTime(System.GetUnixTime(), user, LF_Utils.GetPlayerUID(user), bank.GetSiteId(), user.GetOrigin(), bank.GetOwner().GetOrigin(), bank.GetReach(), LF_LockerConfig.RentPeriodDays(), LF_LockerConfig.GracePeriodDays(), container);
	}

	//------------------------------------------------------------------------------------------------
	//! OpenAt with the lapse applied first (#3457): a rent that has lapsed since the last sweep moves to the
	//! site's lost-and-found here, so the lapsed renter is refused instead of opening the locker for free.
	static LF_ELockerOpenResult OpenAtTime(int now, IEntity user, string characterKey, string siteId, vector userPos, vector bankPos, float reach, int rentPeriodDays, int gracePeriodDays, out IEntity container)
	{
		LF_LostAndFound.MoveLapsed(now, siteId, characterKey, rentPeriodDays, gracePeriodDays);
		return OpenAt(user, characterKey, siteId, userPos, bankPos, reach, container);
	}

	//------------------------------------------------------------------------------------------------
	//! The seam tests drive with their own key, site and positions. Builds the container only when
	//! every check passes: the character is in reach and rents a locker at the site.
	static LF_ELockerOpenResult OpenAt(IEntity user, string characterKey, string siteId, vector userPos, vector bankPos, float reach, out IEntity container)
	{
		if (!Replication.IsServer())
			return LF_ELockerOpenResult.NOT_SERVER;

		if (!user || characterKey.IsEmpty() || siteId.IsEmpty())
		{
			LF_Debug.Error(FEATURE, string.Format("open refused: missing character (%1) or site (%2)", characterKey, siteId));
			return LF_ELockerOpenResult.BAD_CALLER;
		}

		if (vector.Distance(userPos, bankPos) > reach)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused at %2: out of reach", characterKey, siteId));
			return LF_ELockerOpenResult.OUT_OF_REACH;
		}

		if (!LF_LockerBank.IsRenting(siteId, characterKey))
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused at %2: no locker rented here", characterKey, siteId));
			return LF_ELockerOpenResult.NOT_RENTER;
		}

		// Already open (a second press, or a body left from a dropped client): capture it, then build fresh.
		LF_LockerStorageManagerComponent previous = FindOpen(siteId, characterKey);
		if (previous)
			previous.Release("reopened");

		container = LF_Utils.SpawnEntityPrefab(CONTENTS_PREFAB, bankPos);
		if (!container)
		{
			LF_Debug.Error(FEATURE, string.Format("open failed for %1 at %2: the contents container did not spawn", characterKey, siteId));
			return LF_ELockerOpenResult.NO_CONTAINER;
		}

		LF_LockerStorageManagerComponent manager = LF_LockerStorageManagerComponent.Cast(container.FindComponent(LF_LockerStorageManagerComponent));
		if (!manager)
		{
			LF_Debug.Error(FEATURE, string.Format("open failed for %1 at %2: %3 has no LF_LockerStorageManagerComponent", characterKey, siteId, LF_Utils.GetPrefabName(container)));
			SCR_EntityHelper.DeleteEntityAndChildren(container);
			container = null;
			return LF_ELockerOpenResult.NO_CONTAINER;
		}

		manager.Bind(user, characterKey, siteId, bankPos, reach);
		return LF_ELockerOpenResult.OPENED;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: fills the new container from the renter's snapshot and starts watching it.
	protected void Bind(IEntity user, string characterKey, string siteId, vector bankPos, float reach)
	{
		m_sSiteId = siteId;
		m_sCharacterKey = characterKey;
		m_OpenerId = user.GetID();
		m_vBankPos = bankPos;
		m_fReach = reach;
		Open().Set(OpenKey(siteId, characterKey), GetOwner().GetID());

		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Get(characterKey, SlotFor(siteId));
		if (record)
		{
			m_bApplying = true;
			int spawned = LF_StorageSnapshot.Apply(GetOwner(), record);
			m_bApplying = false;
			LF_Debug.Info(FEATURE, string.Format("%1 opened the locker at %2: %3 record(s), %4 spawned", characterKey, siteId, record.m_aItems.Count(), spawned));
		}
		else
		{
			LF_Debug.Info(FEATURE, string.Format("%1 opened an empty locker at %2", characterKey, siteId));
		}

		m_OnItemAddedInvoker.Insert(OnContentsChanged);
		m_OnItemRemovedInvoker.Insert(OnContentsChanged);
		GetGame().GetCallqueue().CallLater(WatchTick, WATCH_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnContentsChanged(IEntity item, BaseInventoryStorageComponent storage)
	{
		if (m_bApplying || m_bReleased || !Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
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
	//! Server: stores the current contents as the renter's snapshot for this site.
	bool Capture()
	{
		if (m_bReleased || m_sCharacterKey.IsEmpty() || m_sSiteId.IsEmpty())
			return false;

		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Capture(m_sCharacterKey, SlotFor(m_sSiteId), GetOwner());
		if (!record)
			return false;

		LF_StorageSnapshot.Set(record);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, save point: refreshes the snapshot of every open locker.
	//! \return Lockers captured.
	static int CaptureAllOpen()
	{
		if (!s_mOpen || !GetGame() || !GetGame().GetWorld())
			return 0;

		int captured;
		foreach (string key, EntityID id : s_mOpen)
		{
			IEntity entity = GetGame().GetWorld().FindEntityByID(id);
			if (!entity)
				continue;

			LF_LockerStorageManagerComponent manager = LF_LockerStorageManagerComponent.Cast(entity.FindComponent(LF_LockerStorageManagerComponent));
			if (manager && manager.Capture())
				captured++;
		}

		return captured;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, every WATCH_MS: the locker closes when its renter is gone (disconnected), no longer
	//! alive, or has walked away from the bank.
	protected void WatchTick()
	{
		IEntity opener = GetGame().GetWorld().FindEntityByID(m_OpenerId);
		if (!opener)
		{
			Release("renter gone");
			return;
		}

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager && playerManager.GetPlayerIdFromControlledEntity(opener) <= 0)
		{
			Release("renter disconnected");
			return;
		}

		if (!LF_Utils.IsAliveState(opener))
		{
			Release("renter down");
			return;
		}

		if (vector.Distance(opener.GetOrigin(), m_vBankPos) > m_fReach + WALK_AWAY_SLACK)
			Release("renter walked away");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: stores the contents and the renter's carried inventory together, then deletes the
	//! transient container. The snapshot is the locker from here on.
	//! \return False when it was already released.
	bool Release(string why)
	{
		if (m_bReleased || !Replication.IsServer())
			return false;

		Capture();
		IEntity opener = GetGame().GetWorld().FindEntityByID(m_OpenerId);
		// The locker is keyed by the record key (LF_Utils.GetPlayerUID); the carried snapshot by the
		// character id, which differs for a character in slot 1 (its record key is the account uid).
		if (opener && LF_Utils.IsAliveState(opener))
			LF_StorageSnapshot.CaptureCarried(LF_PlayerAccountManager.CharacterIdOfKey(m_sCharacterKey), opener);

		m_bReleased = true;
		ScriptCallQueue queue = GetGame().GetCallqueue();
		queue.Remove(WatchTick);
		queue.Remove(CaptureQueued);
		m_OnItemAddedInvoker.Remove(OnContentsChanged);
		m_OnItemRemovedInvoker.Remove(OnContentsChanged);

		string key = OpenKey(m_sSiteId, m_sCharacterKey);
		EntityID id;
		if (s_mOpen && s_mOpen.Find(key, id) && id == GetOwner().GetID())
			s_mOpen.Remove(key);

		LF_Debug.Info(FEATURE, string.Format("%1 closed the locker at %2 (%3)", m_sCharacterKey, m_sSiteId, why));
		SCR_EntityHelper.DeleteEntityAndChildren(GetOwner());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether this inventory manager belongs to the renter who opened this locker.
	bool IsRenterManager(InventoryStorageManagerComponent instigatorManager)
	{
		if (!instigatorManager)
			return false;

		return instigatorManager.GetOwner().GetID() == m_OpenerId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the renter's own character entity, or null.
	IEntity GetOpener()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(m_OpenerId);
	}

	//------------------------------------------------------------------------------------------------
	string GetSiteId()
	{
		return m_sSiteId;
	}

	//------------------------------------------------------------------------------------------------
	//! The engine asks this when a client's inventory request moves or removes an item the container
	//! holds. Only the renter's own inventory may; everyone else is refused and it is logged.
	override protected bool HandleAccessValidation(InventoryStorageManagerComponent instigatorManager, IEntity item, EInventoryManagerOperation operation)
	{
		if (!Replication.IsServer() || instigatorManager == this || IsRenterManager(instigatorManager))
			return true;

		LF_Debug.Warn(FEATURE, string.Format("locker %1 kept %2: the taker is not the renter", m_sSiteId, LF_Utils.GetPrefabName(item)));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key the renter sees for a result.
	static string ResultKey(LF_ELockerOpenResult result)
	{
		switch (result)
		{
			case LF_ELockerOpenResult.NOT_RENTER:
				return "#LF-LockerBank_NotRenter";
			case LF_ELockerOpenResult.OUT_OF_REACH:
				return "#LF-LockerBank_OutOfReach";
		}

		return "#LF-LockerBank_Refused";
	}

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset): the containers die with the world, their snapshots are already stored.
	static void ResetStatics()
	{
		s_mOpen = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LockerStorageManagerComponent's statics (LF_StaticReset finds it by reflection).
class LF_LockerStorageManagerComponent_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LockerStorageManagerComponent";
	}

	override void Apply()
	{
		LF_LockerStorageManagerComponent.ResetStatics();
	}
}
