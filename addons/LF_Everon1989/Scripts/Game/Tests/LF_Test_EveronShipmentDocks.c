// red-proof: observed 2026-11-19 (#3439) on main, before the event existed: `tools\cli test --no-build --only LF_Test_EveronShipmentDocks` failed (`the shipment docks event resolves through the overlay`, the module and scene confs did not load). `RedProof()` skips the overlay lookup, so the event reads as missing. #3783 added: `--red LF_Test_EventSceneDrop,LF_Test_EveronShipmentDocks` printed `RED-OK everon/shipment-docks (fails with the hook on, 36 assertions)`; the sample and plan assertions go red with the overlay lookup skipped.
//! #3439 under the Everon 1989 header (the pack leg of `tools\cli test`): beat 1 of the shipment arc. The coaster
//! never docks (the base game has no ship), so a truck backs onto the quay and a crew walks it in. The event is
//! pack data layered over the core event list (#1952) from its own file in `Configs/Events/LF_Events_Everon1989/`:
//! phases `dawn` then `unload`, a cooldown of one game week (seven 120-minute days), a dawn hour window, a scene
//! with a parked truck, three dockers in work clothes and waypoints that are the scene's own placed markers in
//! walking order. The red crate (`scene-crate`) is the warehouse layer's (#3445); this test only proves its
//! prefab resolves. The test world holds no island, so no marker is looked up.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronShipmentDocks : LF_Test
{
	static const ResourceName SCENE_CONF = "{7A1D3E5C9B2F4806}Configs/Events/LF_Scene_ShipmentDocks.conf";
	static const ResourceName DAWN_CONF = "{5B3F8D1A6C2E7094}Configs/Events/LF_Condition_ShipmentDawn.conf";
	static const ResourceName CRATE_PREFAB = "{C8C36ADC9DFC5FBB}Prefabs/Props/Crates/Crate_01_red.et";
	static const ResourceName SAMPLE_PREFAB = "{A3BFEFD984031D81}Prefabs/Items/Contraband/LF_ShipmentSample.et";
	static const string EVENT_ID = "shipment_docks";
	static const string DAWN_MODULE = "shipment_docks_dawn";
	static const string SCENE_MODULE = "shipment_docks_scene";
	static const float GAME_WEEK_SECONDS = 50400; // 7 days of 120 real minutes (the flagship day)

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/shipment-docks";
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
		LF_EventDef docks;
		if (!RedProof())
			docks = LF_EventOverlay.Find(EVENT_ID);

		ctx.NotNull(docks, "the shipment docks event resolves through the overlay");
		if (docks)
		{
			ctx.True(docks.m_bEnabled, "the shipment docks event is enabled");
			ctx.True(docks.m_fCooldownSeconds >= GAME_WEEK_SECONDS, "the ship docks at most once a game week");
			ctx.Equal(2, docks.m_aPhases.Count(), "the event has two phases");
			if (docks.m_aPhases.Count() >= 2)
			{
				ctx.EqualStr("dawn", docks.m_aPhases[0].m_sName, "the first phase is dawn");
				ctx.EqualStr("unload", docks.m_aPhases[1].m_sName, "the second phase is unload");
				ctx.True(docks.m_aPhases[0].m_aModules && docks.m_aPhases[0].m_aModules.Find(DAWN_MODULE) != -1, "the dawn phase waits on the dawn condition");
				ctx.True(docks.m_aPhases[1].m_aModules && docks.m_aPhases[1].m_aModules.Find(SCENE_MODULE) != -1, "the unload phase runs the dock scene");
			}
		}

		// Its dawn window.
		LF_EventPrecondition hours = LF_EventPrecondition.Cast(Load(DAWN_CONF));
		ctx.NotNull(hours, "the dawn condition loads");
		if (hours)
		{
			ctx.True(hours.m_eKind == LF_EEventConditionKind.HOUR_RANGE, "the dawn condition is an hour range");
			ctx.True(LF_EventPrecondition.HourInRange(6, hours.m_fFromHour, hours.m_fToHour), "06:00 is inside the window");
			ctx.False(LF_EventPrecondition.HourInRange(3, hours.m_fFromHour, hours.m_fToHour), "03:00 is outside the window");
			ctx.False(LF_EventPrecondition.HourInRange(12, hours.m_fFromHour, hours.m_fToHour), "noon is outside the window");
			ctx.False(LF_EventPrecondition.HourInRange(20, hours.m_fFromHour, hours.m_fToHour), "20:00 is outside the window");
		}

		// Its scene: the truck, the crew and the route.
		LF_EventScene scene = LF_EventScene.Cast(Load(SCENE_CONF));
		ctx.NotNull(scene, "the shipment docks scene loads");
		if (scene)
		{
			ctx.EqualStr(EVENT_ID, scene.m_sSceneId, "the scene id is the event id");
			ctx.EqualStr("", LF_EventScenePlanner.Validate(scene), "the planner accepts the scene");
			ctx.Equal(3, scene.m_iNpcCount, "a foreman and two dockers come out");
			ctx.True(scene.m_aWaypointIds.Count() >= 3, "the route has waypoints from the quay to the warehouse");
			for (int i = 0; i < scene.m_aWaypointIds.Count(); i++)
				ctx.EqualStr(LF_Places.OrderedId(scene.m_sSceneId, i), scene.m_aWaypointIds[i], string.Format("waypoint %1 is the scene's placed marker %1, in order", i));

			// A truck parks on the quay: a real prefab, not a slot that resolves to nothing.
			ctx.False(scene.m_sVehicleSlot.IsEmpty(), "the scene parks a truck");
			ResourceName truck = LF_EventSceneSpawn.VehiclePrefabFor(scene.m_sVehicleSlot);
			ctx.False(truck.IsEmpty(), "the truck resolves to a prefab");
			if (!truck.IsEmpty())
			{
				Resource truckResource = Resource.Load(truck);
				ctx.True(truckResource && truckResource.IsValid(), string.Format("the truck prefab %1 resolves", truck));
			}

			// The dockers wear work clothes and every outfit is a real prefab.
			int distinct = 0;
			array<string> seen = {};
			if (scene.m_aPrefabs)
			{
				foreach (ResourceName dockerPrefab : scene.m_aPrefabs)
				{
					string key = dockerPrefab;
					if (seen.Find(key) != -1)
						continue;

					seen.Insert(key);
					distinct++;
					Resource dockerResource = Resource.Load(dockerPrefab);
					ctx.True(dockerResource && dockerResource.IsValid(), string.Format("the docker prefab %1 resolves", key));
				}
			}
			ctx.True(distinct >= 2, "the crew does not all wear the same outfit");

			// The marked crate the scene leaves in the warehouse.
			Resource crate = Resource.Load(CRATE_PREFAB);
			ctx.True(crate && crate.IsValid(), "the red crate prefab resolves");

			// The shipment's sample (#3783): the scene leaves one on the placed item spot when the ship docks.
			ctx.True(scene.m_sDropPrefab == SAMPLE_PREFAB, "the scene leaves the shipment sample");
			Resource sample = Resource.Load(scene.m_sDropPrefab);
			ctx.True(sample && sample.IsValid(), "the sample prefab resolves");

			// The scene fits the AI budget: it plans with room, and skips with a plain reason when the budget is spent.
			LF_EventScenePlan plan = LF_EventScenePlanner.Plan(scene, 99);
			ctx.True(plan.m_bGo, "the planner makes a plan with room in the AI budget");
			ctx.Equal(scene.m_iNpcCount, plan.m_aNpcs.Count(), "the plan keeps every docker");
			ctx.EqualStr(scene.m_sVehicleSlot, plan.m_sVehicleSlot, "the plan carries the truck");
			ctx.True(plan.m_sDropPrefab == SAMPLE_PREFAB, "the plan carries the sample");
			LF_EventScenePlanner.ResetStatics();

			LF_EventScenePlan tight = LF_EventScenePlanner.Plan(scene, scene.m_iNpcCount - 1);
			ctx.False(tight.m_bGo, "the planner skips the scene when the AI budget cannot hold the crew");
			LF_EventScenePlanner.ResetStatics();
		}

		// The overlay's module refs register as the condition and the scene the phases name.
		LF_EventsConfig events = LF_EventOverlay.Get();
		ctx.True(events && LF_EventOverlay.FindModule(events, DAWN_MODULE) != -1, "the overlay lists the dawn module");
		ctx.True(events && LF_EventOverlay.FindModule(events, SCENE_MODULE) != -1, "the overlay lists the scene module");
		if (events)
		{
			LF_EventModuleLoader.RegisterFrom(events);
			ctx.True(LF_EventShell.IsRegistered(DAWN_MODULE), "the dawn condition registers");
			ctx.True(LF_EventSceneOperation.Cast(LF_EventShell.FindOperation(SCENE_MODULE)) != null, "the dock scene registers as a scene operation");
		}
	}
}
