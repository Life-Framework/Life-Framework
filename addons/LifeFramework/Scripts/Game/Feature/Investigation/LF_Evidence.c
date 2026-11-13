//------------------------------------------------------------------------------------------------
//! One open crime scene: where a crime report was filed and until when shots and drops near it
//! count as evidence. Server only; never saved (a restart closes every scene).
class LF_EvidenceScene
{
	int m_iReport;
	string m_sUid;
	string m_sOffence;
	string m_sPlace;
	vector m_vOrigin;
	int m_iOpenedAt;
	int m_iCasings;
}

//------------------------------------------------------------------------------------------------
//! One piece of evidence lying in the world. m_Marker is the entity an officer interacts with: the
//! casing itself, or a paper marker beside a dropped item. Entity ids stay on the server.
class LF_EvidenceRow
{
	EntityID m_Item;
	EntityID m_Marker;
	int m_iReport;
	string m_sKind;
	string m_sOffence;
	string m_sPlace;
	int m_iFoundAt;
}

//------------------------------------------------------------------------------------------------
//! Scene evidence (#268). A crime report opens a scene (LF_WantedReports.Report). While it is open,
//! a shot fired within its radius leaves a casing at the shooter, and an item dropped there is
//! tagged with a marker. An on-duty officer bags a piece with LF_BagEvidenceAction: the bag
//! records the place, the time found, and the crime report, and holds the piece. Untouched
//! evidence expires. Nothing runs per frame: shots and drops are events, and one timer sweeps
//! while anything is lying about. Bounded by LF_EvidenceConfig (scenes, casings per scene, total).
class LF_Evidence
{
	static const string FEATURE = "Evidence";
	static const string KIND_CASING = "casing";
	static const string KIND_DROPPED = "dropped";
	static const ResourceName CASING_PREFAB = "{7D3A9E15C2B4F680}Prefabs/Investigation/LF_EvidenceCasing.et";
	static const ResourceName MARKER_PREFAB = "{4C81F2A6D9E37B05}Prefabs/Investigation/LF_EvidenceMarker.et";
	static const ResourceName BAG_PREFAB = "{A2E64B90F1C8D357}Prefabs/Investigation/LF_EvidenceBag.et";
	static const int SWEEP_MS = 30000;
	static const int DROP_SETTLE_MS = 1500;
	static const int MAX_PENDING_DROPS = 64;

