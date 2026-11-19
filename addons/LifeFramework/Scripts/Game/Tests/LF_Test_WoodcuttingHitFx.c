// red-proof: 2026-11-19, before #3372 the hit effect lived inline in the hit zone behind
// System.IsConsoleApp(), so LF_DestructibleResourceComponent had no SpawnHitFx and this test
// did not compile; `--red` skips the SpawnHitFx act and the "configured hit effect spawns" check fails.

//! An axe hit on a cuttable tree reaches its hit effect through LF_DestructibleResourceComponent:
//! the server broadcasts it (RpcDo_HitFx) so a dedicated server's clients see chips (#3372).
//! A test server is a console app: this proves the spawn the client RPC runs and that the hit
//! zone's damage path (HandleDamage -> OnDamage -> OnToolHit) takes the broadcast branch without error.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_WoodcuttingHitFx : LF_Test
{
	protected static const ResourceName TREE = "{391791B1CAE33AFE}Prefabs/Resources/WoodCutting/Pinus_Sylvestris/CuttableTree_Pinus_Sylvestris_2f.et";
	protected static const ResourceName AXE = "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "logging/axe-hit-fx";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity tree = LF_Utils.SpawnEntityPrefab(TREE, vector.Zero);
		IEntity axe = LF_Utils.SpawnEntityPrefab(AXE, vector.Zero);

		RunChecks(ctx, tree, axe);

		if (tree)
			SCR_EntityHelper.DeleteEntityAndChildren(tree);
		if (axe)
			SCR_EntityHelper.DeleteEntityAndChildren(axe);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChecks(LF_TestContext ctx, IEntity tree, IEntity axe)
	{
		ctx.NotNull(tree, "cuttable tree spawns");
		ctx.NotNull(axe, "axe spawns");
		if (ctx.FailureCount() > 0)
			return;

		LF_DestructibleResourceComponent resource = LF_Component<LF_DestructibleResourceComponent>.Find(tree);
		ctx.NotNull(resource, "tree carries LF_DestructibleResourceComponent");
		if (ctx.FailureCount() > 0)
			return;

		bool spawned = false;
		if (!RedProof())
			spawned = resource.SpawnHitFx(tree.GetOrigin(), vector.Up);
		ctx.True(spawned, "the tree's configured hit effect spawns at the hit point");

		HitZone zone = resource.GetDefaultHitZone();
		ctx.NotNull(zone, "tree exposes a default hit zone");
		if (ctx.FailureCount() > 0)
			return;

		vector hitPosDirNorm[3];
		hitPosDirNorm[0] = tree.GetOrigin();
		hitPosDirNorm[1] = vector.Forward;
		hitPosDirNorm[2] = vector.Up;
		SCR_DamageContext hit = new SCR_DamageContext(EDamageType.MELEE, 20.0, hitPosDirNorm,
			tree, zone, Instigator.CreateInstigator(axe), null, -1, -1);
		hit.damageSource = axe;

		float before = zone.GetHealth();
		resource.HandleDamage(hit);
		ctx.True(zone.GetHealth() < before, "an axe hit still lowers the tree health after the broadcast path");
	}
}
