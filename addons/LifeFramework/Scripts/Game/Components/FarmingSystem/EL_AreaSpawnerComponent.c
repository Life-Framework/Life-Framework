[EntityEditorProps(style: "box", category: "EveronLife/Utils", description: "Region for spawning", color: "251 91 0 255", dynamicBox: true)]
class EL_AreaSpawnerComponentClass : ScriptComponentClass
{
}

class EL_AreaSpawnerComponent : ScriptComponent
{
	[Attribute("1 1 1", desc: "Size of the spawning area", category: "Spawn Area")]
	private vector m_BoxSize;

	[Attribute("0 0 0", desc: "Offset of the spawning area", category: "Spawn Area", params: "inf inf 0 purposeCoords spaceEntity")]
	private vector m_BoxOffset;

	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Prefab to spawn", "et", category: "Spawn Area")]
	private ResourceName m_PrefabToSpawn;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Auto, desc: "Amount to spawn", category: "Spawn Area")]
	private int m_AmountToSpawn;

	[Attribute("false", UIWidgets.CheckBox, "Spawn all on init?", category: "Spawn Area")]
	private bool m_SpawnOnInit;

	//------------------------------------------------------------------------------------------------
	void SpawnPrefabs(IEntity parent)
	{
		EL_Debug.Log("Farming", string.Format("area spawner: prefab=%1 amount=%2", m_PrefabToSpawn, m_AmountToSpawn));
		for (int i = 0; i < m_AmountToSpawn; i++)
		{
			EL_Utils.SpawnEntityPrefab(m_PrefabToSpawn, GetRandomInArea(), vector.Zero, true, parent);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! EntityEditorProps size is diameter -> divide by 2
	private vector GetRandomInArea()
	{
		float rndX = Math.RandomFloatInclusive(-m_BoxSize[0], m_BoxSize[0]);
		float rndY = Math.RandomFloatInclusive(-m_BoxSize[1], m_BoxSize[1]);
		float rndZ = Math.RandomFloatInclusive(-m_BoxSize[2], m_BoxSize[2]);

		vector rndPos = Vector(rndX, rndY, rndZ);
		return GetOwner().CoordToParent(rndPos + m_BoxOffset);
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetPrefabToSpawn()
	{
		return m_PrefabToSpawn;
	}

	//------------------------------------------------------------------------------------------------
	int GetAmountToSpawn()
	{
		return m_AmountToSpawn;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);
		owner.SetFlags(EntityFlags.ACTIVE, true);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (!m_PrefabToSpawn)
			return;

		if (m_SpawnOnInit)
			SpawnPrefabs(owner);
	}
}