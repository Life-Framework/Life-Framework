[EntityEditorProps(category: "EveronLife/Mining", description: "Mining Area", color: "251 91 0 255", dynamicBox: true)]
class LF_MiningAreaClass: GenericEntityClass
{
}

[BaseContainerProps()]
class LF_WeightedSpawn
{
	[Attribute(defvalue: "50", UIWidgets.Auto, "% Chance to spawn")]
	int m_iChance;
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Prefab", params: "et")]
	ResourceName m_Prefab;
}

class LF_MiningArea : GenericEntity
{
	[Attribute("10 10 10", desc: "Area to spawn ores in", category: "Mining Area")]
	vector m_vSize;

	[Attribute("5", UIWidgets.EditBox, desc: "Amount of ores to spawn", category: "Mining Area")]
	int m_iAmount;

	[Attribute("5", UIWidgets.EditBox, desc: "Time to respawn ores (min)", category: "Mining Area")]
	float m_fRespawnTime;

	[Attribute("", UIWidgets.Object, desc: "Prefab List", category: "Mining Area")]
	ref array<ref LF_WeightedSpawn> m_PrefabsToSpawn;

	//! The system a server switches ore spawning off with (#1075). Owned by Mining: an area only places rocks.
	static const string SYSTEM = "Mining";

	protected WorldTimestamp m_NextRespawn;
	private const int MIN_TO_MS = 60000;

	//------------------------------------------------------------------------------------------------
	//! Sum of every configured spawn chance. Zero when the list is empty or every chance is zero.
	static int GetTotalWeight(array<ref LF_WeightedSpawn> weights)
	{
		int totalWeight = 0;
		if (weights)
		{
			foreach (LF_WeightedSpawn weightedSpawnObj : weights)
			{
				if (weightedSpawnObj)
					totalWeight += weightedSpawnObj.m_iChance;
			}
		}
		return totalWeight;
	}

	//------------------------------------------------------------------------------------------------
	//! Maps one random draw in [0, totalWeight) onto a weighted entry by accumulating chances.
	//! \return The chosen index, or -1 when the list is empty or sums to zero.
	static int PickWeightedIndex(array<ref LF_WeightedSpawn> weights, int rndValue)
	{
		if (!weights || weights.Count() == 0)
			return -1;

		int totalWeight = GetTotalWeight(weights);
		if (totalWeight <= 0)
			return -1;

		int remaining = rndValue;
		for (int i = 0; i < weights.Count(); i++)
		{
			LF_WeightedSpawn weightedSpawnObj = weights[i];
			if (!weightedSpawnObj)
				continue;

			if (remaining < weightedSpawnObj.m_iChance)
				return i;

			remaining -= weightedSpawnObj.m_iChance;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! How many ores a respawn tick may spawn: the shortfall to the configured amount, never negative.
	static int ComputeSpawnCount(int amount, int existingChildren)
	{
		return LF_Utils.MaxInt(0, amount - existingChildren);
	}

	//------------------------------------------------------------------------------------------------
	//! A world point inside the configured box around the area origin, on the terrain.
	private vector GetRandomPoint()
	{
		vector origin = GetOrigin();
		float rndX = Math.RandomFloatInclusive(-m_vSize[0], m_vSize[0]);
		float rndZ = Math.RandomFloatInclusive(-m_vSize[2], m_vSize[2]);

		return origin + Vector(rndX, 0, rndZ);
	}

	//------------------------------------------------------------------------------------------------
	void SpawnOres(int amount)
	{
		if (amount <= 0)
			return;

		if (LF_Systems.Refuses(SYSTEM, "ore spawn"))
			return;

		int totalWeight = GetTotalWeight(m_PrefabsToSpawn);
		if (totalWeight <= 0)
		{
			LF_Debug.Log("Mining", "spawn skipped: no weights configured");
			return;
		}

		int spawned = 0;
		for (int i = 0; i < amount; i++)
		{
			int rndWeight = Math.RandomInt(0, totalWeight);
			int pickIndex = PickWeightedIndex(m_PrefabsToSpawn, rndWeight);
			if (pickIndex < 0)
				continue;

			LF_WeightedSpawn weightedSpawnObj = m_PrefabsToSpawn[pickIndex];
			float rndRot = Math.RandomFloat(-180, 180);
			IEntity nextOre = LF_Utils.SpawnEntityPrefab(weightedSpawnObj.m_Prefab, GetRandomPoint(), Vector(0, rndRot, 0));
			if (!nextOre)
				continue;

			vector transform[4];
			nextOre.GetWorldTransform(transform);
			if (SCR_TerrainHelper.SnapAndOrientToTerrain(transform))
				nextOre.SetOrigin(transform[3]);

			this.AddChild(nextOre, -1, EAddChildFlags.AUTO_TRANSFORM);
			if (!nextOre.GetParent())
			{
				LF_Debug.Error("Mining", string.Format("spawned ore %1 could not parent (prefab lacks Hierarchy) - deleting to avoid a stray entity", weightedSpawnObj.m_Prefab.GetPath()));
				SCR_EntityHelper.DeleteEntityAndChildren(nextOre);
				continue;
			}

			spawned++;
		}

		LF_Debug.Log("Mining", string.Format("spawned %1/%2 ores", spawned, amount));
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!Replication.IsServer())
			return;

		// Mining off: no ore is placed or replaced. Rocks already standing stay as they are (#1075).
		if (!LF_Systems.IsEnabled(SYSTEM))
			return;

		WorldTimestamp now = GetWorld().GetTimestamp();
		if (!m_NextRespawn.Greater(now))
		{
			int missing = ComputeSpawnCount(m_iAmount, SCR_EntityHelper.GetChildrenCount(this));
			SpawnOres(missing);
			m_NextRespawn = now.PlusMilliseconds(m_fRespawnTime * MIN_TO_MS);
			if (missing > 0)
				LF_Debug.Log("Mining", string.Format("respawn scheduled in %1 ms", m_fRespawnTime * MIN_TO_MS));
		}
	}

	//------------------------------------------------------------------------------------------------
	void LF_MiningArea(IEntitySource src, IEntity parent)
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