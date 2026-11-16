// red-proof: observed 2026-11-17 (#1957): with the test and no pack data, `tools\cli test --no-build --only LF_Test_EveronArrivalEvent` failed (passed=0 failed=1, the overlay does not resolve the event); with the event, scene, dawn condition and outfit confs it passes (passed=1 failed=0).
// tier: WORLD
//! #1957 under the Everon 1989 header (the pack leg of `tools\cli test`): the flagship's dawn
//! arrival is pack data layered over the core event list (#1952). The event resolves by id, its
//! first phase waits for a dawn hour window, its scene has placed-waypoint ids in walking order that
//! the planner accepts, and the outfits the families wear load with every clothing prefab real. The
//! town card places the waypoints and the shell module that spawns the scene follows (#757), so no
//! marker is looked up here.
class LF_Test_EveronArrivalEvent : LF_Test
{
	static const ResourceName EVENTS_OVERLAY = "{891648ED4392010D}Configs/Events/LF_Events_Everon1989.conf";
	static const ResourceName SCENE_CONF = "{A949D53F95FB7C7F}Configs/Events/LF_Scene_RelocatedArrival.conf";
	static const ResourceName DAWN_CONF = "{A7F9A615B1CB8531}Configs/Events/LF_Condition_RelocatedDawn.conf";
	static const string EVENT_ID = "relocated_arrival";
	static const string DAWN_MODULE = "relocated_arrival_dawn";
	static const string SCENE_MODULE = "relocated_arrival_scene";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/arrival-event";
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
	//! True when the preset loads and every clothing prefab it names resolves.
	protected static bool OutfitResolves(LF_NpcOutfitPreset preset)
	{
		array<ResourceName> slots = {preset.m_Head, preset.m_Torso, preset.m_Legs, preset.m_Feet, preset.m_Vest, preset.m_Backpack};
		int named = 0;
		foreach (ResourceName slot : slots)
		{
			if (slot.IsEmpty())
				continue;

			named++;
			Resource clothing = Resource.Load(slot);
			if (!clothing || !clothing.IsValid())
				return false;
		}

		return named >= 3;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header && header.m_aLF_ConfigOverlays)
			ctx.True(header.m_aLF_ConfigOverlays.Find(EVENTS_OVERLAY) != -1, "the header lists the Everon event overlay");
		else
			ctx.Fail("the header lists no config overlays");

		// The act: the core overlay loader layers the pack's events over the core list.
		LF_EventDef arrival = LF_EventOverlay.Find(EVENT_ID);
		ctx.NotNull(arrival, "the relocated arrival event resolves through the overlay");
		if (arrival)
		{
			ctx.True(arrival.m_aPhases.Count() >= 2, "the event has a dawn phase and a walk phase");
			ctx.True(arrival.m_fCooldownSeconds >= 3600, "the arrival happens at most once a day");
			if (arrival.m_aPhases.Count() >= 2)
			{
				ctx.True(arrival.m_aPhases[0].m_aModules && arrival.m_aPhases[0].m_aModules.Find(DAWN_MODULE) != -1, "the first phase waits on the dawn condition");
				ctx.True(arrival.m_aPhases[1].m_aModules && arrival.m_aPhases[1].m_aModules.Find(SCENE_MODULE) != -1, "the second phase runs the arrival scene");
			}
		}

		// Its hour range is dawn.
		LF_EventPrecondition dawn = LF_EventPrecondition.Cast(Load(DAWN_CONF));
		ctx.NotNull(dawn, "the dawn condition loads");
		if (dawn)
		{
			ctx.True(dawn.m_eKind == LF_EEventConditionKind.HOUR_RANGE, "the dawn condition is an hour range");
			ctx.True(LF_EventPrecondition.HourInRange(6, dawn.m_fFromHour, dawn.m_fToHour), "06:00 is inside the window");
			ctx.False(LF_EventPrecondition.HourInRange(12, dawn.m_fFromHour, dawn.m_fToHour), "noon is outside the window");
			ctx.False(LF_EventPrecondition.HourInRange(3, dawn.m_fFromHour, dawn.m_fToHour), "03:00, before the light, is outside the window");
			ctx.False(LF_EventPrecondition.HourInRange(22, dawn.m_fFromHour, dawn.m_fToHour), "22:00 is outside the window");
		}

		// Its scene: placed waypoint ids in walking order that the planner accepts.
		LF_EventScene scene = LF_EventScene.Cast(Load(SCENE_CONF));
		ctx.NotNull(scene, "the arrival scene loads");
		if (scene)
		{
			ctx.EqualStr(EVENT_ID, scene.m_sSceneId, "the scene id is the event id");
			ctx.EqualStr("", LF_EventScenePlanner.Validate(scene), "the planner accepts the scene");
			ctx.True(scene.m_aWaypointIds.Count() >= 3, "the route has waypoints from the edge of town to the square");
			for (int i = 0; i < scene.m_aWaypointIds.Count(); i++)
				ctx.EqualStr(LF_Places.OrderedId(scene.m_sSceneId, i), scene.m_aWaypointIds[i], string.Format("waypoint %1 is the scene's placed marker %1, in order", i));

			ctx.True(scene.m_iNpcCount >= 3 && scene.m_iNpcCount <= LF_EventScenePlanner.HARD_NPC_CAP, "a few people walk, under the hard cap");
			ctx.False(LF_EventSceneSpawn.OutfitFor(scene.m_sRole, "probe").IsEmpty(), "the scene's role gives its walkers an outfit prefab");

			LF_EventScenePlan plan = LF_EventScenePlanner.Plan(scene, 99);
			ctx.True(plan.m_bGo, "the planner makes a plan with room in the AI budget");
			ctx.Equal(scene.m_aWaypointIds.Count(), plan.m_aWaypointIds.Count(), "the plan keeps every waypoint");
			LF_EventScenePlanner.ResetStatics();
		}

		// What the families wear.
		array<ResourceName> outfits = {
			"{51924080CFFD0D01}Configs/NpcOutfits/LF_Outfit_Relocated_Coat.conf",
			"{22D6B19D66CCA5F0}Configs/NpcOutfits/LF_Outfit_Relocated_Cardigan.conf",
			"{515C7AFD4FE4524A}Configs/NpcOutfits/LF_Outfit_Relocated_Shirt.conf"
		};
		foreach (ResourceName path : outfits)
		{
			LF_NpcOutfitPreset preset = LF_NpcOutfitPreset.Cast(Load(path));
			ctx.NotNull(preset, string.Format("outfit %1 loads", path));
			if (preset)
				ctx.True(OutfitResolves(preset), string.Format("outfit '%1' names real clothing in at least three slots", preset.m_sName));
		}
	}
}
