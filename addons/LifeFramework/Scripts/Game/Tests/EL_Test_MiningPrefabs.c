// red-proof: point the MINING_AREA constant at "{0}Prefabs/Missing.et" and
// run `tools\cli test --tier all` on the main serial thread; the "mining
// prefab loads" assertion fails. Deferred to the serial suite here because
// this workstream may not boot the test server (port collision gate).

// tier: WORLD
class EL_Test_MiningPrefabs : EL_Test
{
	protected static const ResourceName MINING_AREA = "{530D05E39B25A8C8}Prefabs/Resources/Mining/MiningArea.et";
	protected static const ResourceName MINABLE_ORE = "{F36375B16DB07ABD}Prefabs/Resources/Mining/MinableOre.et";
	protected static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	protected static const ResourceName STONE = "{88F43FF542BB6767}Prefabs/Resources/Mining/Stone.et";
	protected static const ResourceName GATHER_VFX = "{F3279D744368E450}Prefabs/Resources/Mining/GatherVFX.et";
	protected static const ResourceName WOOD_VFX = "{8561039E50CED10D}Prefabs/Resources/Mining/WoodVFX.et";
	protected static const ResourceName ORE_HIT = "{1FAA1339FE4492AF}Prefabs/Particles/OreHit.et";
	protected static const ResourceName COPPER_ORE = "{C7FC93FD15F0574C}Prefabs/Resources/Mining/CopperOre.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "mining/prefab-wiring";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		array<ResourceName> prefabs = { MINING_AREA, MINABLE_ORE, PICKAXE, STONE, GATHER_VFX, WOOD_VFX, ORE_HIT };
		foreach (ResourceName path : prefabs)
		{
			ctx.True(Resource.Load(path).IsValid(), "mining prefab loads: " + path);
		}
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;

		IEntity area = GetGame().SpawnEntityPrefab(Resource.Load(MINING_AREA), GetGame().GetWorld(), params);
		ctx.True(area != null, "mining area spawns");
		if (ctx.FailureCount() > 0)
			return;

		EL_MiningArea miningArea = EL_MiningArea.Cast(area);
		ctx.True(miningArea != null, "mining area carries EL_MiningArea");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(10, miningArea.m_iAmount, "mining area configures ten ores");
		ctx.EqualFloat(1.0, miningArea.m_fRespawnTime, 0.001, "mining area configures one minute respawn");
		ctx.EqualFloat(10.0, miningArea.m_vSize[0], 0.001, "mining area configures ten metre x extent");
		ctx.EqualFloat(10.0, miningArea.m_vSize[2], 0.001, "mining area configures ten metre z extent");

		IEntity ore = GetGame().SpawnEntityPrefab(Resource.Load(COPPER_ORE), GetGame().GetWorld(), params);
		ctx.True(ore != null, "local copper ore spawns");
		if (ctx.FailureCount() > 0)
			return;

		area.AddChild(ore, -1, EAddChildFlags.AUTO_TRANSFORM);
		ctx.True(ore.GetParent() == area, "spawned ore parents to the mining area for respawn accounting");

		IEntity minable = GetGame().SpawnEntityPrefab(Resource.Load(MINABLE_ORE), GetGame().GetWorld(), params);
		ctx.True(minable != null, "minable ore spawns");
		if (ctx.FailureCount() > 0)
			return;

		EL_DestructibleResourceComponent destructibleResource = EL_Component<EL_DestructibleResourceComponent>.Find(minable);
		ctx.True(destructibleResource != null, "minable ore carries EL_DestructibleResourceComponent");
		if (ctx.FailureCount() > 0)
			return;

		EL_DestructibleResourceComponentClass settings = EL_DestructibleResourceComponentClass.Cast(destructibleResource.GetComponentData(minable));
		ctx.True(settings != null, "minable ore destructible component has class data");
		if (ctx.FailureCount() > 0)
			return;

		bool pickaxeMapped = false;
		if (settings.m_aTools)
		{
			foreach (EL_ResourceDestructionTool tool : settings.m_aTools)
			{
				if (tool.m_rTool == PICKAXE)
				{
					pickaxeMapped = true;
					ctx.True(tool.m_fHitDamage > 0, "pickaxe maps to a positive per-hit damage");
				}
			}
		}
		ctx.True(pickaxeMapped, "minable ore tool list maps the pickaxe");
		ctx.True(Resource.Load(settings.m_rHitEffect).IsValid(), "minable ore hit effect resolves");
		ctx.Equal(2, settings.m_aDamagePhases.Count(), "minable ore configures two damage phases");

		IEntity pickaxe = GetGame().SpawnEntityPrefab(Resource.Load(PICKAXE), GetGame().GetWorld(), params);
		ctx.True(pickaxe != null, "pickaxe spawns");
		if (ctx.FailureCount() > 0)
			return;

		SCR_MeleeWeaponProperties melee = EL_Component<SCR_MeleeWeaponProperties>.Find(pickaxe);
		ctx.True(melee != null, "pickaxe carries melee weapon properties");
	}
}