	protected static ref array<ref LF_EvidenceScene> s_aScenes;
	protected static ref array<ref LF_EvidenceRow> s_aRows;
	protected static bool s_bSweepScheduled;
	protected static ref array<EntityID> s_aPendingDrops;
	protected static bool s_bDropScheduled;
	protected static ref array<ref LF_EvidenceShooter> s_aShooters;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_EvidenceScene> Scenes()
	{
		if (!s_aScenes)
			s_aScenes = new array<ref LF_EvidenceScene>();

		return s_aScenes;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_EvidenceRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_EvidenceRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aScenes = null;
		s_aRows = null;
		s_bSweepScheduled = false;
		s_aShooters = null;
		s_aPendingDrops = null;
		s_bDropScheduled = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player body spawned. Its shots can now leave casings. Called from the spawn logic.
	static void WatchShooter(IEntity body)
	{
		if (!Replication.IsServer() || !body)
			return;

		EventHandlerManagerComponent events = EventHandlerManagerComponent.Cast(body.FindComponent(EventHandlerManagerComponent));
		if (!events)
		{
			LF_Debug.Warn(FEATURE, "body has no EventHandlerManagerComponent: its shots leave no casings");
			return;
		}

		if (!s_aShooters)
			s_aShooters = new array<ref LF_EvidenceShooter>();

		for (int i = s_aShooters.Count() - 1; i >= 0; i--)
		{
			if (!GetGame().GetWorld().FindEntityByID(s_aShooters[i].GetBody()))
				s_aShooters.RemoveOrdered(i);
		}

		LF_EvidenceShooter shooter = new LF_EvidenceShooter(body.GetID());
		s_aShooters.Insert(shooter);
		events.RegisterScriptHandler("OnProjectileShot", shooter, shooter.OnProjectileShot, false);
	}

	//------------------------------------------------------------------------------------------------
	static int SceneCount()
	{
		return Scenes().Count();
	}

	//------------------------------------------------------------------------------------------------
	static int EvidenceCount()
	{
		return Rows().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a crime report was filed here. Opens a scene; the oldest closes when the cap is hit.
	static LF_EvidenceScene OpenScene(int report, string uid, string offence, string place, vector origin, int nowMs)
	{
		if (!Replication.IsServer())
			return null;

		LF_EvidenceScene scene = new LF_EvidenceScene();
		scene.m_iReport = report;
		scene.m_sUid = uid;
		scene.m_sOffence = offence;
		scene.m_sPlace = place;
		scene.m_vOrigin = origin;
		scene.m_iOpenedAt = nowMs;

		array<ref LF_EvidenceScene> scenes = Scenes();
		scenes.Insert(scene);
		while (scenes.Count() > LF_EvidenceConfig.MaxScenes())
		{
			scenes.RemoveOrdered(0);
		}

		LF_Debug.Info(FEATURE, string.Format("scene opened for report #%1 (%2) at %3", report, offence, place));
		return scene;
	}

	//------------------------------------------------------------------------------------------------
	//! The open scene covering a point, newest first. Null when none is open there.
	static LF_EvidenceScene FindScene(vector origin, int nowMs)
	{
		int windowMs = LF_EvidenceConfig.SceneSeconds() * 1000;
		float radius = LF_EvidenceConfig.SceneRadius();
		array<ref LF_EvidenceScene> scenes = Scenes();
		for (int i = scenes.Count() - 1; i >= 0; i--)
		{
			LF_EvidenceScene scene = scenes[i];
			if (nowMs - scene.m_iOpenedAt > windowMs)
				continue;

			if (vector.Distance(scene.m_vOrigin, origin) <= radius)
				return scene;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player fired a shot at this point. Leaves one casing when the point is inside an
	//! open scene and the scene still has room.
	//! \return The casing, or null (no scene, scene full, spawn failed).
	static IEntity OnShot(vector origin, int nowMs)
	{
		if (!Replication.IsServer())
			return null;

		LF_EvidenceScene scene = FindScene(origin, nowMs);
		if (!scene)
			return null;

		if (scene.m_iCasings >= LF_EvidenceConfig.MaxCasingsPerScene())
			return null;

		IEntity casing = LF_Utils.SpawnEntityPrefab(CASING_PREFAB, origin + "0 0.1 0");
		if (!casing)
			return null;

		scene.m_iCasings++;
		Track(casing, casing, scene, KIND_CASING, nowMs);
		return casing;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an item left an inventory. Waits for it to settle, then OnDropped decides.
	static void WatchDrop(IEntity item)
	{
		if (!Replication.IsServer() || !item || !GetGame().GetCallqueue())
			return;

		if (Scenes().Count() == 0)
			return;

		if (!s_aPendingDrops)
			s_aPendingDrops = new array<EntityID>();

		if (s_aPendingDrops.Count() >= MAX_PENDING_DROPS)
			return;

		s_aPendingDrops.Insert(item.GetID());
		if (s_bDropScheduled)
			return;

		s_bDropScheduled = true;
		GetGame().GetCallqueue().CallLater(SettleDrops, DROP_SETTLE_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SettleDrops()
	{
		s_bDropScheduled = false;
		if (!s_aPendingDrops)
			return;

		array<EntityID> pending = s_aPendingDrops;
		s_aPendingDrops = null;
		foreach (EntityID itemId : pending)
		{
			IEntity item = GetGame().GetWorld().FindEntityByID(itemId);
			if (!item || item.GetParent())
				continue;

			InventoryItemComponent inventoryItem = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
			if (inventoryItem && inventoryItem.GetParentSlot())
				continue;

			OnDropped(item, System.GetTickCount());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an item lies on the ground. Tags it when it lies inside an open scene.
	//! \return The marker, or null when it is not evidence.
	static IEntity OnDropped(notnull IEntity item, int nowMs)
	{
		if (!Replication.IsServer())
			return null;

		if (IsTracked(item.GetID()) || item.FindComponent(LF_EvidenceBagComponent))
			return null;

		LF_EvidenceScene scene = FindScene(item.GetOrigin(), nowMs);
		if (!scene)
			return null;

		IEntity marker = LF_Utils.SpawnEntityPrefab(MARKER_PREFAB, item.GetOrigin() + "0 0.05 0");
		if (!marker)
			return null;

		Track(item, marker, scene, KIND_DROPPED, nowMs);
		return marker;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Track(notnull IEntity item, notnull IEntity marker, notnull LF_EvidenceScene scene, string kind, int nowMs)
	{
		array<ref LF_EvidenceRow> rows = Rows();
		while (rows.Count() >= LF_EvidenceConfig.MaxEvidence())
		{
			Discard(rows[0], "evidence cap reached");
			rows.RemoveOrdered(0);
		}

		LF_EvidenceRow row = new LF_EvidenceRow();
		row.m_Item = item.GetID();
		row.m_Marker = marker.GetID();
		row.m_iReport = scene.m_iReport;
		row.m_sKind = kind;
		row.m_sOffence = scene.m_sOffence;
		row.m_sPlace = scene.m_sPlace;
		row.m_iFoundAt = nowMs;
		rows.Insert(row);
		LF_Debug.Info(FEATURE, string.Format("%1 left as evidence for report #%2 at %3 (%4 lying about)", kind, scene.m_iReport, scene.m_sPlace, rows.Count()));
		ScheduleSweep();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsTracked(EntityID id)
	{
		return FindRow(id) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes the entity an officer would interact with. A dropped item stays where it lies; only
	//! its marker goes, so untouched is never deleted property.
	protected static void Discard(LF_EvidenceRow row, string why)
	{
		IEntity marker = GetGame().GetWorld().FindEntityByID(row.m_Marker);
		if (marker)
			SCR_EntityHelper.DeleteEntityAndChildren(marker);

		LF_Debug.Info(FEATURE, string.Format("%1 for report #%2 removed: %3", row.m_sKind, row.m_iReport, why));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: removes expired evidence and closed scenes. The timer calls it while anything is open.
	//! \return Evidence pieces removed.
	static int Sweep(int nowMs)
	{
		int removed;
		int expiryMs = LF_EvidenceConfig.ExpirySeconds() * 1000;
		array<ref LF_EvidenceRow> rows = Rows();
		for (int i = rows.Count() - 1; i >= 0; i--)
		{
			LF_EvidenceRow row = rows[i];
			IEntity marker = GetGame().GetWorld().FindEntityByID(row.m_Marker);
			if (!marker)
			{
				rows.RemoveOrdered(i);
				continue;
			}

			if (nowMs - row.m_iFoundAt < expiryMs)
				continue;

			Discard(row, "expired untouched");
			rows.RemoveOrdered(i);
			removed++;
		}

		int windowMs = LF_EvidenceConfig.SceneSeconds() * 1000;
		array<ref LF_EvidenceScene> scenes = Scenes();
		for (int s = scenes.Count() - 1; s >= 0; s--)
		{
			if (nowMs - scenes[s].m_iOpenedAt > windowMs)
				scenes.RemoveOrdered(s);
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ScheduleSweep()
	{
		if (s_bSweepScheduled || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bSweepScheduled = true;
		GetGame().GetCallqueue().CallLater(SweepTimer, SWEEP_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SweepTimer()
	{
		s_bSweepScheduled = false;
		Sweep(System.GetTickCount());
		if (Rows().Count() > 0)
			ScheduleSweep();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an on-duty officer bags a piece. Spawns an evidence bag stamped with the report, the
	//! place and the time found, seals the piece inside it, and hands the bag to the officer.
	//! \return The bag, or null when the entity is no longer evidence.
	static IEntity Bag(IEntity evidence, IEntity officer, int nowMs)
	{
		if (!Replication.IsServer() || !evidence)
			return null;

		LF_EvidenceRow row = FindRow(evidence.GetID());
		if (!row)
		{
			LF_Debug.Warn(FEATURE, "bag refused: that is not scene evidence (expired or already bagged)");
			return null;
		}

		IEntity item = GetGame().GetWorld().FindEntityByID(row.m_Item);
		if (!item)
		{
			LF_Debug.Warn(FEATURE, "bag refused: the evidence is gone");
			Discard(row, "evidence entity gone");
			Rows().RemoveItem(row);
			return null;
		}

		IEntity bag = LF_Utils.SpawnEntityPrefab(BAG_PREFAB, item.GetOrigin() + "0 0.1 0");
		LF_EvidenceBagComponent bagComponent;
		if (bag)
			bagComponent = LF_EvidenceBagComponent.Cast(bag.FindComponent(LF_EvidenceBagComponent));

		if (!bagComponent)
		{
			LF_Debug.Error(FEATURE, "bag refused: the evidence bag prefab did not spawn with its component");
			if (bag)
				SCR_EntityHelper.DeleteEntityAndChildren(bag);

			return null;
		}

		int minutes = (nowMs - row.m_iFoundAt) / 60000;
		bagComponent.Stamp(row.m_iReport, row.m_sOffence, row.m_sPlace, row.m_sKind, minutes);
		bagComponent.Seal(item);

		if (row.m_Marker != row.m_Item)
		{
			IEntity marker = GetGame().GetWorld().FindEntityByID(row.m_Marker);
			if (marker)
				SCR_EntityHelper.DeleteEntityAndChildren(marker);
		}

		Rows().RemoveItem(row);
		LF_Debug.Info(FEATURE, string.Format("%1 bagged for report #%2 (%3)", row.m_sKind, row.m_iReport, row.m_sPlace));

		SCR_InventoryStorageManagerComponent inventory;
		if (officer)
			inventory = SCR_InventoryStorageManagerComponent.Cast(officer.FindComponent(SCR_InventoryStorageManagerComponent));

		if (inventory && !inventory.TryInsertItem(bag))
			LF_Debug.Warn(FEATURE, "the officer's inventory is full: the bag lies at the scene");

		return bag;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EvidenceRow FindRow(EntityID id)
	{
		foreach (LF_EvidenceRow row : Rows())
		{
			if (row.m_Item == id || row.m_Marker == id)
				return row;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! Listens to one player body's shots (server). The event handler needs a live Managed instance,
//! so LF_Evidence.WatchShooter keeps one per body in a table it prunes on every new body.
class LF_EvidenceShooter : Managed
{
	protected EntityID m_Body;

	//------------------------------------------------------------------------------------------------
	void LF_EvidenceShooter(EntityID body)
	{
		m_Body = body;
	}

	//------------------------------------------------------------------------------------------------
	EntityID GetBody()
	{
		return m_Body;
	}

	//------------------------------------------------------------------------------------------------
	//! One shot, not one frame. The casing falls where the shooter stands.
	void OnProjectileShot(int playerId, BaseWeaponComponent weapon, IEntity projectile)
	{
		IEntity body = GetGame().GetWorld().FindEntityByID(m_Body);
		if (!body)
			return;

		LF_Evidence.OnShot(body.GetOrigin(), System.GetTickCount());
		// The taser's dart finds no hit zone on a town NPC, so its shot is read here (LF_NpcStun).
		LF_NpcStun.OnShot(body, projectile);
	}
}
