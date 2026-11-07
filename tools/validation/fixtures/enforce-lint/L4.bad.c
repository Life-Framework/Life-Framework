class LF_FixtureRegistryComponent : ScriptComponent
{
	protected static ref array<LF_FixtureRegistryComponent> s_aAll = {};

	override void OnPostInit(IEntity owner)
	{
		s_aAll.Insert(this);
	}
}
