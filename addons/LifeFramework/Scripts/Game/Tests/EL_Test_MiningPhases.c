// red-proof: 2026-10-11 fast boot logs_2026-10-11_15-03-47 printed [ELTEST] FAIL mining/phases / [ELTEST]   - one tool hit enters the first phase (expected 0, got 1)

// tier: LOGIC
class EL_Test_MiningPhases : EL_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "mining/phases";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_OreNode Ore(array<ref LF_OrePhase> phases, ResourceName intact)
	{
		LF_OreNode node = new LF_OreNode();
		node.m_aPhases = phases;
		node.m_fOriginalHealth = LF_MiningPhases.OriginalHealth(40, phases);
		node.m_fHealth = node.m_fOriginalHealth;
		node.m_OriginalModel = intact;
		node.m_Model = intact;
		node.m_fEffectiveDamage = 20;
		node.m_bDeleteAfterFinal = true;
		node.m_iPhase = 0;
		return node;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		ResourceName intact = "{6399C4DB701BF230}Assets/Resources/Mining/MetalOre.xob";
		ResourceName chipped = "{A1B2C3D4E5F60718}Assets/Resources/Mining/MetalOreChipped.xob";
		ResourceName rubble = "{B1B2C3D4E5F60718}Assets/Resources/Mining/MetalOreRubble.xob";

		ref array<ref LF_OrePhase> phases = new array<ref LF_OrePhase>();
		LF_OrePhase first = new LF_OrePhase();
		first.m_fHealth = 40;
		first.m_Model = chipped;
		phases.Insert(first);
		LF_OrePhase second = new LF_OrePhase();
		second.m_fHealth = 40;
		second.m_Model = rubble;
		phases.Insert(second);

		ctx.EqualFloat(120, LF_MiningPhases.OriginalHealth(40, phases), 0.001, "intact ore health is the base plus both phase budgets");
		ctx.Equal(1, LF_MiningPhases.PhaseAfterDamage(0, phases, 20, true), "one tool hit enters the first phase");
		ctx.Equal(1, LF_MiningPhases.PhaseAfterDamage(0, phases, 0, true), "zero damage still enters the phase walk");
		ctx.Equal(0, LF_MiningPhases.PhaseAfterDamage(0, phases, -1, true), "negative damage stays on the current phase");
		ctx.Equal(2, LF_MiningPhases.PhaseAfterDamage(0, phases, 40, true), "damage that spends the first budget lands on the second phase");
		ctx.Equal(3, LF_MiningPhases.PhaseAfterDamage(0, phases, 80, true), "damage past the last budget destroys the ore");
		ctx.Equal(2, LF_MiningPhases.PhaseAfterDamage(0, phases, 80, false), "without delete-after-final the walk stops on the last phase");
		ctx.Equal(2, LF_MiningPhases.PhaseAfterDamage(1, phases, 20, true), "a hit from the first phase enters the second");

		ResourceName model = intact;
		bool swapMesh = false;
		float health = LF_MiningPhases.ApplyPhase(0, 120, intact, phases, chipped, model, swapMesh);
		ctx.EqualFloat(120, health, 0.001, "no phase data restores the original health");
		ctx.True(swapMesh, "no phase data restores the mesh");
		ctx.True(model == intact, "restored mesh is the original ore");

		health = LF_MiningPhases.ApplyPhase(1, 120, intact, phases, intact, model, swapMesh);
		ctx.EqualFloat(40, health, 0.001, "the first phase sets health to its budget");
		ctx.True(swapMesh, "the first phase swaps the mesh");
		ctx.True(model == chipped, "the first phase mesh is the chipped ore");

		health = LF_MiningPhases.ApplyPhase(2, 120, intact, phases, chipped, model, swapMesh);
		ctx.EqualFloat(40, health, 0.001, "the second phase sets health to its budget");
		ctx.True(model == rubble, "the second phase mesh is the rubble ore");

		first.m_Model = ResourceName.Empty;
		health = LF_MiningPhases.ApplyPhase(1, 120, intact, phases, chipped, model, swapMesh);
		ctx.EqualFloat(40, health, 0.001, "an empty phase mesh still sets the phase health");
		ctx.False(swapMesh, "an empty phase mesh does not swap");
		ctx.True(model == chipped, "an empty phase mesh keeps the current mesh");
		first.m_Model = chipped;

		ctx.EqualFloat(20, LF_MiningPhases.ToolDamage(20, 20), 0.001, "matching tool damage is accepted");
		ctx.EqualFloat(0, LF_MiningPhases.ToolDamage(10, 20), 0.001, "any other tool damage is zero");

		LF_OreNode node = Ore(phases, intact);
		LF_MiningPhases.Strike(node, 10);
		ctx.Equal(0, node.m_iPhase, "a mismatched tool leaves the phase");
		ctx.False(node.m_bSwapMesh, "a mismatched tool does not swap the mesh");
		ctx.EqualFloat(120, node.m_fHealth, 0.001, "a mismatched tool leaves the health");

		LF_MiningPhases.Strike(node, 20);
		ctx.Equal(1, node.m_iPhase, "an accepted hit enters the first phase");
		ctx.True(node.m_bSwapMesh, "an accepted hit swaps the mesh");
		ctx.True(node.m_Model == chipped, "an accepted hit shows the chipped mesh");
		ctx.EqualFloat(40, node.m_fHealth, 0.001, "an accepted hit sets the first phase health");
		ctx.False(node.m_bDestroyed, "the first phase is still ore");

		LF_MiningPhases.Strike(node, 20);
		ctx.Equal(2, node.m_iPhase, "the next hit enters the second phase");
		ctx.True(node.m_Model == rubble, "the second hit shows the rubble mesh");

		LF_MiningPhases.Strike(node, 20);
		ctx.True(node.m_bDestroyed, "the hit past the last phase destroys the ore");
		ctx.EqualFloat(0, node.m_fHealth, 0.001, "a destroyed ore has no health");
		ctx.Equal(3, node.m_iPhase, "destruction is the index past the last phase");

		LF_MiningPhases.Strike(node, 20);
		ctx.Equal(3, node.m_iPhase, "a destroyed ore ignores further hits");

		ref array<ref LF_OreWeight> empty = new array<ref LF_OreWeight>();
		ctx.Equal(0, LF_MiningSpawn.TotalWeight(empty), "empty weight list sums to zero");
		ctx.Equal(-1, LF_MiningSpawn.PickIndex(empty, 0), "empty weight list picks nothing");
		ref array<ref LF_OreWeight> nullList = null;
		ctx.Equal(-1, LF_MiningSpawn.PickIndex(nullList, 0), "null weight list picks nothing");

		ref array<ref LF_OreWeight> weights = new array<ref LF_OreWeight>();
		LF_OreWeight copper = new LF_OreWeight();
		copper.m_iChance = 50;
		weights.Insert(copper);
		LF_OreWeight iron = new LF_OreWeight();
		iron.m_iChance = 30;
		weights.Insert(iron);
		LF_OreWeight gold = new LF_OreWeight();
		gold.m_iChance = 20;
		weights.Insert(gold);
		ctx.Equal(100, LF_MiningSpawn.TotalWeight(weights), "ore chances accumulate");
		ctx.Equal(0, LF_MiningSpawn.PickIndex(weights, 0), "roll 0 maps to the first ore");
		ctx.Equal(0, LF_MiningSpawn.PickIndex(weights, 49), "roll 49 maps to the first ore");
		ctx.Equal(1, LF_MiningSpawn.PickIndex(weights, 50), "roll 50 maps to the second ore");
		ctx.Equal(2, LF_MiningSpawn.PickIndex(weights, 80), "roll 80 maps to the third ore");
		ctx.Equal(-1, LF_MiningSpawn.PickIndex(weights, 100), "a roll outside the total picks nothing");
		ctx.Equal(7, LF_MiningSpawn.SpawnCount(10, 3), "spawn count is the shortfall");
		ctx.Equal(0, LF_MiningSpawn.SpawnCount(10, 12), "an over-full area spawns nothing");

		ctx.Equal(5000, LF_MiningForge.DelayMs(5), "five seconds is a 5000 ms forge delay");
		ctx.Equal(0, LF_MiningForge.DelayMs(0), "zero seconds waits 0 ms");
		ctx.Equal(0, LF_MiningForge.DelayMs(-2), "a negative delay waits 0 ms");

		LF_ForgeHeat cold = new LF_ForgeHeat();
		cold.m_iNuggets = 1;
		ctx.False(LF_MiningForge.Start(cold), "a forge with no recipe does not start");
		ctx.Equal(0, cold.m_iBars, "a rejected forge yields no bar");

		LF_ForgeHeat heat = new LF_ForgeHeat();
		LF_MiningForge.UseIronRecipe(heat);
		ctx.True(heat.m_Nugget == LF_MiningForge.IRON_NUGGET, "the forge consumes an iron nugget");
		ctx.True(heat.m_Bar == LF_MiningForge.IRON_BAR, "the forge yields an iron bar");
		ctx.Equal(1, heat.m_iNuggetsRequired, "the forge takes one nugget");
		ctx.Equal(1, heat.m_iBarsYielded, "the forge yields one bar");
		ctx.Equal(5000, heat.m_iDurationMs, "the iron forge waits 5000 ms");

		ctx.False(LF_MiningForge.Start(heat), "the forge rejects an empty hand");
		ctx.Equal(0, heat.m_iBars, "a missing nugget yields no bar");

		heat.m_iNuggets = 1;
		ctx.True(LF_MiningForge.Start(heat), "one nugget starts the forge");
		ctx.True(heat.m_bHeating, "the forge is heating before the delay");
		ctx.False(LF_MiningForge.Start(heat), "a second start is rejected while heating");
		ctx.False(LF_MiningForge.Advance(heat, 4999), "4999 ms does not finish the bar");
		ctx.Equal(1, heat.m_iNuggets, "the nugget is still held before the delay");
		ctx.Equal(0, heat.m_iBars, "no bar exists before the delay");
		ctx.True(LF_MiningForge.Advance(heat, 1), "5000 ms turns the nugget into a bar");
		ctx.Equal(0, heat.m_iNuggets, "the finished forge consumed the nugget");
		ctx.Equal(1, heat.m_iBars, "the finished forge produced one bar");
		ctx.False(heat.m_bHeating, "the forge is idle after the bar");
		ctx.False(LF_MiningForge.Advance(heat, 5000), "an idle forge does not produce another bar");
		ctx.Equal(1, heat.m_iBars, "the bar count stays at one");

		heat.m_iNuggets = 1;
		ctx.True(LF_MiningForge.Start(heat), "a later nugget can start another heat");
		heat.m_iNuggets = 0;
		ctx.False(LF_MiningForge.Advance(heat, 5000), "a nugget removed before the end yields no bar");
		ctx.Equal(1, heat.m_iBars, "the failed heat did not add a bar");
		ctx.False(heat.m_bHeating, "the failed heat is no longer running");

		LF_ForgeHeat instant = new LF_ForgeHeat();
		LF_MiningForge.UseIronRecipe(instant);
		instant.m_iDurationMs = 0;
		instant.m_iNuggets = 1;
		ctx.True(LF_MiningForge.Start(instant), "a zero delay finishes inside start");
		ctx.Equal(1, instant.m_iBars, "a zero delay still yields the bar");
		ctx.Equal(0, instant.m_iNuggets, "a zero delay still consumes the nugget");
		ctx.False(instant.m_bHeating, "a zero delay is not left heating");
	}
}
