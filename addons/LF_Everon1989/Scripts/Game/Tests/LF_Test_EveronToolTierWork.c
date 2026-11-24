// red-proof: #4790 `--red` matches a tool by its own prefab name only (the old rule), so every tier tool deals no damage to its node, is refused by a gather row and finds no rod row, and the "works on" lines fail. Observed 2026-11-24 before the fix: the Worn pickaxe logged `IronOre takes no damage from LF_Tool_Pickaxe_Worn: not one of its tools`.
//! #4790 (island playtest round 4): the worn, standard and fine pickaxe, axe and rod (#4188) inherit the
//! base tool, and each of them works what the base works: a pickaxe tier breaks iron, coal, copper and
//! gold rock, an axe tier fells a tree, a rod tier fishes. A tool of the wrong kind still does nothing.
//! Spawns each node and tool once and deletes them (pack leg: the tier prefabs are pack resources).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronToolTierWork : LF_Test
{
	protected static const ResourceName IRON_ORE = "{2C571D6F7C05E095}Prefabs/Resources/Mining/IronOre.et";
	protected static const ResourceName COAL_ORE = "{5A986136ADB1EC2C}Prefabs/Resources/Mining/CoalOre.et";
	protected static const ResourceName COPPER_ORE = "{C7FC93FD15F0574C}Prefabs/Resources/Mining/CopperOre.et";
	protected static const ResourceName GOLD_ORE = "{3C4F54AD0CB24525}Prefabs/Resources/Mining/GoldOre.et";
	protected static const ResourceName TREE = "{388228D9B7D2CCB0}Prefabs/Resources/WoodCutting/CuttableTree_Base.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/tool-tier-work";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> pickaxes = {LF_Test_EveronToolTiers.BASE_PICKAXE, LF_Test_EveronToolTiers.PICKAXE_WORN, LF_Test_EveronToolTiers.PICKAXE_STANDARD, LF_Test_EveronToolTiers.PICKAXE_FINE};
		array<ResourceName> axes = {LF_Test_EveronToolTiers.BASE_AXE, LF_Test_EveronToolTiers.AXE_WORN, LF_Test_EveronToolTiers.AXE_STANDARD, LF_Test_EveronToolTiers.AXE_FINE};
		array<ResourceName> rods = {LF_Test_EveronToolTiers.BASE_ROD, LF_Test_EveronToolTiers.ROD_WORN, LF_Test_EveronToolTiers.ROD_STANDARD, LF_Test_EveronToolTiers.ROD_FINE};

		array<ResourceName> rocks = {IRON_ORE, COAL_ORE, COPPER_ORE, GOLD_ORE};
		foreach (ResourceName rock : rocks)
			CheckNode(ctx, rock, pickaxes, axes, rods);

		CheckNode(ctx, TREE, axes, pickaxes, rods);
		CheckGather(ctx, pickaxes, axes);
		CheckRods(ctx, rods, pickaxes);
	}

	//------------------------------------------------------------------------------------------------
	//! One node: every tool of `right` deals damage, no tool of `wrong` or `rods` does.
	protected void CheckNode(LF_TestContext ctx, ResourceName nodePrefab, array<ResourceName> right, array<ResourceName> wrong, array<ResourceName> rods)
	{
		IEntity node = LF_Utils.SpawnEntityPrefab(nodePrefab, "0 0 0");
		ctx.NotNull(node, string.Format("%1 spawns", nodePrefab));
		if (!node)
			return;

		LF_DestructibleResourceComponent component = LF_Component<LF_DestructibleResourceComponent>.Find(node);
		ctx.NotNull(component, string.Format("%1 carries the destructible resource component", nodePrefab));
		LF_DestructibleResourceComponentClass settings;
		if (component)
			settings = LF_DestructibleResourceComponentClass.Cast(component.GetComponentData(node));

		ctx.NotNull(settings, string.Format("%1 has its tool list", nodePrefab));
		if (settings)
		{
			CheckTools(ctx, nodePrefab, settings, right, true);
			CheckTools(ctx, nodePrefab, settings, wrong, false);
			CheckTools(ctx, nodePrefab, settings, rods, false);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(node);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckTools(LF_TestContext ctx, ResourceName nodePrefab, LF_DestructibleResourceComponentClass settings, array<ResourceName> tools, bool shouldWork)
	{
		foreach (ResourceName toolPrefab : tools)
		{
			IEntity tool = LF_Utils.SpawnEntityPrefab(toolPrefab, "0 0 0");
			if (!tool)
			{
				ctx.Fail(string.Format("%1 spawns as a tool", toolPrefab));
				continue;
			}

			float damage = DamageOf(settings, tool);
			if (shouldWork)
				ctx.True(damage > 0, string.Format("%1 works on %2 (%3 damage)", toolPrefab, nodePrefab, damage));
			else
				ctx.True(damage == 0, string.Format("%1 does nothing to %2 (%3 damage)", toolPrefab, nodePrefab, damage));

			SCR_EntityHelper.DeleteEntityAndChildren(tool);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The hit damage of a tool on a node. `--red` matches by the tool's own prefab name, as before #4790.
	protected float DamageOf(LF_DestructibleResourceComponentClass settings, IEntity tool)
	{
		if (!RedProof())
			return LF_DestructibleResourceHitZone.HitDamageOf(settings, tool);

		if (!settings.m_aTools)
			return 0;

		foreach (LF_ResourceDestructionTool row : settings.m_aTools)
		{
			if (row && LF_Utils.SamePrefab(row.m_rTool, LF_Utils.GetPrefabName(tool)))
				return row.m_fHitDamage;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! A tool-gated gather node (a rock, a seam) accepts every pickaxe tier and refuses an axe.
	protected void CheckGather(LF_TestContext ctx, array<ResourceName> pickaxes, array<ResourceName> axes)
	{
		LF_ResourceNodeDef row = new LF_ResourceNodeDef();
		row.m_ToolPrefab = LF_Test_EveronToolTiers.BASE_PICKAXE;
		foreach (ResourceName pickaxe : pickaxes)
		{
			ResourceName held = pickaxe;
			if (RedProof() && !LF_Utils.SamePrefab(pickaxe, row.m_ToolPrefab))
				held = ResourceName.Empty;

			ctx.True(LF_GatherTools.Accepts(row, held), string.Format("a gather that wants the pickaxe accepts %1", pickaxe));
		}

		foreach (ResourceName axe : axes)
			ctx.False(LF_GatherTools.Accepts(row, axe), string.Format("a gather that wants the pickaxe refuses %1", axe));
	}

	//------------------------------------------------------------------------------------------------
	//! Every rod tier has the fishing row of the base rod; a pickaxe has none.
	protected void CheckRods(LF_TestContext ctx, array<ResourceName> rods, array<ResourceName> pickaxes)
	{
		LF_FishingConfig config = LF_FishingConfig.Get();
		ctx.NotNull(config, "the fishing config loads");
		if (!config)
			return;

		LF_FishingRodDef baseRow = config.FindRod(LF_Test_EveronToolTiers.BASE_ROD);
		ctx.NotNull(baseRow, "the base rod has a fishing row");
		foreach (ResourceName rod : rods)
		{
			ResourceName probe = rod;
			if (RedProof() && !LF_Utils.SamePrefab(rod, LF_Test_EveronToolTiers.BASE_ROD))
				probe = ResourceName.Empty;

			LF_FishingRodDef def = config.FindRod(probe);
			ctx.NotNull(def, string.Format("%1 fishes", rod));
			if (def && baseRow)
				ctx.True(def == baseRow, string.Format("%1 fishes as the base rod", rod));
		}

		foreach (ResourceName pickaxe : pickaxes)
			ctx.True(config.FindRod(pickaxe) == null, string.Format("%1 is no fishing rod", pickaxe));
	}
}
