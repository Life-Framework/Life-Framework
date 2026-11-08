class LF_FixtureSpawner
{
	IEntity Spawn(ResourceName prefab)
	{
		return GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), null);
	}
}
