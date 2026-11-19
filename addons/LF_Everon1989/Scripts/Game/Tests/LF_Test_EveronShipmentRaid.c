// red-proof: observed 2026-11-19 (#3443): `tools\cli test --no-build --red LF_Test_EveronShipmentRaid` printed `RED PROOF OK (all 1 listed test(s) fail with the hook on)`. `RedProof()` skips the overlay lookup and the seizure run, so the event reads as missing and no locker row is logged.
//! #3443 (card D of the raid split) under the Everon 1989 header (the pack leg of `tools\cli test`): beat 4 of the
//! shipment arc. A player turns in `shipment_report` at the police desk; after a delay the `shipment_raid` event
//! leaves its `tipoff` phase (the QUEST_DONE condition from #3784) and the `raid` phase runs two modules: the
//! dispatch and seize operation (#3785: a police call at the placed `event_target`, then the sample is looked for
//! and logged in the evidence locker or the call says "moved") and the scene of two gendarmes walking in from the
//! markers #3786 placed. All pack data in its own file under `Configs/Events/LF_Events_Everon1989/`. The test
//! world holds no island, so the seizure runs on a site override and the scene's markers are not looked up.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronShipmentRaid : LF_Test
{
	static const ResourceName SCENE_CONF = "{D2B2C672CFEBEFAD}Configs/Events/LF_Scene_ShipmentRaid.conf";
	static const ResourceName TIP_CONF = "{D5969DE4F818C4E3}Configs/Events/LF_Condition_ShipmentRaidTip.conf";
	static const ResourceName SEIZE_CONF = "{FCEC0626792EB80F}Configs/Events/LF_Dispatch_ShipmentRaid.conf";
	static const ResourceName SAMPLE_PREFAB = "{A3BFEFD984031D81}Prefabs/Items/Contraband/LF_ShipmentSample.et";
	static const ResourceName CRATE_PREFAB = "{C8C36ADC9DFC5FBB}Prefabs/Props/Crates/Crate_01_red.et";
	static const string EVENT_ID = "shipment_raid";
	static const string TIP_MODULE = "shipment_raid_tip";
	static const string DISPATCH_MODULE = "shipment_raid_dispatch";
	static const string SCENE_MODULE = "shipment_raid_scene";
	static const string QUEST_ID = "shipment_report";
	static const float GAME_WEEK_SECONDS = 50400; // 7 days of 120 real minutes (the flagship day)

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/shipment-raid";
	}

	//------------------------------------------------------------------------------------------------
	protected static Managed Load(ResourceName path)
	{
		Resource resource = BaseContainerTools.LoadContainer(path);
		if (!resource || !resource.IsValid() || !resource.GetResource())
			return null;

		return BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer());
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The act: the core overlay loader layers the pack's events over the core list.
		LF_EventDef raid;
		if (!RedProof())
			raid = LF_EventOverlay.Find(EVENT_ID);

		ctx.NotNull(raid, "the shipment raid event resolves through the overlay");
		if (raid)
		{
			ctx.True(raid.m_bEnabled, "the shipment raid event is enabled");
			ctx.True(raid.m_fCooldownSeconds >= GAME_WEEK_SECONDS, "the raid happens at most once a game week");
			ctx.Equal(2, raid.m_aPhases.Count(), "the event has two phases");
			if (raid.m_aPhases.Count() >= 2)
			{
				ctx.EqualStr("tipoff", raid.m_aPhases[0].m_sName, "the first phase is the tip-off");
				ctx.EqualStr("raid", raid.m_aPhases[1].m_sName, "the second phase is the raid");
				ctx.True(raid.m_aPhases[0].m_aModules && raid.m_aPhases[0].m_aModules.Find(TIP_MODULE) != -1, "the tip-off phase waits on the report");
				ctx.True(raid.m_aPhases[1].m_aModules && raid.m_aPhases[1].m_aModules.Find(DISPATCH_MODULE) != -1, "the raid phase files the police call and seizes");
				ctx.True(raid.m_aPhases[1].m_aModules && raid.m_aPhases[1].m_aModules.Find(SCENE_MODULE) != -1, "the raid phase sends the gendarmes");
			}
		}

		// Its precondition reads the quest state: nothing before the report, nothing before the delay, then it holds.
		LF_EventPrecondition tip = LF_EventPrecondition.Cast(Load(TIP_CONF));
		ctx.NotNull(tip, "the tip-off condition loads");
		if (tip)
		{
			ctx.True(tip.m_eKind == LF_EEventConditionKind.QUEST_DONE, "the tip-off is a quest-done condition");
			ctx.EqualStr(QUEST_ID, tip.m_sQuestId, "it waits on the police report");
			ctx.True(tip.m_fDelaySeconds > 0, "the gendarmes do not arrive the instant the report is filed");

			LF_EventContext none = LF_EventContext.Make(12, 1, 0, 5000);
			ctx.False(tip.Holds(none), "no report turned in holds nothing");

			LF_EventContext early = LF_EventContext.Make(12, 1, 0, 5000);
			early.NoteQuestDone(QUEST_ID, 5000);
			ctx.False(tip.Holds(early), "the moment of the report is too early");

			LF_EventContext due = LF_EventContext.Make(12, 1, 0, 5000 + tip.m_fDelaySeconds + 1);
			due.NoteQuestDone(QUEST_ID, 5000);
			ctx.True(tip.Holds(due), "after the delay the raid is due");

			LF_EventContext other = LF_EventContext.Make(12, 1, 0, 5000 + tip.m_fDelaySeconds + 1);
			other.NoteQuestDone("shipment_sold", 5000);
			ctx.False(tip.Holds(other), "selling the sample instead of reporting it brings no raid");

			if (tip.m_fWindowSeconds > 0)
			{
				LF_EventContext late = LF_EventContext.Make(12, 1, 0, 5000 + tip.m_fDelaySeconds + tip.m_fWindowSeconds + 1);
				late.NoteQuestDone(QUEST_ID, 5000);
				ctx.False(tip.Holds(late), "a report left unanswered for too long no longer brings a raid");
			}
		}

		// Its scene: two gendarmes walking the placed raid route.
		LF_EventScene scene = LF_EventScene.Cast(Load(SCENE_CONF));
		ctx.NotNull(scene, "the shipment raid scene loads");
		if (scene)
		{
			ctx.EqualStr(EVENT_ID, scene.m_sSceneId, "the scene id is the event id");
			ctx.EqualStr("", LF_EventScenePlanner.Validate(scene), "the planner accepts the scene");
			ctx.Equal(2, scene.m_iNpcCount, "two gendarmes come to the door");
			for (int i = 0; i < scene.m_aWaypointIds.Count(); i++)
				ctx.EqualStr(LF_Places.OrderedId(scene.m_sSceneId, i), scene.m_aWaypointIds[i], string.Format("waypoint %1 is the scene's placed marker %1, in order", i));

			ctx.True(scene.m_sDropPrefab.IsEmpty(), "the raid leaves nothing behind (the dock scene's sample is the one seized)");
			int gendarmes;
			if (scene.m_aPrefabs)
			{
				foreach (ResourceName gendarme : scene.m_aPrefabs)
				{
					gendarmes++;
					Resource gendarmeResource = Resource.Load(gendarme);
					ctx.True(gendarmeResource && gendarmeResource.IsValid(), string.Format("the gendarme prefab %1 resolves", gendarme));
				}
			}
			ctx.True(gendarmes >= 1, "the scene names a gendarme prefab");

			LF_EventScenePlan plan = LF_EventScenePlanner.Plan(scene, 99);
			ctx.True(plan.m_bGo, "the planner makes a plan with room in the AI budget");
			ctx.Equal(scene.m_iNpcCount, plan.m_aNpcs.Count(), "the plan keeps both gendarmes");
			LF_EventScenePlanner.ResetStatics();

			LF_EventScenePlan tight = LF_EventScenePlanner.Plan(scene, scene.m_iNpcCount - 1);
			ctx.False(tight.m_bGo, "the planner skips the scene when the AI budget cannot hold the gendarmes");
			LF_EventScenePlanner.ResetStatics();
		}

		// The overlay's module refs register as the condition and the two operations the phases name.
		LF_EventsConfig events = LF_EventOverlay.Get();
		ctx.True(events && LF_EventOverlay.FindModule(events, TIP_MODULE) != -1, "the overlay lists the tip-off module");
		ctx.True(events && LF_EventOverlay.FindModule(events, DISPATCH_MODULE) != -1, "the overlay lists the dispatch module");
		ctx.True(events && LF_EventOverlay.FindModule(events, SCENE_MODULE) != -1, "the overlay lists the scene module");
		if (!events)
			return;

		LF_EventModuleLoader.RegisterFrom(events);
		ctx.True(LF_EventShell.IsRegistered(TIP_MODULE), "the tip-off condition registers");
		ctx.True(LF_EventSceneOperation.Cast(LF_EventShell.FindOperation(SCENE_MODULE)) != null, "the gendarme scene registers as a scene operation");
		LF_EventDispatchSeizeOperation seize = LF_EventDispatchSeizeOperation.Cast(LF_EventShell.FindOperation(DISPATCH_MODULE));
		ctx.NotNull(seize, "the dispatch module registers as a dispatch and seize operation");
		if (!seize || !seize.m_Config)
			return;

		// What it seizes: the shipment sample and nothing a player can buy; the call goes to the raid scene's target.
		ctx.EqualStr("", seize.m_Config.Validate(), "the dispatch and seize config is complete");
		ctx.EqualStr(EVENT_ID, seize.m_Config.m_sSceneId, "the call goes to the raid scene's placed target");
		ctx.True(LF_Utils.SamePrefab(SAMPLE_PREFAB, seize.m_Config.m_Item), "the item seized is the shipment sample");
		ctx.False(LF_Utils.SamePrefab(CRATE_PREFAB, seize.m_Config.m_Item), "the red crate prop is left alone");
		ctx.False(seize.m_Config.m_sPlace.IsEmpty(), "the evidence row names the place");

		if (Replication.IsServer())
			RunSeizure(ctx, seize);
	}

	//------------------------------------------------------------------------------------------------
	//! The operation as the raid phase runs it, with the site standing in for the island's placed marker:
	//! the sample at its place is logged in the evidence locker; the sample the smugglers moved is not.
	protected void RunSeizure(LF_TestContext ctx, LF_EventDispatchSeizeOperation seize)
	{
		// Only this feature's and its two sinks' statics; LF_StaticReset.ResetAll() would wipe every feature's.
		LF_EvidenceLocker.ResetStatics();
		LF_Dispatch.ClearLog();

		float x = 40;
		float z = 40;
		vector site = Vector(x, GetGame().GetWorld().GetSurfaceY(x, z) + 0.2, z);
		seize.m_bSiteOverride = true;
		seize.m_vSiteOverride = site;

		float radius = seize.m_Config.m_fRadius;
		IEntity sample = LF_ExploitGuardsWorldFixture.Spawn(SAMPLE_PREFAB, site + Vector(radius * 0.5, 0, 0));
		ctx.NotNull(sample, "the sample spawns inside the warehouse radius");
		if (!sample)
			return;

		EntityID sampleId = sample.GetID();
		int calls = LF_Dispatch.CallCount();
		int rows = LF_EvidenceLocker.Count();
		bool acted;
		if (!RedProof())
			acted = seize.Run(EVENT_ID, "raid", LF_EventContext.Make(5, 1, 1, 100));

		ctx.True(acted, "the raid acted");
		ctx.Equal(LF_EDispatchSeizeOutcome.SEIZED, seize.m_eLast, "the sample at the warehouse is seized");
		ctx.Equal(calls + 1, LF_Dispatch.CallCount(), "one police call was filed");
		ctx.Equal(rows + 1, LF_EvidenceLocker.Count(), "one evidence locker row was logged");
		LF_DispatchRecord record = LF_Dispatch.Last();
		if (record)
			ctx.EqualStr(seize.m_Config.m_sOffence, record.m_sOffence, "the call carries the smuggling offence");

		array<ref LF_EvidenceLockerRow> lockerRows = LF_EvidenceLocker.ExportAll();
		if (lockerRows.Count() > 0)
		{
			LF_EvidenceLockerRow row = lockerRows[lockerRows.Count() - 1];
			ctx.EqualStr(seize.m_Config.m_sPlace, row.m_sPlace, "the locker row names the warehouse");
			ctx.True(LF_Utils.SamePrefab(SAMPLE_PREFAB, row.m_sPrefab), "the locker row holds the sample");
		}
		ctx.True(GetGame().GetWorld().FindEntityByID(sampleId) == null, "the seized sample is taken out of the world");

		// The smugglers got it out first: the police are told, nothing is seized, and what is left is not touched.
		IEntity moved = LF_ExploitGuardsWorldFixture.Spawn(SAMPLE_PREFAB, site + Vector(radius + 6, 0, 0));
		acted = seize.Run(EVENT_ID, "raid", LF_EventContext.Make(5, 1, 1, 5000));
		ctx.Equal(LF_EDispatchSeizeOutcome.MOVED, seize.m_eLast, "a sample moved out of the warehouse is not seized");
		ctx.Equal(calls + 2, LF_Dispatch.CallCount(), "the police were still called");
		ctx.Equal(rows + 1, LF_EvidenceLocker.Count(), "nothing more was logged");
		ctx.True(moved != null && GetGame().GetWorld().FindEntityByID(moved.GetID()) != null, "the moved sample is left alone");
		LF_ExploitGuardsWorldFixture.Despawn(moved);

		// A red crate prop at the place is not the sample: it is never taken.
		IEntity crate = LF_ExploitGuardsWorldFixture.Spawn(CRATE_PREFAB, site);
		seize.Run(EVENT_ID, "raid", LF_EventContext.Make(5, 1, 1, 9000));
		ctx.Equal(LF_EDispatchSeizeOutcome.MOVED, seize.m_eLast, "a crate at the place is not the sample");
		ctx.True(crate != null && GetGame().GetWorld().FindEntityByID(crate.GetID()) != null, "the crate prop is left alone");
		LF_ExploitGuardsWorldFixture.Despawn(crate);

		seize.m_bSiteOverride = false;
		LF_Dispatch.StopAllAlarms();
		LF_Dispatch.ClearLog();
		LF_EvidenceLocker.ResetStatics();
	}
}
