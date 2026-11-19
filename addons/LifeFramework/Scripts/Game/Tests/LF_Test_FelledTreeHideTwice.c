// red-proof: 2026-11-19, on the PR's first HideTree (SetInteractionLayer(0)) `--only LF_Test_FelledTreeHideTwice`
// failed "a hidden tree has no collision": a static body read back its old layer 10535680 and kept colliding.
//! lumber/felled-tree-hide-twice (#3261 review): a hidden felled tree must leave the collision world, or
//! players walk into an invisible tree. On a listen host the axe action hides the server's tree and the
//! local broadcast handler finds the same entity and hides it again; whichever show runs first or last,
//! the regrown tree collides again.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FelledTreeHideTwice : LF_Test
{
	protected static const ResourceName PROP = "{61418A72BA63D059}Prefabs/Processor/Sawmill.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lumber/felled-tree-hide-twice";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		float groundY = GetGame().GetWorld().GetSurfaceY(332, 100);
		IEntity prop = LF_Utils.SpawnEntityPrefab(PROP, Vector(332, groundY, 100));
		ctx.NotNull(prop, "a static prop with physics spawned");
		if (!prop)
			return;

		Physics physics = prop.GetPhysics();
		ctx.True(physics != null, "the prop has physics");
		if (physics)
		{
			int original = physics.GetSimulationState();
			ctx.True(original != SimulationState.NONE, "the prop collides before the hide");

			int first = LF_FelledTreesReplication.HideTree(prop);
			ctx.Equal(SimulationState.NONE, physics.GetSimulationState(), "a hidden tree has no collision");
			int second = LF_FelledTreesReplication.HideTree(prop);
			ctx.Equal(original, first, "the first hide returns the real state");
			ctx.Equal(-1, second, "a second hide of a hidden tree holds nothing");
			ctx.True(LF_FelledTreesReplication.IsHidden(prop), "the prop is hidden");

			// Either order of the two shows (regrow tick, local broadcast handler) ends collidable.
			LF_FelledTreesReplication.ShowTree(prop, second);
			LF_FelledTreesReplication.ShowTree(prop, first);
			ctx.Equal(original, physics.GetSimulationState(), "the tree collides again after both shows");
			ctx.False(LF_FelledTreesReplication.IsHidden(prop), "the prop is drawn again");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(prop);
	}
}
