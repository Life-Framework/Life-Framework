//------------------------------------------------------------------------------------------------
//! Permanent placements (#1032, slice #2653): a thing a player placed whose placeable is PERMANENT
//! (a chair) is a row here, saved in the "placements" payload, spawned again on load, and gone once
//! it is picked up (LF_PlacementPickup). TEMPORARY kinds (the spike strip) never get a row.
//! Caps: an owner holds at most `m_iOwnerCap` rows (40) and a property at most `m_iPropertyCap` (25);
//! both are on the placement policy conf beside the kinds. The live entity is found through an
//! in-memory EntityID map that is never saved: the row keeps a stable id, the prefab and the spot.
//------------------------------------------------------------------------------------------------

//! One saved permanent placement. Positional in a binary save: append fields only, with a version bump.
class LF_PlacementRow
{
	static const int VERSION = 1;

	int m_iVersion;
	string m_sPlacementId;
	string m_sPrefab;
	vector m_vPos;
	//! Yaw, pitch, roll in degrees (Math3D.MatrixToAngles).
	vector m_vAngles;
	//! The placer's character key (LF_Houses.CharacterKeyOf).
	string m_sOwnerKey;
	string m_sKind;
	//! The house the spot belongs to (nearest bound house within 30 m), empty outside any.
	string m_sPropertyId;
	//! The carried item a pick-up gives back, empty for none.
	string m_sItemPrefab;

