class LF_FixtureSpawner
{
	IEntity Spawn(ResourceName prefab)
	{
		// GetGame().SpawnEntityPrefab(Resource.Load(prefab)) in a comment is not code.
		return LF_Utils.SpawnEntityPrefab(prefab, vector.Zero);
	}
}
