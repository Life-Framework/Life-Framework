class LF_FixtureRegistryComponent : ScriptComponent
{
	protected static ref array<LF_FixtureRegistryComponent> s_aAll = {};
	protected static ref map<string, EntityID> s_mIds;

	override void OnPostInit(IEntity owner)
	{
		s_aAll.Insert(this);
	}

	override void OnDelete(IEntity owner)
	{
		s_aAll.RemoveItem(this);
	}
}
