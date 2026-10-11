// red-proof: expected gather reward 1 and ran `tools\cli test --no-build --tier fast`
// on 2026-10-11. The suite reported
// `[ELTEST]   - gather reward stays 0 (expected 1, got 0)`
// and `SUMMARY tier=fast passed=58 failed=1 total=59`.
// Restored the expected value to 0.

// tier: LOGIC
class EL_Test_Roadmap_Resources : EL_Test
{
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName JUICE = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	protected static const ResourceName BAR = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";

	override string GetName()
	{
		return "roadmap/resources";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_JobManager jobs = EL_JobManager.GetInstance();
		ctx.NotNull(jobs, "job manager exists");
		if (ctx.FailureCount() > 0)
			return;

		jobs.ClearScopeRewards();
		ctx.Equal(0, jobs.GetGatherReward(APPLE), "gather reward stays 0");
		ctx.Equal(0, jobs.GetProcessReward(JUICE), "process reward stays 0");
		ctx.Equal(0, jobs.GetGatherReward(NUGGET), "nugget gather reward stays 0");
		ctx.Equal(0, jobs.GetProcessReward(BAR), "bar process reward stays 0");

		jobs.SetScopeReward(NUGGET, 12, 0);
		ctx.Equal(12, jobs.GetGatherReward(NUGGET), "a job scope can pay for a gather");
		ctx.Equal(0, jobs.GetProcessReward(NUGGET), "a gather scope does not pay for processing that prefab");
		ctx.Equal(0, jobs.GetGatherReward(APPLE), "an unrelated gather stays 0");

		jobs.SetScopeReward(BAR, 0, 40);
		ctx.Equal(40, jobs.GetProcessReward(BAR), "a job scope can pay for a process");
		ctx.Equal(0, jobs.GetGatherReward(BAR), "a process scope does not pay for gathering that prefab");
		ctx.Equal(0, jobs.GetProcessReward(JUICE), "an unrelated process stays 0");

		jobs.SetScopeReward(APPLE, -5, -5);
		ctx.Equal(0, jobs.GetGatherReward(APPLE), "a negative gather reward stays 0");
		ctx.Equal(0, jobs.GetProcessReward(APPLE), "a negative process reward stays 0");

		jobs.ClearScopeRewards();
		ctx.Equal(0, jobs.GetGatherReward(NUGGET), "clearing the scope drops the gather reward");
		ctx.Equal(0, jobs.GetProcessReward(BAR), "clearing the scope drops the process reward");

		EL_ResourceNodeDef missingPrefab = new EL_ResourceNodeDef();
		missingPrefab.m_sNodeId = "broken-node";
		missingPrefab.m_ItemPrefab = ResourceName.Empty;
		missingPrefab.m_iYield = 1;
		ctx.False(EL_ResourceChainCatalog.IsNodeUsable(missingPrefab), "a missing item prefab is unusable");

		EL_ResourceNodeDef zeroYield = new EL_ResourceNodeDef();
		zeroYield.m_sNodeId = "zero-yield";
		zeroYield.m_ItemPrefab = APPLE;
		zeroYield.m_iYield = 0;
		ctx.False(EL_ResourceChainCatalog.IsNodeUsable(zeroYield), "a non-positive yield is unusable");

		EL_ResourceNodeDef badCooldown = new EL_ResourceNodeDef();
		badCooldown.m_sNodeId = "bad-cooldown";
		badCooldown.m_ItemPrefab = APPLE;
		badCooldown.m_iYield = 1;
		badCooldown.m_fCooldownMs = -1;
		ctx.False(EL_ResourceChainCatalog.IsNodeUsable(badCooldown), "a negative cooldown is unusable");

		EL_ResourceMachineDef emptyMachine = new EL_ResourceMachineDef();
		emptyMachine.m_sMachineId = "empty-machine";
		emptyMachine.m_aInputs = new array<ref EL_ProcessingInput>();
		emptyMachine.m_aOutputs = new array<ref EL_ProcessingOutput>();
		ctx.False(EL_ResourceChainCatalog.IsMachineUsable(emptyMachine), "an empty recipe is unusable");
		ctx.False(EL_ProcessAction.IsRecipeConfigured(emptyMachine.m_aInputs, emptyMachine.m_aOutputs), "an empty recipe is refused before inventory changes");
	}
}

