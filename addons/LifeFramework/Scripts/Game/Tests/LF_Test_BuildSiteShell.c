// red-proof: observed 2026-11-18 (#2807 review) `--only LF_Test_BuildSiteShell` on the old config failed `stage 2's shell carries no actions: {81CB1B2631584427}Prefabs/Props/Construction/CementPile.et`; `--red` printed `RED-OK property/build-site-shell (fails with the hook on, 20 assertions)` (the hook keeps the old server-only spawn, so the DirtPile never reached clients); `--only` then passed.
//! property/build-site-shell (#2807 review of #2650): every stage shell of the example tier is scenery
//! that reaches clients. A shell prefab carries no ActionsManagerComponent (the stage 2 CementPile was
//! a cement gather node at every plot). A plain prop (stage 1's DirtPile, no RplComponent) is spawned
//! by clients themselves, since the server's copy never replicates; a replicated one (the stage 4
//! house) is spawned by the server alone, so a client does not add a second copy.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BuildSiteShell : LF_Test
{
	override string GetName()
	{
		return "property/build-site-shell";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PropertyConfig.Reload();
		string tier = LF_PropertyConfig.PlotTierId("plot-a");
		int count = LF_PropertyConfig.ShellPrefabCount(tier);
		ctx.True(count >= 4, "the example tier has a shell per stage");
		if (count < 4)
			return;

		for (int stage = 1; stage <= count; stage++)
		{
			ResourceName prefab = LF_PropertyConfig.ShellPrefab(tier, stage);
			Resource resource = Resource.Load(prefab);
			ctx.True(resource && resource.IsValid(), string.Format("stage %1's shell prefab loads: %2", stage, prefab));
			if (!resource || !resource.IsValid())
				continue;

			bool hasActions = SCR_BaseContainerTools.FindComponentSource(resource, ActionsManagerComponent) != null;
			ctx.False(hasActions, string.Format("stage %1's shell carries no actions: %2", stage, prefab));

			bool replicated = LF_BuildSiteComponent.IsReplicatedPrefab(prefab);
			ctx.True(LF_BuildSiteComponent.SpawnsShellHere(true, replicated), string.Format("the server spawns stage %1's shell", stage));

			// The hook stands for the old rule: only the server ever spawned a shell.
			bool clientSpawns = !RedProof() && LF_BuildSiteComponent.SpawnsShellHere(false, replicated);
			ctx.True(clientSpawns != replicated, string.Format("a client spawns stage %1's shell only when it does not replicate: %2", stage, prefab));
		}

		ctx.False(LF_BuildSiteComponent.IsReplicatedPrefab(LF_PropertyConfig.ShellPrefab(tier, 1)), "stage 1's dirt pile has no RplComponent");
		ctx.True(LF_BuildSiteComponent.IsReplicatedPrefab(LF_PropertyConfig.ShellPrefab(tier, 4)), "stage 4's house has an RplComponent");
	}
}
