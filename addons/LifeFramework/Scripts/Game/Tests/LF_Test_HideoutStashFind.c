// red-proof: 2026-11-12 with FindOwnedStash returning null, `tools\cli test --no-build --only
// LF_Test_HideoutStashFind` failed "a placed stash is found without its entity name". Restored; green.

//! The agent harness finds a hideout stash through the placed storages, not by entity name (#323).
// tier: WORLD
class LF_Test_HideoutStashFind : LF_Test
{
	override string GetName()
	{
		return "hideout/find-stash";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity stash = LF_HideoutStashStorageManagerComponent.FindOwnedStash();
		ctx.NotNull(stash, "a placed stash is found without its entity name");
		if (!stash)
			return;

		ctx.NotNull(LF_Component<LF_HideoutStashStorageComponent>.Find(stash), "the found stash carries the stash storage");
	}
}
