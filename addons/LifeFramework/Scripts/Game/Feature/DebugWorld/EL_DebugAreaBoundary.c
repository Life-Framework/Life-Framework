// EL_DebugAreaBoundary.c - reusable debug-world area boundary.
//
// A GenericEntity that marks a rectangular feature area in the DebugWorld:
//   - draws the same Workbench editor box as EL_MiningArea (dynamicBox +
//     _WB_GetBoundBox), so each area is visible while authoring;
//   - spawns one corner pole at each of the four box corners on the server,
//     so the area is visible in a running game too.
// Placed once per feature/area with m_vSize set to enclose all of that
// feature's prefabs; walkways are the space between adjacent boxes.

[EntityEditorProps(category: "EveronLife/DebugWorld", description: "Debug Area Boundary", color: "0 255 0 255", dynamicBox: true)]
class EL_DebugAreaBoundaryClass: GenericEntityClass
{
}

class EL_DebugAreaBoundary : GenericEntity
{
	//! Half-extents of the box around the entity origin (matches EL_MiningArea).
	[Attribute("10 1 10", desc: "Box half-extents X Y Z (the area this boundary encloses)", category: "Debug Area Boundary")]
	vector m_vSize;

	[Attribute("{305528B8998E0834}Prefabs/Debug/EL_DebugBoundaryPole.et", UIWidgets.ResourcePickerThumbnail, "Corner pole prefab", params: "et", category: "Debug Area Boundary")]
	ResourceName m_PolePrefab;

	protected bool m_bPolesSpawned;

	//------------------------------------------------------------------------------------------------
	//! The four world-space corners of the box: (-x,-z) (-x,z) (x,-z) (x,z) around the origin.
	//! Pure static so LOGIC-tier tests can assert the geometry without a world.
	static void GetCornerPositions(vector origin, vector size, out vector corners[4])
	{
		float x = size[0];
		float z = size[2];
		corners[0] = origin + Vector(-x, 0, -z);
		corners[1] = origin + Vector(-x, 0, z);
		corners[2] = origin + Vector(x, 0, -z);
		corners[3] = origin + Vector(x, 0, z);
	}

	//------------------------------------------------------------------------------------------------
	private void SpawnCornerPoles()
	{
		if (m_PolePrefab.IsEmpty())
		{
			EL_Debug.Error("DebugWorld", "boundary spawn skipped: no pole prefab configured");
			return;
		}

		if (m_vSize[0] <= 0 || m_vSize[2] <= 0)
		{
			EL_Debug.Error("DebugWorld", string.Format("boundary %1 skipped: invalid size %2", GetName(), m_vSize));
			return;
		}

		vector corners[4];
		GetCornerPositions(GetOrigin(), m_vSize, corners);

		int spawned = 0;
		for (int i = 0; i < 4; i++)
		{
			vector poleTransform[4];
			poleTransform[3] = corners[i];
			if (SCR_TerrainHelper.SnapAndOrientToTerrain(poleTransform))
				corners[i] = poleTransform[3];

			IEntity pole = EL_Utils.SpawnEntityPrefab(m_PolePrefab, corners[i], Vector(0, 0, 0));
			if (!pole)
			{
				EL_Debug.Error("DebugWorld", string.Format("boundary %1 corner %2 pole spawn failed", GetName(), i));
				continue;
			}

			this.AddChild(pole, -1, EAddChildFlags.AUTO_TRANSFORM);
			if (!pole.GetParent())
			{
				EL_Debug.Error("DebugWorld", string.Format("boundary %1 corner %2 pole could not parent - deleting to avoid a stray", GetName(), i));
				SCR_EntityHelper.DeleteEntityAndChildren(pole);
				continue;
			}

			spawned++;
		}

		EL_Debug.Log("DebugWorld", string.Format("boundary %1 spawned %2/4 corner poles (size %3)", GetName(), spawned, m_vSize));
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!Replication.IsServer())
			return;

		if (!m_bPolesSpawned)
		{
			SpawnCornerPoles();
			m_bPolesSpawned = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	void EL_DebugAreaBoundary(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT | EntityEvent.FRAME);
		SetFlags(EntityFlags.ACTIVE, true);
	}

	#ifdef WORKBENCH
	//------------------------------------------------------------------------------------------------
	override void _WB_GetBoundBox(inout vector min, inout vector max, IEntitySource src)
	{
		min = -m_vSize;
		max = m_vSize;
	}
	#endif
};