// tier: WORLD
class EL_Test_Roadmap_ResourcesWorld : EL_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const ResourceName JACKET_PREFAB = "{9F546CCA2582D16F}Prefabs/Characters/Uniforms/Jacket_M88.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	protected static const ResourceName BAR = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	protected static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";

	protected static ref array<IEntity> s_aBars = {};

	override string GetName()
	{
		return "e2e/roadmap-resources";
	}

	protected static bool InsertBar(IEntity entity)
	{
		s_aBars.Insert(entity);
		return true;
	}

	protected static bool FilterBar(IEntity entity)
	{
		if (!entity)
			return false;

		return EL_Utils.GetPrefabName(entity) == BAR;
	}

	protected int CountBarsNear(vector center)
	{
		s_aBars = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere(center, 4, InsertBar, FilterBar);
		return s_aBars.Count();
	}

	protected EL_ProcessAction FindIronProcess(IEntity furnace)
	{
		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(furnace);
		if (!actions)
			return null;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		foreach (BaseUserAction action : actionList)
		{
			EL_ProcessAction process = EL_ProcessAction.Cast(action);
			if (process && process.m_sMachineId == "furnace-iron")
				return process;
		}

		return null;
	}

	override void Run(EL_TestContext ctx)
	{
		EL_JobManager jobs = EL_JobManager.GetInstance();
		if (jobs)
			jobs.ClearScopeRewards();

		EL_ResourceNodeDef iron = EL_ResourceChainCatalog.FindNode("iron");
		ctx.True(EL_ResourceChainCatalog.IsNodeUsable(iron), "iron node resolves");
		if (iron)
		{
			ctx.Equal(1, iron.m_iYield, "iron yield is one nugget");
			ctx.EqualStr(PICKAXE, iron.m_ToolPrefab, "iron gather names the pickaxe");
		}

		EL_ResourceNodeDef ironDebug = EL_ResourceChainCatalog.FindNode("iron-debug");
		ctx.True(EL_ResourceChainCatalog.IsNodeUsable(ironDebug), "iron-debug node resolves");
		if (ironDebug)
		{
			ctx.Equal(4, ironDebug.m_iYield, "IronOre_Debug still grants four nuggets");
			ctx.True(ironDebug.m_ToolPrefab.IsEmpty(), "IronOre_Debug gather does not require a held tool");
		}

		EL_ResourceMachineDef machine = EL_ResourceChainCatalog.FindMachine("furnace-iron");
		ctx.True(EL_ResourceChainCatalog.IsMachineUsable(machine), "furnace-iron recipe resolves");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(1, machine.m_aInputs.Count(), "furnace-iron has one input");
		ctx.Equal(1, machine.m_aOutputs.Count(), "furnace-iron has one output");
		if (ctx.FailureCount() > 0)
			return;

		ctx.EqualStr(NUGGET, machine.m_aInputs[0].m_InputPrefab, "furnace consumes iron nuggets");
		ctx.Equal(2, machine.m_aInputs[0].m_iInputAmount, "furnace consumes two nuggets");
		ctx.EqualStr(BAR, machine.m_aOutputs[0].m_OutputPrefab, "furnace produces an iron bar");
		ctx.Equal(1, machine.m_aOutputs[0].m_iOutputAmount, "furnace produces one bar");

		IEntity furnace = GetGame().GetWorld().FindEntityByName("Furnace");
		ctx.True(furnace != null, "Furnace is placed");
		if (ctx.FailureCount() > 0)
			return;

		EL_ProcessAction ironProcess = FindIronProcess(furnace);
		ctx.NotNull(ironProcess, "Furnace carries the furnace-iron action");
		if (ctx.FailureCount() > 0)
			return;

		Resource characterResource = Resource.Load(CHARACTER_PREFAB);
		ctx.True(characterResource.IsValid(), "character prefab loads");
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointForFaction("CIVILIAN");
		if (!spawnPoint)
			spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();
		ctx.True(spawnPoint != null, "a spawn point resolves for the smelter");
		if (ctx.FailureCount() > 0)
			return;

		vector position, yawPitchRoll;
		spawnPoint.GetPositionAndRotation(position, yawPitchRoll);
		Math3D.AnglesToMatrix(yawPitchRoll, params.Transform);
		params.Transform[3] = position + "0 0.1 0";

		IEntity character = GetGame().SpawnEntityPrefab(characterResource, GetGame().GetWorld(), params);
		ctx.True(character != null, "smelter character spawns");
		if (ctx.FailureCount() > 0)
			return;

		InventoryStorageManagerComponent storage = EL_InventoryUtils.GetResponsibleStorageManager(character);
		ctx.NotNull(storage, "smelter resolves an inventory");
		if (ctx.FailureCount() > 0)
			return;

		IEntity jacket = EL_Utils.SpawnEntityPrefab(JACKET_PREFAB, character.GetOrigin());
		ctx.True(jacket != null, "jacket spawns for the smelter pockets");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(storage.TryInsertItem(jacket, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "jacket inserts into the smelter loadout");
		if (ctx.FailureCount() > 0)
			return;

		int cashBefore = EL_MoneyUtils.GetCash(character);
		int barsBefore = CountBarsNear(furnace.GetOrigin());
		ctx.Equal(0, EL_InventoryUtils.GetAmount(character, NUGGET), "smelter starts with no nuggets");
		ctx.Equal(0, EL_InventoryUtils.GetAmount(character, BAR), "smelter starts with no iron bar");

		ironProcess.PerformAction(furnace, character);
		ctx.Equal(0, EL_InventoryUtils.GetAmount(character, NUGGET), "a process with no nuggets leaves the inventory unchanged");
		ctx.Equal(0, EL_InventoryUtils.GetAmount(character, BAR), "a refused process does not grant a bar");
		ctx.Equal(barsBefore, CountBarsNear(furnace.GetOrigin()), "a refused process does not drop a bar");
		ctx.Equal(cashBefore, EL_MoneyUtils.GetCash(character), "a refused process does not mint cash");

		ctx.Equal(2, EL_InventoryUtils.AddAmount(character, NUGGET, 2), "two iron nuggets enter inventory");
		ctx.Equal(2, EL_InventoryUtils.GetAmount(character, NUGGET), "inventory holds two nuggets");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(ironProcess.CanBePerformedScript(character), "two nuggets can be smelted");
		ironProcess.PerformAction(furnace, character);

		int nuggetsAfter = EL_InventoryUtils.GetAmount(character, NUGGET);
		int barsInInventory = EL_InventoryUtils.GetAmount(character, BAR);
		int barsDropped = CountBarsNear(furnace.GetOrigin()) - barsBefore;
		ctx.Equal(0, nuggetsAfter, "two nuggets are consumed");
		ctx.Equal(1, barsInInventory + barsDropped, "two nuggets become one iron bar");
		ctx.Equal(cashBefore, EL_MoneyUtils.GetCash(character), "smelting does not mint cash");
		ctx.Equal(0, jobs.GetProcessReward(BAR), "the furnace line has no process reward");
	}
}
