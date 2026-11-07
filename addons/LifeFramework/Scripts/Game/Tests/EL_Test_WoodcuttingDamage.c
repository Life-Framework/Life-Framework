// red-proof: this probe reproduces the reported gameplay failure (axe swing
// deals no damage to a cuttable tree). The "axe melee damage applies to the
// tree hit zone" assertion fails on the current tree prefab because the tool
// match / damage pipeline drops the hit. Proven red by the first run, then
// the fix makes it green.

// tier: WORLD
class EL_Test_WoodcuttingDamage : EL_Test
{
	protected static const ResourceName TREE = "{391791B1CAE33AFE}Prefabs/Resources/WoodCutting/Pinus_Sylvestris/CuttableTree_Pinus_Sylvestris_2f.et";
	protected static const ResourceName AXE = "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et";
	protected static const ResourceName ORE = "{F36375B16DB07ABD}Prefabs/Resources/Mining/MinableOre.et";
	protected static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	protected static const ResourceName CHARACTER = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "woodcutting/damage-pipeline";
	}

	//------------------------------------------------------------------------------------------------
	//! Builds a melee damage context exactly like SCR_MeleeComponent.HandleDamageDelayed
	//! for a non-SCR_DestructibleEntity target: damageSource = the swung weapon.
	protected SCR_DamageContext CreateMeleeContext(IEntity target, HitZone hitZone, IEntity weapon)
	{
		vector hitPosDirNorm[3];
		hitPosDirNorm[0] = target.GetOrigin();
		hitPosDirNorm[1] = vector.Forward;
		hitPosDirNorm[2] = vector.Up;

		SCR_DamageContext context = new SCR_DamageContext(EDamageType.MELEE, 20.0, hitPosDirNorm,
			target, hitZone, Instigator.CreateInstigator(weapon), null, -1, -1);
		context.damageSource = weapon;
		return context;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.LOCAL;

		// --- tree + axe -------------------------------------------------------------
		IEntity tree = GetGame().SpawnEntityPrefab(Resource.Load(TREE), GetGame().GetWorld(), spawnParams);
		ctx.True(tree != null, "cuttable tree spawns");
		if (ctx.FailureCount() > 0)
			return;

		EL_DestructibleResourceComponent treeResource = EL_Component<EL_DestructibleResourceComponent>.Find(tree);
		ctx.True(treeResource != null, "tree carries EL_DestructibleResourceComponent");
		if (ctx.FailureCount() > 0)
			return;

		HitZone treeZone = treeResource.GetDefaultHitZone();
		ctx.True(treeZone != null, "tree exposes a default hit zone");
		if (ctx.FailureCount() > 0)
			return;

		IEntity axe = GetGame().SpawnEntityPrefab(Resource.Load(AXE), GetGame().GetWorld(), spawnParams);
		ctx.True(axe != null, "axe spawns");
		if (ctx.FailureCount() > 0)
			return;

		SCR_DamageContext treeCtx = CreateMeleeContext(tree, treeZone, axe);
		float toolDamage = treeZone.ComputeEffectiveDamage(treeCtx, false);
		ctx.True(toolDamage > 0.0, "axe tool match yields positive damage on tree");

		float initialTreeHealth = treeZone.GetHealth();
		treeResource.HandleDamage(treeCtx);
		float afterTreeHealth = treeZone.GetHealth();
		ctx.True(afterTreeHealth < initialTreeHealth, string.Format("axe melee damage applies to tree hit zone (%1 -> %2)", initialTreeHealth, afterTreeHealth));

		// --- ore + pickaxe (shared pipeline control) -------------------------------
		IEntity ore = GetGame().SpawnEntityPrefab(Resource.Load(ORE), GetGame().GetWorld(), spawnParams);
		ctx.True(ore != null, "minable ore spawns");
		if (ctx.FailureCount() > 0)
			return;

		EL_DestructibleResourceComponent oreResource = EL_Component<EL_DestructibleResourceComponent>.Find(ore);
		ctx.True(oreResource != null, "ore carries EL_DestructibleResourceComponent");
		if (ctx.FailureCount() > 0)
			return;

		HitZone oreZone = oreResource.GetDefaultHitZone();
		ctx.True(oreZone != null, "ore exposes a default hit zone");
		if (ctx.FailureCount() > 0)
			return;

		IEntity pickaxe = GetGame().SpawnEntityPrefab(Resource.Load(PICKAXE), GetGame().GetWorld(), spawnParams);
		ctx.True(pickaxe != null, "pickaxe spawns");
		if (ctx.FailureCount() > 0)
			return;

		SCR_DamageContext oreCtx = CreateMeleeContext(ore, oreZone, pickaxe);
		float oreToolDamage = oreZone.ComputeEffectiveDamage(oreCtx, false);
		ctx.True(oreToolDamage > 0.0, "pickaxe tool match yields positive damage on ore");

		float initialOreHealth = oreZone.GetHealth();
		oreResource.HandleDamage(oreCtx);
		float afterOreHealth = oreZone.GetHealth();
		ctx.True(afterOreHealth < initialOreHealth, string.Format("pickaxe melee damage applies to ore hit zone (%1 -> %2)", initialOreHealth, afterOreHealth));

		// --- wrong tool negative control ------------------------------------------
		SCR_DamageContext wrongToolCtx = CreateMeleeContext(tree, treeZone, pickaxe);
		float wrongToolDamage = treeZone.ComputeEffectiveDamage(wrongToolCtx, false);
		ctx.True(wrongToolDamage <= 0.0, "wrong tool yields no damage on tree");

		// --- player character can perform melee? -----------------------------------
		IEntity character = GetGame().SpawnEntityPrefab(Resource.Load(CHARACTER), GetGame().GetWorld(), spawnParams);
		ctx.True(character != null, "player character spawns");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(character.FindComponent(CharacterControllerComponent) != null, "player character carries CharacterControllerComponent");
		ctx.True(character.FindComponent(CharacterWeaponManagerComponent) != null, "player character carries weapon manager");
		ctx.True(character.FindComponent(SCR_MeleeComponent) != null, "player character carries SCR_MeleeComponent");

		// --- melee trace replica of SCR_MeleeComponent.CheckCollisionsSimple ---------
		// A Projectile-layer TraceSphere must strike the tree, or the axe swing never
		// reaches the damage manager no matter how well the pipeline is wired.
		EntitySpawnParams traceSpawn();
		traceSpawn.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(traceSpawn.Transform);
		traceSpawn.Transform[3] = "1100 10 1100";
		IEntity traceTree = GetGame().SpawnEntityPrefab(Resource.Load(TREE), GetGame().GetWorld(), traceSpawn);
		ctx.True(traceTree != null, "trace tree spawns at known position");
		if (ctx.FailureCount() > 0)
			return;

		TraceSphere trace = new TraceSphere();
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		trace.Radius = 0.5;
		trace.Start = "1100 9 1100";
		trace.End = "1100 12 1100";
		array<IEntity> exclude = {};
		trace.ExcludeArray = exclude;
		GetGame().GetWorld().TraceMove(trace, null);
		ctx.True(trace.TraceEnt != null, "projectile-layer melee trace strikes something at the tree");
		ctx.True(trace.TraceEnt == traceTree, "projectile-layer melee trace strikes the tree entity itself");
	}
}