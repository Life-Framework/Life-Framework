// red-proof: observed 2026-11-19 (#3181) on main, before the event existed: `tools\cli test --no-build --only LF_Test_EveronSquareEvening` failed (`the square evening event resolves through the overlay`, the module and scene confs did not load). `RedProof()` skips the overlay lookup, so the event reads as missing.
// red-proof (#3724): `RedProof()` also makes the outfit count read as one, so "the four walkers wear at least three different civilian outfits" fails; the single-jacket conf failed it on main.
//! #3181 under the Everon 1989 header (the pack leg of `tools\cli test`): the flagship's evening event is pack
//! data layered over the core event list (#1952). It is in the merged table and enabled, its hour window is
//! the evening (true at 20:00, false at 03:00 and noon), and its scene names walkers who wear a real role outfit
//! and waypoints that are the scene's own placed markers in walking order, each resolved by place kind
//! (`event_waypoint`, `event_spawn`, `event_target`) under the scene id. The test world holds no island, so no
//! marker is looked up: the island card (#3395) places them under the same id.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronSquareEvening : LF_Test
{
	static const ResourceName SCENE_CONF = "{9C3B7F1D2E5A4068}Configs/Events/LF_Event_SquareEvening.conf";
	static const ResourceName EVENING_CONF = "{4F6D8A2B1C7E3095}Configs/Events/LF_Condition_Evening.conf";
	static const string EVENT_ID = "square_evening";
	static const string EVENING_MODULE = "square_evening_hours";
	static const string SCENE_MODULE = "square_evening_scene";
	static const string ARRIVAL_ID = "relocated_arrival";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/square-evening";
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
		LF_EventDef evening;
		if (!RedProof())
			evening = LF_EventOverlay.Find(EVENT_ID);

		ctx.NotNull(evening, "the square evening event resolves through the overlay");
		if (evening)
		{
			ctx.True(evening.m_bEnabled, "the square evening is enabled");
			ctx.True(evening.m_aPhases.Count() >= 2, "the event has a wait phase and a walk phase");
			ctx.True(evening.m_fCooldownSeconds >= 3600, "the evening happens at most once an evening");
			if (evening.m_aPhases.Count() >= 2)
			{
				ctx.True(evening.m_aPhases[0].m_aModules && evening.m_aPhases[0].m_aModules.Find(EVENING_MODULE) != -1, "the first phase waits on the evening condition");
				ctx.True(evening.m_aPhases[1].m_aModules && evening.m_aPhases[1].m_aModules.Find(SCENE_MODULE) != -1, "the second phase runs the walk scene");
			}
		}

		// The arrival is a separate row: this card does not touch it.
		ctx.NotNull(LF_EventOverlay.Find(ARRIVAL_ID), "the relocated arrival is still in the table");

		// Its hour range is the evening.
		LF_EventPrecondition hours = LF_EventPrecondition.Cast(Load(EVENING_CONF));
		ctx.NotNull(hours, "the evening condition loads");
		if (hours)
		{
			ctx.True(hours.m_eKind == LF_EEventConditionKind.HOUR_RANGE, "the evening condition is an hour range");
			ctx.True(LF_EventPrecondition.HourInRange(20, hours.m_fFromHour, hours.m_fToHour), "20:00 is inside the window");
			ctx.True(LF_EventPrecondition.HourInRange(19, hours.m_fFromHour, hours.m_fToHour), "19:00 is inside the window");
			ctx.False(LF_EventPrecondition.HourInRange(3, hours.m_fFromHour, hours.m_fToHour), "03:00 is outside the window");
			ctx.False(LF_EventPrecondition.HourInRange(12, hours.m_fFromHour, hours.m_fToHour), "noon is outside the window");
			ctx.False(LF_EventPrecondition.HourInRange(23, hours.m_fFromHour, hours.m_fToHour), "23:00 is outside the window");
		}

		// Its scene: the walkers wear a real role outfit and every waypoint is the scene's own marker, in order.
		LF_EventScene scene = LF_EventScene.Cast(Load(SCENE_CONF));
		ctx.NotNull(scene, "the square evening scene loads");
		if (scene)
		{
			ctx.EqualStr(EVENT_ID, scene.m_sSceneId, "the scene id is the event id");
			ctx.EqualStr("", LF_EventScenePlanner.Validate(scene), "the planner accepts the scene");
			ctx.True(scene.m_iNpcCount >= 3 && scene.m_iNpcCount <= 4, "three or four walkers come out");
			ctx.True(scene.m_aWaypointIds.Count() >= 3, "the route has waypoints from a lane to the square");

			// #3724: the walkers do not all wear the same jacket, and every outfit is a real prefab with an AI body.
			// RedProof() skips the count, so the single-jacket scene fails here.
			int distinct = 0;
			array<string> seen = {};
			if (scene.m_aPrefabs)
			{
				foreach (ResourceName walkerPrefab : scene.m_aPrefabs)
				{
					string walkerKey = walkerPrefab;
					if (seen.Find(walkerKey) != -1)
						continue;

					seen.Insert(walkerKey);
					distinct++;
					Resource walkerResource = Resource.Load(walkerPrefab);
					ctx.True(walkerResource && walkerResource.IsValid(), string.Format("the walker prefab %1 resolves", walkerKey));
				}
			}
			if (RedProof())
				distinct = 1;

			ctx.True(distinct >= 3, "the four walkers wear at least three different civilian outfits");
			for (int i = 0; i < scene.m_aWaypointIds.Count(); i++)
				ctx.EqualStr(LF_Places.OrderedId(scene.m_sSceneId, i), scene.m_aWaypointIds[i], string.Format("waypoint %1 is the scene's placed marker %1, in order", i));

			ResourceName outfit = LF_EventSceneSpawn.OutfitFor(scene.m_sRole, "probe");
			ctx.False(outfit.IsEmpty(), "the scene's role gives its walkers an outfit prefab");
			if (!outfit.IsEmpty())
			{
				Resource prefab = Resource.Load(outfit);
				ctx.True(prefab && prefab.IsValid(), string.Format("the walker outfit %1 resolves", outfit));
			}

			LF_EventScenePlan plan = LF_EventScenePlanner.Plan(scene, 99);
			ctx.True(plan.m_bGo, "the planner makes a plan with room in the AI budget");
			ctx.Equal(scene.m_iNpcCount, plan.m_aNpcs.Count(), "the plan keeps every walker");
			LF_EventScenePlanner.ResetStatics();

			// The marker kinds the scene reads, by the names the placed markers register under.
			ctx.EqualStr("event_waypoint", LF_EventWaypointComponent.KIND_WAYPOINT, "waypoints are the event_waypoint kind");
			ctx.EqualStr("event_spawn", LF_EventWaypointComponent.KIND_SPAWN, "walkers appear at the event_spawn kind");
			ctx.EqualStr("event_target", LF_EventWaypointComponent.KIND_TARGET, "the route ends at the event_target kind");
		}

		// The overlay's module refs register as the condition and the scene the phases name.
		LF_EventsConfig events = LF_EventOverlay.Get();
		ctx.True(events && LF_EventOverlay.FindModule(events, EVENING_MODULE) != -1, "the overlay lists the evening module");
		ctx.True(events && LF_EventOverlay.FindModule(events, SCENE_MODULE) != -1, "the overlay lists the scene module");
		if (events)
		{
			LF_EventModuleLoader.RegisterFrom(events);
			ctx.True(LF_EventShell.IsRegistered(EVENING_MODULE), "the evening condition registers");
			ctx.True(LF_EventSceneOperation.Cast(LF_EventShell.FindOperation(SCENE_MODULE)) != null, "the walk scene registers as a scene operation");
		}
	}
}
