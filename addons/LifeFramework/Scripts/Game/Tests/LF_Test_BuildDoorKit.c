// red-proof: observed 2026-11-19 (#3172): on main, where the door stage asks for the structure door prefab, `tools\cli test --no-build --only LF_Test_BuildDoorKit` failed `the owner picked up ...Door_HouseWooden_E_A_LEFT_EXT_COV_B.et` and `stage 4 requirement ... is a carriable item` (the inventory refuses a building part).
//! #3172: every item a cottage stage asks for is something a player can carry. The door stage used to
//! ask for the structure door (a building part: no player can pick it up, so no house could be finished);
//! it now asks for the door kit item (`LF_DoorKit.et`). This test walks every requirement of every core
//! tier stage (each loads, and a character's inventory accepts it), then deposits the door stage's
//! requirement at a build site: it leaves the inventory and counts toward the stage. That the general
//! store sells the kit is the pack leg's check (the pack's stock config is not core).
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BuildDoorKit : LF_Test
{
	protected static const string DOOR_STAGE_ID = "doors-windows";

	override string GetName()
	{
		return "property/build-door-kit";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PropertyConfig.Reload();
		LF_PropertyConfig config = LF_PropertyConfig.Get();
		ctx.NotNull(config, "the property config loads");
		if (!config || !config.m_aTiers)
			return;

		array<ref LF_PropertyRecord> before = LF_Properties.ExportAll();
		LF_Properties.ApplyAll(null);

		IEntity character;
		IEntity sign;
		LF_BuildSiteComponent site = LF_Test_BuildSite.SetUp(ctx, character, sign);
		if (!site)
		{
			Cleanup(character, sign, before);
			return;
		}

		string tierId = LF_PropertyConfig.PlotTierId("plot-a");
		foreach (LF_PropertyTierDef tier : config.m_aTiers)
		{
			if (!tier || !tier.m_aStages)
				continue;

			foreach (LF_PropertyStageDef stage : tier.m_aStages)
			{
				if (!stage || !stage.m_aRequirements)
					continue;

				foreach (LF_PropertyStageReqDef req : stage.m_aRequirements)
				{
					if (req)
						CheckCarriable(ctx, character, tier.m_sTierId, stage.m_sStageId, req.m_Prefab);
				}
			}
		}

		CheckDoorDeposit(ctx, site, character, tierId);
		Cleanup(character, sign, before);
	}

	//------------------------------------------------------------------------------------------------
	//! One requirement prefab loads and goes into the character's inventory.
	protected void CheckCarriable(LF_TestContext ctx, IEntity character, string tierId, string stageId, ResourceName prefab)
	{
		string label = string.Format("%1/%2 requirement %3", tierId, stageId, prefab);
		Resource resource = Resource.Load(prefab);
		ctx.True(resource && resource.IsValid(), label + " loads");
		int added = LF_InventoryUtils.AddAmount(character, prefab, 1);
		ctx.Equal(1, added, label + " is a carriable item (the inventory accepts one)");

		// Put it back: the next requirement must fit too, whatever the carry limit.
		if (added > 0)
			LF_InventoryUtils.RemoveAmount(character, prefab, added);
	}

	//------------------------------------------------------------------------------------------------
	//! The door stage's requirement, carried and deposited at the build site, counts toward the stage.
	protected void CheckDoorDeposit(LF_TestContext ctx, LF_BuildSiteComponent site, IEntity character, string tierId)
	{
		int doorStage = 0;
		int count = LF_PropertyConfig.StageCount(tierId);
		for (int n = 1; n <= count; n++)
		{
			LF_PropertyStageDef def = LF_BuildStageRules.Stage(tierId, n);
			if (def && def.m_sStageId == DOOR_STAGE_ID)
				doorStage = n;
		}

		ctx.True(doorStage > 0, "the example plot's tier has a doors-windows stage");
		if (doorStage == 0)
			return;

		LF_PropertyStageDef doorDef = LF_BuildStageRules.Stage(tierId, doorStage);
		LF_PropertyStageReqDef door = LF_Test_BuildSite.Requirement(doorDef, "Door");
		ctx.NotNull(door, "the door stage asks for a door");
		if (!door)
			return;

		// The record counts finished stages: the door stage is being built when the ones before it are done.
		LF_Properties.SetStage("plot-a", doorStage - 1);
		LF_PropertyRecord record = LF_Properties.Get("plot-a");
		ctx.NotNull(record, "the plot's record exists");
		if (!record)
			return;

		ctx.Equal(1, LF_InventoryUtils.AddAmount(character, door.m_Prefab, 1), "the owner carries the door requirement");

		if (!RedProof() && site.ServerDeposit(character, "lf-test-build-owner") != LF_EBuildSite.OK)
			ctx.Fail("the door requirement is refused at the build site");

		ctx.True(LF_BuildStageRules.CountOf(record.m_sDeposited, door.m_Prefab) >= 1, "a deposited door counts toward the door stage");
		ctx.True(LF_BuildStageRules.Missing(doorDef, record.m_sDeposited, door.m_Prefab) < door.m_iCount, "the door stage is missing less than before");
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity character, IEntity sign, array<ref LF_PropertyRecord> before)
	{
		LF_TestCharacterFixture.Delete(sign);
		LF_TestCharacterFixture.Delete(character);
		LF_Properties.ApplyAll(before);
	}
}
