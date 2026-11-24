// red-proof: observed 2026-11-24 (#3243), batched run `tools\cli test --no-build --red LF_Test_EveronCoalSeamGate`: RedProof() skips the pack node lookup and the gated swing, so `the pack adds the coal-seam node` and every check behind it fail.
//! #3243 (East Rise coal seam): under the Everon 1989 header (the pack leg of `tools\cli test`) the pack's `coal-seam`
//! node (a pack ResourceChains overlay row) opens at MINER 4 with six pieces per rock, and a rock pointed at it takes
//! a pickaxe hit from a first-day miner as a refusal: nothing drops and no piece breaks. The same rock on the plain
//! `coal` node still drops a lump (the control), so the refusal is the gate and nothing else.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronCoalSeamGate : LF_Test
{
	protected static const ResourceName COAL_ORE = "{5A986136ADB1EC2C}Prefabs/Resources/Mining/CoalOre.et";
	protected static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	protected static const ResourceName LUMP = "{C2AE9B7E62F0369F}Prefabs/Resources/Mining/CoalLump.et";
	protected static const vector SPOT = "70 0 70";

	protected ref array<IEntity> m_aFound = {};

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/coal-seam-gate";
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectLump(IEntity entity)
	{
		m_aFound.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsLump(IEntity entity)
	{
		return entity && LF_Utils.GetPrefabName(entity) == LUMP && !entity.GetParent();
	}

	//------------------------------------------------------------------------------------------------
	protected void FindLumps(vector centre)
	{
		m_aFound = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere(centre, 6, CollectLump, IsLump);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ResourceChainCatalog.Reload();
		LF_ResourceNodeDef row;
		if (!RedProof())
			row = LF_ResourceChainCatalog.FindNode("coal-seam");

		ctx.NotNull(row, "the pack adds the coal-seam node");
		if (!row)
			return;

		ctx.EqualStr("MINER", row.m_sLevelJob, "the coal seam is gated by the MINER trade");
		ctx.Equal(4, row.m_iMinLevel, "the coal seam opens at MINER 4");
		ctx.Equal(6, row.m_iMaxGathers, "a coal seam rock gives six pieces");
		ctx.EqualStr("#LF-Level_Far_Miner", LF_OreNodeComponent.LevelRefusal(row, 1), "a first-day miner is refused in words");
		ctx.EqualStr("", LF_OreNodeComponent.LevelRefusal(row, 4), "a MINER 4 miner is let in");

		LF_ResourceNodeDef plain = LF_ResourceChainCatalog.FindNode("coal");
		ctx.NotNull(plain, "the core coal node is still there");
		if (plain)
			ctx.Equal(0, plain.m_iMinLevel, "the plain core coal node stays ungated");

		float y = GetGame().GetWorld().GetSurfaceY(SPOT[0], SPOT[2]);
		vector at = Vector(SPOT[0], y, SPOT[2]);
		IEntity rock = LF_Utils.SpawnEntityPrefab(COAL_ORE, at);
		IEntity pickaxe = LF_Utils.SpawnEntityPrefab(PICKAXE, at + Vector(0, 5, 0));
		ctx.NotNull(rock, "the coal rock spawns");
		ctx.NotNull(pickaxe, "the pickaxe spawns");
		LF_OreNodeComponent ore;
		if (rock)
			ore = LF_OreNodeComponent.Cast(rock.FindComponent(LF_OreNodeComponent));
		ctx.NotNull(ore, "the rock carries LF_OreNodeComponent");
		if (ore && pickaxe)
		{
			vector hitPos = at + Vector(-0.6, 0.4, 0);

			// The gate: a swing from a miner at level 1 (no body is level 1) is refused.
			ore.UseNodeForTest("coal-seam");
			int refusedDrops = ore.OnToolHit(60, hitPos, vector.Right, pickaxe);
			ctx.Equal(0, refusedDrops, "a first-day swing at a coal-seam rock drops nothing");
			ctx.Equal(0, ore.GetBreaks(), "a first-day swing at a coal-seam rock breaks no piece");
			FindLumps(at);
			ctx.Equal(0, m_aFound.Count(), "no coal lump lies by the refused rock");

			// The control: the same rock and swing on the plain coal node drops a lump.
			ore.UseNodeForTest("coal");
			int controlDrops = ore.OnToolHit(60, hitPos, vector.Right, pickaxe);
			ctx.Equal(1, controlDrops, "the same swing at a plain coal rock drops a lump");
			FindLumps(at);
			ctx.Equal(1, m_aFound.Count(), "the plain rock's lump lies in the world");
		}

		FindLumps(at);
		foreach (IEntity lump : m_aFound)
			SCR_EntityHelper.DeleteEntityAndChildren(lump);
		m_aFound = new array<IEntity>();
		if (rock)
			SCR_EntityHelper.DeleteEntityAndChildren(rock);
		if (pickaxe)
			SCR_EntityHelper.DeleteEntityAndChildren(pickaxe);
	}
}
