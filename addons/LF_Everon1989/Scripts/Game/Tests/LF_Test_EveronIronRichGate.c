// red-proof: observed 2026-11-24 (#3242), batched run `tools\cli test --no-build --red LF_Test_EveronIronRichGate`: RedProof() skips the pack node lookup and the gated swing, so `the pack adds the iron-rich node` and every check behind it fail.
//! #3242 (Highstone iron mine): under the Everon 1989 header (the pack leg of `tools\cli test`) the pack's `iron-rich`
//! node (a pack ResourceChains overlay row) opens at MINER 12 with six pieces per rock, and a rock pointed at it takes
//! a pickaxe hit from a first-day miner as a refusal: nothing drops and no piece breaks. The same rock on the plain
//! `iron` node still drops a nugget (the control), so the refusal is the gate and nothing else.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronIronRichGate : LF_Test
{
	protected static const ResourceName IRON_ORE = "{2C571D6F7C05E095}Prefabs/Resources/Mining/IronOre.et";
	protected static const ResourceName RICH_ORE = "{8406DCAC1238E2A3}Prefabs/Resources/Mining/LF_IronOreRich_Everon1989.et";
	protected static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	protected static const vector SPOT = "70 0 70";

	protected ref array<IEntity> m_aFound = {};

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/iron-rich-gate";
	}

	//------------------------------------------------------------------------------------------------
	//! The node id a rock prefab's LF_OreNodeComponent names.
	protected string NodeOfPrefab(ResourceName prefab)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return "";

		IEntityComponentSource source = SCR_BaseContainerTools.FindComponentSource(resource, LF_OreNodeComponent);
		string id;
		if (source)
			source.Get("m_sNodeId", id);

		return id;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectNugget(IEntity entity)
	{
		m_aFound.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsNugget(IEntity entity)
	{
		return entity && LF_Utils.GetPrefabName(entity) == NUGGET && !entity.GetParent();
	}

	//------------------------------------------------------------------------------------------------
	protected void FindNuggets(vector centre)
	{
		m_aFound = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere(centre, 6, CollectNugget, IsNugget);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ResourceChainCatalog.Reload();
		LF_ResourceNodeDef row;
		if (!RedProof())
			row = LF_ResourceChainCatalog.FindNode("iron-rich");

		ctx.NotNull(row, "the pack adds the iron-rich node");
		if (!row)
			return;

		ctx.EqualStr("MINER", row.m_sLevelJob, "the Highstone iron is gated by the MINER trade");
		ctx.Equal(12, row.m_iMinLevel, "the Highstone iron opens at MINER 12");
		ctx.Equal(6, row.m_iMaxGathers, "a Highstone iron rock gives six pieces");
		ctx.EqualStr("#LF-Level_Far_Miner", LF_OreNodeComponent.LevelRefusal(row, 1), "a first-day miner is refused in words");
		ctx.EqualStr("", LF_OreNodeComponent.LevelRefusal(row, 12), "a MINER 12 miner is let in");

		LF_ResourceNodeDef plain = LF_ResourceChainCatalog.FindNode("iron");
		ctx.NotNull(plain, "the core iron node is still there");
		if (plain)
		{
			ctx.Equal(0, plain.m_iMinLevel, "the plain core iron node stays ungated");
			ctx.EqualStr("", LF_OreNodeComponent.LevelRefusal(plain, 7), "a MINER 7 miner (the hour-5 player) mines the plain iron rocks");
		}

		// The face has two kinds of rock: the pack's amber rich rock names iron-rich, the plain rock names iron.
		ctx.EqualStr("iron-rich", NodeOfPrefab(RICH_ORE), "the rich rock prefab names the iron-rich node");
		ctx.EqualStr("iron", NodeOfPrefab(IRON_ORE), "the plain rock prefab names the iron node");
		ctx.EqualStr("#LF-Level_Near_Miner", LF_OreNodeComponent.LevelRefusal(row, 11), "MINER 11 is refused at the rich rock, nearly there");
		ctx.EqualStr("#LF-Level_Far_Miner", LF_OreNodeComponent.LevelRefusal(row, 7), "a MINER 7 miner is refused at the rich rock");

		float y = GetGame().GetWorld().GetSurfaceY(SPOT[0], SPOT[2]);
		vector at = Vector(SPOT[0], y, SPOT[2]);
		IEntity rock = LF_Utils.SpawnEntityPrefab(IRON_ORE, at);
		IEntity pickaxe = LF_Utils.SpawnEntityPrefab(PICKAXE, at + Vector(0, 5, 0));
		ctx.NotNull(rock, "the iron rock spawns");
		ctx.NotNull(pickaxe, "the pickaxe spawns");
		LF_OreNodeComponent ore;
		if (rock)
			ore = LF_OreNodeComponent.Cast(rock.FindComponent(LF_OreNodeComponent));
		ctx.NotNull(ore, "the rock carries LF_OreNodeComponent");
		if (ore && pickaxe)
		{
			vector hitPos = at + Vector(-0.6, 0.4, 0);

			// The gate: a swing from a miner at level 1 (no body is level 1) is refused.
			ore.UseNodeForTest("iron-rich");
			int refusedDrops = ore.OnToolHit(60, hitPos, vector.Right, pickaxe);
			ctx.Equal(0, refusedDrops, "a first-day swing at an iron-rich rock drops nothing");
			ctx.Equal(0, ore.GetBreaks(), "a first-day swing at an iron-rich rock breaks no piece");
			FindNuggets(at);
			ctx.Equal(0, m_aFound.Count(), "no iron nugget lies by the refused rock");

			// The control: the same rock and swing on the plain iron node drops a nugget.
			ore.UseNodeForTest("iron");
			int controlDrops = ore.OnToolHit(60, hitPos, vector.Right, pickaxe);
			ctx.Equal(1, controlDrops, "the same swing at a plain iron rock drops a nugget");
			FindNuggets(at);
			ctx.Equal(1, m_aFound.Count(), "the plain rock's nugget lies in the world");
		}

		FindNuggets(at);
		foreach (IEntity nugget : m_aFound)
			SCR_EntityHelper.DeleteEntityAndChildren(nugget);
		m_aFound = new array<IEntity>();
		if (rock)
			SCR_EntityHelper.DeleteEntityAndChildren(rock);
		if (pickaxe)
			SCR_EntityHelper.DeleteEntityAndChildren(pickaxe);
	}
}
