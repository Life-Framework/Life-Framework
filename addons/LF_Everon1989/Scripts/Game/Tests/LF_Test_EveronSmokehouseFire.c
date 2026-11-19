// red-proof: RedProof() skips the spawn, so there is no fire to be lit and both checks below fail.
//! everon/smokehouse-fire (#3592): the dock smokehouse's fire is a vanilla fireplace that burns from the moment it
//! loads, so flame, light and the campfire crackle (all the base game's own) reach a player who walks up without
//! lighting anything. (The smoke plume is a particle child of the smoker prefab; a dedicated server builds no
//! particle entities, so that one is proved by the `look` picture, not here.)
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronSmokehouseFire : LF_Test
{
	protected static const ResourceName FIRE = "{34BA147AE88D0D24}Prefabs/Props/Fishing/LF_SmokehouseFire.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/smokehouse-fire";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity fire;
		if (!RedProof())
			fire = LF_Utils.SpawnEntityPrefab(FIRE, Vector(0, 500, 0));

		ctx.NotNull(fire, "the smokehouse fire prefab spawns");
		if (!fire)
			return;

		SCR_FireplaceComponent fireplace = SCR_FireplaceComponent.Cast(fire.FindComponent(SCR_FireplaceComponent));
		ctx.NotNull(fireplace, "the fire is a fireplace");
		if (fireplace)
			ctx.True(fireplace.IsOn(), "the fire is lit as soon as it exists");

		SCR_EntityHelper.DeleteEntityAndChildren(fire);
	}
}
