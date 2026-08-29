// red-proof: point the tree's tool m_rTool at a nonexistent prefab (edit
// CuttableTree_Base.et m_aTools -> "{0}Prefabs/Missing.et") and run
// `tools\cli test --tier all`; the "tree tool list maps the axe" assertion
// fails. Proven red by that perturbation, then reverted.
// tier: WORLD
class EL_Test_LoggingChain : EL_Test
{
	protected static const ResourceName TREE = "{391791B1CAE33AFE}Prefabs/Resources/WoodCutting/Pinus_Sylvestris/CuttableTree_Pinus_Sylvestris_2f.et";
	protected static const ResourceName TREE_DST = "{B4E47486922CFCF4}Prefabs/Resources/WoodCutting/Pinus_Sylvestris/CuttableTree_Pinus_Sylvestris_2f_Dst.et";
	protected static const ResourceName TREE_STUMP = "{FC9E572F03833045}Prefabs/Resources/WoodCutting/Pinus_Sylvestris/CuttableTree_Pinus_Sylvestris_Stump_01.et";
	protected static const ResourceName LOG = "{32F5A906D8A573BC}Prefabs/Resources/WoodCutting/WoodLog_01_Small.et";
	protected static const ResourceName PLANK = "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et";
	protected static const ResourceName KINDLING = "{6697C9B8CEB5234C}Prefabs/Resources/WoodCutting/WoodKindling.et";
	protected static const ResourceName AXE = "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et";
	protected static const ResourceName SAWMILL = "{61418A72BA63D059}Prefabs/Processor/Sawmill.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "logging/chain-wiring";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(EL_TestContext ctx)
	{
		array<ResourceName> prefabs = { TREE, TREE_DST, TREE_STUMP, LOG, PLANK, KINDLING, AXE, SAWMILL };
		foreach (ResourceName path : prefabs)
		{
			ctx.True(Resource.Load(path).IsValid(), "logging prefab loads: " + path);
		}
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity tree = GetGame().SpawnEntityPrefab(Resource.Load(TREE), GetGame().GetWorld(), params);
		ctx.True(tree != null, "cuttable tree spawns");
		if (ctx.FailureCount() > 0)
			return;

		EL_DestructibleResourceComponent destructibleResource = EL_Component<EL_DestructibleResourceComponent>.Find(tree);
		ctx.True(destructibleResource != null, "tree carries EL_DestructibleResourceComponent");
		if (ctx.FailureCount() > 0)
			return;

		EL_DestructibleResourceComponentClass settings = EL_DestructibleResourceComponentClass.Cast(destructibleResource.GetComponentData(tree));
		ctx.True(settings != null, "tree destructible component has class data");
		if (ctx.FailureCount() > 0)
			return;

		bool axeMapped = false;
		if (settings.m_aTools)
		{
			foreach (EL_ResourceDestructionTool tool : settings.m_aTools)
			{
				if (tool.m_rTool == AXE)
				{
					axeMapped = true;
					ctx.True(tool.m_fHitDamage > 0, "axe maps to a positive per-hit damage");
				}
			}
		}
		ctx.True(axeMapped, "tree tool list maps the axe");

		bool spawnsPresent = settings.m_DestroySpawnObjects && settings.m_DestroySpawnObjects.Count() > 0;
		ctx.True(spawnsPresent, "tree configures destroy spawn objects");
		bool spawnsResolve = spawnsPresent;
		if (spawnsPresent)
		{
			foreach (SCR_BaseSpawnable baseSpawnable : settings.m_DestroySpawnObjects)
			{
				SCR_PrefabSpawnable spawnable = SCR_PrefabSpawnable.Cast(baseSpawnable);
				if (!spawnable)
					continue;

				foreach (ResourceName spawned : spawnable.m_Prefabs)
				{
					if (!Resource.Load(spawned).IsValid())
						spawnsResolve = false;
				}
			}
		}
		ctx.True(spawnsResolve, "tree destroy spawns all resolve");

		IEntity sawmill = GetGame().SpawnEntityPrefab(Resource.Load(SAWMILL), GetGame().GetWorld(), params);
		ctx.True(sawmill != null, "sawmill spawns");
		if (ctx.FailureCount() > 0)
			return;

		ActionsManagerComponent actionsManager = EL_Component<ActionsManagerComponent>.Find(sawmill);
		ctx.True(actionsManager != null, "sawmill carries ActionsManagerComponent");
		if (ctx.FailureCount() > 0)
			return;

		array<BaseUserAction> actionList = {};
		actionsManager.GetActionsList(actionList);
		bool processWired = false;
		foreach (BaseUserAction action : actionList)
		{
			EL_ProcessAction process = EL_ProcessAction.Cast(action);
			if (!process)
				continue;

			bool inputIsLog = false;
			if (process.m_aProcessingInputs)
			{
				foreach (EL_ProcessingInput input : process.m_aProcessingInputs)
				{
					if (input.m_InputPrefab == LOG)
						inputIsLog = true;
				}
			}

			bool outputIsWood = false;
			if (process.m_aProcessingOutputs)
			{
				foreach (EL_ProcessingOutput output : process.m_aProcessingOutputs)
				{
					if (output.m_OutputPrefab == PLANK || output.m_OutputPrefab == KINDLING)
						outputIsWood = true;
				}
			}

			if (inputIsLog && outputIsWood)
				processWired = true;
		}
		ctx.True(processWired, "sawmill process action consumes log and produces plank + kindling");
	}
}