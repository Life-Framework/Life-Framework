// red-proof: observed 2026-11-18 on the unfixed code (no stage signal) `tools\cli test --no-build --only LF_Test_QuestBuildStage` failed `finishing the foundation moves the owner's objective`, `... the key holder who deposited`; the same run passes with the hook.
//! quests/build-stage (#3195): a finished build stage reports "property.stage_done:<stageId>" to the plot's owner and
//! to every key holder who deposited or worked on it, and to nobody else. A stranger (no key) is refused at the site
//! and never contributes; a stage that is not the one finished moves nothing.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_QuestBuildStage : LF_Test
{
	protected static const ResourceName KEY_PREFAB = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";
	protected static const string OWNER = "lf-test-build-owner";	// the uid LF_Test_BuildSite.SetUp makes the plot's owner
	protected static const string FRIEND = "lf-test-stage-friend";
	protected static const string STRANGER = "lf-test-stage-stranger";
	protected static const string PLOT = "plot-a";
	protected static const string FOUNDATION = "property.stage_done:foundation";
	protected static const string WALLS = "property.stage_done:walls-roof";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "quests/build-stage";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		ctx.NotNull(manager, "the quest manager exists");
		if (!manager)
			return;

		ctx.EqualStr(FOUNDATION, LF_QuestProgressSignals.StageDone("foundation"), "the stage signal names the stage");
		LF_PropertyConfig.Reload();
		array<ref LF_PropertyRecord> before = LF_Properties.ExportAll();
		LF_Properties.ApplyAll(null);

		IEntity owner;
		IEntity sign;
		LF_BuildSiteComponent site = LF_Test_BuildSite.SetUp(ctx, owner, sign);
		IEntity friend = LF_TestCharacterFixture.Spawn(ctx);
		IEntity stranger = LF_TestCharacterFixture.Spawn(ctx);
		string tier = LF_PropertyConfig.PlotTierId(PLOT);
		LF_PropertyStageDef def = LF_BuildStageRules.Stage(tier, 2);
		ctx.NotNull(def, "the example plot's tier has a stage 2");
		ctx.NotNull(friend, "the key holder's character spawns");
		ctx.NotNull(stranger, "the stranger's character spawns");

		LF_QuestTrackerComponent ownerTracker;
		LF_QuestTrackerComponent friendTracker;
		LF_QuestTrackerComponent strangerTracker;
		if (owner)
			ownerTracker = LF_QuestTrackerComponent.Cast(owner.FindComponent(LF_QuestTrackerComponent));

		if (friend)
			friendTracker = LF_QuestTrackerComponent.Cast(friend.FindComponent(LF_QuestTrackerComponent));

		if (stranger)
			strangerTracker = LF_QuestTrackerComponent.Cast(stranger.FindComponent(LF_QuestTrackerComponent));

		if (!site || !def || !friend || !stranger || !ownerTracker || !friendTracker || !strangerTracker)
		{
			ctx.Fail("the fixture is not complete");
			Cleanup(owner, friend, stranger, sign, before);
			return;
		}

		LF_QuestProgressSignals.SetBodyForTest(OWNER, owner);
		LF_QuestProgressSignals.SetBodyForTest(FRIEND, friend);
		LF_QuestProgressSignals.SetBodyForTest(STRANGER, stranger);
		array<LF_QuestTrackerComponent> all = {ownerTracker, friendTracker, strangerTracker};
		foreach (LF_QuestTrackerComponent tracker : all)
		{
			Open(manager, tracker, FOUNDATION);
			Open(manager, tracker, WALLS);
		}

		// A stranger with no key is refused and gets nothing.
		LF_PropertyStageReqDef first = def.m_aRequirements[0];
		ctx.Equal(1, LF_InventoryUtils.AddAmount(stranger, first.m_Prefab, 1), "the stranger carries a bag");
		ctx.Equal(LF_EBuildSite.REFUSED_NOT_ALLOWED, site.ServerDeposit(stranger, STRANGER), "a stranger may not deposit");

		// The friend carries a key bound to the plot and deposits every item of the foundation.
		ctx.Equal(1, LF_InventoryUtils.AddAmount(friend, KEY_PREFAB, 1), "the friend is handed a house key");
		array<IEntity> keys = LF_InventoryUtils.FindItemsByPrefab(LF_InventoryUtils.GetResponsibleStorageManager(friend), KEY_PREFAB);
		LF_KeyComponent key;
		if (!keys.IsEmpty())
			key = LF_Component<LF_KeyComponent>.Find(keys[0]);

		ctx.NotNull(key, "the key item carries LF_KeyComponent");
		if (!key)
		{
			Cleanup(owner, friend, stranger, sign, before);
			return;
		}

		key.SetHouseIdentifier(PLOT);
		foreach (LF_PropertyStageReqDef req : def.m_aRequirements)
		{
			if (!DepositAs(ctx, site, friend, FRIEND, req.m_Prefab, req.m_iCount))
			{
				Cleanup(owner, friend, stranger, sign, before);
				return;
			}
		}

		ctx.False(Done(friendTracker, FOUNDATION), "depositing alone does not finish the stage");

		// The owner puts in the work; the last unit finishes the foundation.
		LF_PropertyRecord record = LF_Properties.Get(PLOT);
		ctx.Equal(1, record.m_iBuildStage, "the foundation is the stage being built");
		float now = 100;
		for (int unit = 1; unit <= def.m_iWorkUnits; unit++)
		{
			now += 5;
			if (RedProof() && unit == def.m_iWorkUnits)
				break;

			ctx.Equal(LF_EBuildSite.OK, site.ServerWork(owner, OWNER, now), string.Format("the owner's work unit %1 lands", unit));
		}

		ctx.Equal(2, record.m_iBuildStage, "the foundation is finished");
		ctx.True(Done(ownerTracker, FOUNDATION), "finishing the foundation moves the owner's objective");
		ctx.True(Done(friendTracker, FOUNDATION), "and the key holder who deposited");
		ctx.False(Done(strangerTracker, FOUNDATION), "and not the stranger's");
		ctx.False(Done(ownerTracker, WALLS), "another stage's signal moves nothing");
		ctx.False(Done(friendTracker, WALLS), "not even the key holder's");

		Cleanup(owner, friend, stranger, sign, before);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the character `count` of the item a few at a time and deposits each batch as `actorKey`.
	protected bool DepositAs(LF_TestContext ctx, LF_BuildSiteComponent site, IEntity character, string actorKey, ResourceName prefab, int count)
	{
		while (count > 0)
		{
			int batch = LF_Utils.MinInt(count, 2);
			if (LF_InventoryUtils.AddAmount(character, prefab, batch) != batch)
			{
				ctx.Fail("the character could not pick up " + prefab);
				return false;
			}

			if (site.ServerDeposit(character, actorKey) != LF_EBuildSite.OK)
			{
				ctx.Fail("deposit of " + prefab + " as " + actorKey + " refused");
				return false;
			}

			count -= batch;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A quest of one SIGNAL objective on this key, accepted by the character.
	protected void Open(LF_QuestManager manager, LF_QuestTrackerComponent tracker, string key)
	{
		LF_QuestObjective objective = new LF_QuestObjective();
		objective.m_eType = LF_EQuestEventType.SIGNAL;
		objective.m_iTarget = 1;
		objective.m_eMode = LF_EQuestProgressMode.ADD;
		objective.m_sFilter = key;

		LF_QuestDefinition def = new LF_QuestDefinition();
		def.m_sQuestId = QuestId(key);
		def.m_eRequiredJob = LF_EJobType.UNEMPLOYED;
		def.m_aObjectives = new array<ref LF_QuestObjective>();
		def.m_aObjectives.Insert(objective);
		manager.RegisterDefinition(def);
		tracker.GetTracker().Accept(def, 1, LF_EJobType.UNEMPLOYED, "", 0, new array<string>());
	}

	//------------------------------------------------------------------------------------------------
	protected string QuestId(string key)
	{
		return "lf_test_stage_" + key;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Done(LF_QuestTrackerComponent tracker, string key)
	{
		return tracker.GetTracker().GetStatus(QuestId(key)) == LF_EQuestStatus.COMPLETED;
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity owner, IEntity friend, IEntity stranger, IEntity sign, array<ref LF_PropertyRecord> before)
	{
		LF_QuestProgressSignals.SetBodyForTest(OWNER, null);
		LF_QuestProgressSignals.SetBodyForTest(FRIEND, null);
		LF_QuestProgressSignals.SetBodyForTest(STRANGER, null);
		LF_TestCharacterFixture.Delete(sign);
		LF_TestCharacterFixture.Delete(owner);
		LF_TestCharacterFixture.Delete(friend);
		LF_TestCharacterFixture.Delete(stranger);
		LF_Properties.ApplyAll(before);
	}
}
