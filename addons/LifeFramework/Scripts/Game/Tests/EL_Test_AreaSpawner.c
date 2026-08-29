// red-proof: expect 3 tomatoes after SpawnPrefabs instead of 5 (or point the
// assertion at an empty m_PrefabToSpawn) and run `tools\cli test --tier all`.
// Red run pending: blocked by the parallel-workstream rule (tools\cli test is
// serial-only this session); the mutation above fails on the next full-tier run.
// tier: WORLD
class EL_Test_AreaSpawner : EL_Test
{
	protected static const ResourceName TOMATO_CROP = "{28EAA2BC76483922}Prefabs/Vegetation/Crops/TomatoCrop.et";
	protected static const ResourceName TOMATO_ITEM = "{0815D91FDF997A0A}Prefabs/Items/Food/Tomato.et";

	protected static ref array<IEntity> s_aFound = {};

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crop/area-spawner";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool InsertFound(IEntity entity)
	{
		s_aFound.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool FilterTomato(IEntity entity)
	{
		if (!entity)
			return false;

		return EL_Utils.GetPrefabName(entity) == TOMATO_ITEM;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountNearbyTomatoes(vector center)
	{
		s_aFound = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere(center, 10, InsertFound, FilterTomato);
		return s_aFound.Count();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		Resource res = Resource.Load(TOMATO_CROP);
		ctx.True(res.IsValid(), "tomato crop prefab loads: " + TOMATO_CROP);
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;

		IEntity cropEntity = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(cropEntity != null, "tomato crop spawns an entity");
		if (ctx.FailureCount() > 0)
			return;

		EL_AreaSpawnerComponent spawner = EL_Component<EL_AreaSpawnerComponent>.Find(cropEntity);
		ctx.True(spawner != null, "tomato crop carries EL_AreaSpawnerComponent");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualStr(TOMATO_ITEM, spawner.GetPrefabToSpawn(), "area spawner configured to drop tomatoes");
		ctx.Equal(5, spawner.GetAmountToSpawn(), "area spawner drops 5 tomatoes per harvest");

		int before = CountNearbyTomatoes(cropEntity.GetOrigin());
		spawner.SpawnPrefabs(cropEntity);
		int after = CountNearbyTomatoes(cropEntity.GetOrigin());

		ctx.Equal(before + 5, after, string.Format("SpawnPrefabs drops 5 tomatoes (was %1, now %2)", before, after));
	}
}