	//------------------------------------------------------------------------------------------------
	static LF_PlacementRow Create(string id, ResourceName prefab, vector transform[4], string ownerKey, string kind, string propertyId, ResourceName itemPrefab)
	{
		LF_PlacementRow row();
		row.m_iVersion = VERSION;
		row.m_sPlacementId = id;
		row.m_sPrefab = prefab;
		row.m_vPos = transform[3];
		row.m_vAngles = Math3D.MatrixToAngles(transform);
		row.m_sOwnerKey = ownerKey;
		row.m_sKind = kind;
		row.m_sPropertyId = propertyId;
		row.m_sItemPrefab = itemPrefab;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
class LF_PlacementSave
{
	static const string FEATURE = "Placement";
	static const int DEFAULT_OWNER_CAP = 40; // lf-fixed: fallback only; the cap is LF_PlacementPolicyConfig.m_iOwnerCap
	static const int DEFAULT_PROPERTY_CAP = 25; // lf-fixed: fallback only; the cap is LF_PlacementPolicyConfig.m_iPropertyCap

	//! Placement id to row; the source of truth for the save and the caps.
	protected static ref map<string, ref LF_PlacementRow> s_mRows = new map<string, ref LF_PlacementRow>();
	//! Placement id to the live entity standing for it (this session only, never saved).
	protected static ref map<string, EntityID> s_mLive = new map<string, EntityID>();
	protected static int s_iNextId;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mRows = new map<string, ref LF_PlacementRow>();
		s_mLive = new map<string, EntityID>();
		s_iNextId = 0;
	}

	//------------------------------------------------------------------------------------------------
	static int OwnerCap()
	{
		LF_PlacementPolicyConfig config = LF_PlacementPolicyConfig.Get();
		if (config && config.m_iOwnerCap > 0)
			return config.m_iOwnerCap;

		return DEFAULT_OWNER_CAP;
	}

	//------------------------------------------------------------------------------------------------
	static int PropertyCap()
	{
		LF_PlacementPolicyConfig config = LF_PlacementPolicyConfig.Get();
		if (config && config.m_iPropertyCap > 0)
			return config.m_iPropertyCap;

		return DEFAULT_PROPERTY_CAP;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return s_mRows.Count();
	}

	//------------------------------------------------------------------------------------------------
	static int CountOwned(string ownerKey)
	{
		int count;
		foreach (string id, LF_PlacementRow row : s_mRows)
		{
			if (row && row.m_sOwnerKey == ownerKey)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	static int CountOnProperty(string propertyId)
	{
		if (propertyId.IsEmpty())
			return 0;

		int count;
		foreach (string id, LF_PlacementRow row : s_mRows)
		{
			if (row && row.m_sPropertyId == propertyId)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: why one more permanent placement for this owner on this property is refused, empty when
	//! it is not. The reason names the cap.
	static string CapProblem(string ownerKey, string propertyId, out string toastKey)
	{
		toastKey = "";
		int ownerCap = OwnerCap();
		if (CountOwned(ownerKey) >= ownerCap)
		{
			toastKey = "#LF-Placement_CapOwner";
			return string.Format("the owner cap of %1 permanent placements is reached", ownerCap);
		}

		int propertyCap = PropertyCap();
		if (CountOnProperty(propertyId) >= propertyCap)
		{
			toastKey = "#LF-Placement_CapProperty";
			return string.Format("the property cap of %1 permanent placements is reached on %2", propertyCap, propertyId);
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! The house a spot belongs to, for the property cap: the nearest bound house within 30 m.
	static string PropertyAt(vector spot)
	{
		LF_PlacementWhereFacts facts = new LF_PlacementWhereFacts();
		array<string> noKeys = {};
		LF_PlacementZones.GatherHouseFor(spot, "", noKeys, facts);
		return facts.m_sHouseId;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, the service's confirm before it charges or spawns: the cap check for a permanent
	//! placeable, empty for a temporary one or when under both caps.
	static string CheckBeforePlace(IEntity player, notnull LF_PlaceableComponent placeable, vector spot, out string toastKey)
	{
		toastKey = "";
		if (placeable.GetLifetime() != LF_EPlacementLifetime.PERMANENT)
			return "";

		return CapProblem(LF_Houses.CharacterKeyOf(player), PropertyAt(spot), toastKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, the service's confirm after the spawn: records a permanent placement. Nothing for a
	//! temporary one. \return the new placement id, empty when nothing was recorded.
	static string OnPlaced(IEntity player, notnull LF_PlaceableComponent placeable, IEntity item, IEntity placed)
	{
		if (!placed || placeable.GetLifetime() != LF_EPlacementLifetime.PERMANENT)
			return "";

		ResourceName itemPrefab;
		if (item && item.GetPrefabData())
			itemPrefab = item.GetPrefabData().GetPrefabName();

		return Record(LF_Houses.CharacterKeyOf(player), placeable.GetKind(), placed, itemPrefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Records `placed` as a permanent placement of `ownerKey`. \return its id, empty when refused.
	static string Record(string ownerKey, string kind, IEntity placed, ResourceName itemPrefab = "")
	{
		if (!placed || !placed.GetPrefabData())
		{
			LF_Debug.Error(FEATURE, "permanent placement not recorded: no entity or no prefab");
			return "";
		}

		vector transform[4];
		placed.GetWorldTransform(transform);
		string id = NewId();
		LF_PlacementRow row = LF_PlacementRow.Create(id, placed.GetPrefabData().GetPrefabName(), transform, ownerKey, kind, PropertyAt(transform[3]), itemPrefab);
		s_mRows.Set(id, row);
		s_mLive.Set(id, placed.GetID());
		LF_PlacedOwnerComponent.Stamp(placed, ownerKey);
		LF_Debug.Info(FEATURE, string.Format("recorded permanent placement %1 (%2) for %3 at %4, %5 owned", id, kind, ownerKey, row.m_vPos, CountOwned(ownerKey)));
		return id;
	}

	//------------------------------------------------------------------------------------------------
	protected static string NewId()
	{
		s_iNextId++;
		string id = string.Format("pl-%1-%2", System.GetUnixTime(), s_iNextId);
		while (s_mRows.Contains(id))
		{
			s_iNextId++;
			id = string.Format("pl-%1-%2", System.GetUnixTime(), s_iNextId);
		}

		return id;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The placement id of a live entity, empty when it is not a recorded permanent placement.
	static string IdOf(IEntity entity)
	{
		if (!entity)
			return "";

		EntityID wanted = entity.GetID();
		foreach (string id, EntityID live : s_mLive)
		{
			if (live == wanted)
				return id;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The row of a placement id, null when there is none.
	static LF_PlacementRow Find(string id)
	{
		return s_mRows.Get(id);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The live entity of a placement id, null when it is not standing this session.
	static IEntity LiveEntity(string id)
	{
		EntityID live;
		if (!s_mLive.Find(id, live) || live == EntityID.INVALID)
			return null;

		return GetGame().GetWorld().FindEntityByID(live);
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets a placement (LF_PlacementPickup deletes the entity). \return true when it was recorded.
	static bool Forget(string id)
	{
		if (!s_mRows.Contains(id))
			return false;

		s_mRows.Remove(id);
		s_mLive.Remove(id);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The rows to save, each at its live entity's spot when it stands.
	static array<ref LF_PlacementRow> ExportAll()
	{
		array<ref LF_PlacementRow> rows = new array<ref LF_PlacementRow>();
		foreach (string id, LF_PlacementRow row : s_mRows)
		{
			if (!row)
				continue;

			IEntity live = LiveEntity(id);
			if (live)
			{
				vector transform[4];
				live.GetWorldTransform(transform);
				row.m_vPos = transform[3];
				row.m_vAngles = Math3D.MatrixToAngles(transform);
			}

			rows.Insert(row);
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Load: spawns every row that does not already stand. Idempotent: a row whose entity stands is
	//! left alone, so a second apply makes no duplicate. A row whose prefab no longer resolves is
	//! skipped with an error and not kept. \return how many rows stand after the apply.
	static int ApplyAll(array<ref LF_PlacementRow> rows)
	{
		if (!rows)
			return 0;

		int standing;
		foreach (LF_PlacementRow row : rows)
		{
			if (!row || row.m_sPlacementId.IsEmpty())
				continue;

			if (LiveEntity(row.m_sPlacementId))
			{
				standing++;
				continue;
			}

			Resource resource = Resource.Load(row.m_sPrefab);
			if (!resource || !resource.IsValid())
			{
				LF_Debug.Error(FEATURE, string.Format("permanent placement %1 skipped: prefab does not resolve: %2 (owner %3)", row.m_sPlacementId, row.m_sPrefab, row.m_sOwnerKey));
				continue;
			}

			IEntity placed = LF_Utils.SpawnEntityPrefab(row.m_sPrefab, row.m_vPos, row.m_vAngles);
			if (!placed)
			{
				LF_Debug.Error(FEATURE, string.Format("permanent placement %1 skipped: spawn failed for %2", row.m_sPlacementId, row.m_sPrefab));
				continue;
			}

			s_mRows.Set(row.m_sPlacementId, row);
			s_mLive.Set(row.m_sPlacementId, placed.GetID());
			LF_PlacedOwnerComponent.Stamp(placed, row.m_sOwnerKey);
			standing++;
		}

		LF_Debug.Info(FEATURE, string.Format("restored permanent placements: %1 of %2 row(s) standing", standing, rows.Count()));
		return standing;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PlacementSave's statics (LF_StaticReset finds it by reflection).
class LF_PlacementSave_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PlacementSave";
	}

	override void Apply()
	{
		LF_PlacementSave.ResetStatics();
	}
}
