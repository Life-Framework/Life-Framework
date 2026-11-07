// red-proof: PENDING. Not yet observed. Planned perturbation: make
// EL_DestructibleResourceHitZone.ComputeEffectiveDamage return 0.0 and run
// `tools\cli test --no-build --tier all`; "axe damage lowers the tree hit zone
// health" must fail. Record the observed line here before this PR leaves draft.

// tier: WORLD
//! Logging chain wiring (logging/chain-wiring) proves the axe is listed on the
//! tree. This test proves the damage path: a melee context built the way
//! SCR_MeleeComponent builds it (damageSource = the swung tool) lowers the tree
//! hit zone health for the axe and leaves it untouched for a wrong tool.
class EL_Test_WoodcuttingDamage : EL_Test
{
	protected static const ResourceName TREE = "{391791B1CAE33AFE}Prefabs/Resources/WoodCutting/Pinus_Sylvestris/CuttableTree_Pinus_Sylvestris_2f.et";
	protected static const ResourceName AXE = "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et";
	protected static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "logging/axe-damage";
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_DamageContext CreateMeleeContext(IEntity target, HitZone hitZone, IEntity tool)
	{
		vector hitPosDirNorm[3];
		hitPosDirNorm[0] = target.GetOrigin();
		hitPosDirNorm[1] = vector.Forward;
		hitPosDirNorm[2] = vector.Up;

		SCR_DamageContext context = new SCR_DamageContext(EDamageType.MELEE, 20.0, hitPosDirNorm,
			target, hitZone, Instigator.CreateInstigator(tool), null, -1, -1);
		context.damageSource = tool;
		return context;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;

		IEntity tree = GetGame().SpawnEntityPrefab(Resource.Load(TREE), GetGame().GetWorld(), params);
		IEntity axe = GetGame().SpawnEntityPrefab(Resource.Load(AXE), GetGame().GetWorld(), params);
		IEntity pickaxe = GetGame().SpawnEntityPrefab(Resource.Load(PICKAXE), GetGame().GetWorld(), params);
		ctx.NotNull(tree, "cuttable tree spawns");
		ctx.NotNull(axe, "axe spawns");
		ctx.NotNull(pickaxe, "pickaxe spawns");
		if (ctx.FailureCount() > 0)
			return;

		EL_DestructibleResourceComponent treeResource = EL_Component<EL_DestructibleResourceComponent>.Find(tree);
		ctx.NotNull(treeResource, "tree carries EL_DestructibleResourceComponent");
		if (ctx.FailureCount() > 0)
			return;

		HitZone treeZone = treeResource.GetDefaultHitZone();
		ctx.NotNull(treeZone, "tree exposes a default hit zone");
		if (ctx.FailureCount() > 0)
			return;

		SCR_DamageContext wrongTool = CreateMeleeContext(tree, treeZone, pickaxe);
		ctx.True(treeZone.ComputeEffectiveDamage(wrongTool, false) <= 0.0, "a pickaxe deals no damage to the tree");

		float healthBefore = treeZone.GetHealth();
		treeResource.HandleDamage(wrongTool);
		ctx.True(treeZone.GetHealth() == healthBefore, "a pickaxe hit leaves the tree health unchanged");

		SCR_DamageContext axeHit = CreateMeleeContext(tree, treeZone, axe);
		ctx.True(treeZone.ComputeEffectiveDamage(axeHit, false) > 0.0, "the axe maps to positive damage on the tree");

		treeResource.HandleDamage(axeHit);
		float healthAfter = treeZone.GetHealth();
		ctx.True(healthAfter < healthBefore, string.Format("axe damage lowers the tree hit zone health (%1 -> %2)", healthBefore, healthAfter));
	}
}
