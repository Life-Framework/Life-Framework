// red-proof: observed 2026-11-17 (bug fix: the test run with the fix reverted failed) on the unfixed LF_HouseManagerComponent (no INIT mask): "the house has an identifier after init". Green with the fix.
// tier: WORLD
//! houses/identifier-init (review 2026-11-17): a spawned building with LF_HouseManagerComponent gets a
//! house identifier at init. The component never asked for EOnInit (no SetEventMask), so the identifier
//! stayed empty, SyncDoors never ran, and no key could ever match the building's doors.
class LF_Test_HouseIdentifierInit : LF_Test
{
	protected static const ResourceName BUILDING = "{74676598DF38F201}Prefabs/Structures/Core/EL_Building_Base.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "houses/identifier-init";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource res = Resource.Load(BUILDING);
		ctx.True(res && res.IsValid(), "the building prefab loads");
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity building = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(building != null, "the building spawns");
		if (!building)
			return;

		LF_HouseManagerComponent manager = LF_Component<LF_HouseManagerComponent>.Find(building);
		ctx.NotNull(manager, "the building carries LF_HouseManagerComponent");
		if (manager)
			ctx.False(manager.GetHouseIdentifier().IsEmpty(), "the house has an identifier after init");

		SCR_EntityHelper.DeleteEntityAndChildren(building);
	}
}
