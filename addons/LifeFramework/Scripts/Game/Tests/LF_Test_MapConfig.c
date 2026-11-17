// red-proof: 2026-11-17, `toolsi test --no-build --red LF_Test_MapConfig`: RED-OK (RedProof() hides the two map components, 5 assertions fail); green without it. Before that, a first run with vanilla's marker set crashed EOnInit (squad-leader InitServerLogic, no groups manager), which the no-dynamic-entry check now guards.
// tier: WORLD
//! The map plumbing (#2229, everon-map.md section 0): the Roleplay game mode carries the two map
//! components vanilla's `GameMode_Base.et` has and ours lacked, and the gadget map opens our config.
//! Without `SCR_MapConfigComponent` the map menu returns before it sets the map up; without
//! `SCR_MapMarkerManagerComponent` no marker of any kind shows. `LF_MapMarkerConfig.conf` is vanilla's marker set without the dynamic squad-leader and air-support
//! entries (their server logic needs the groups manager). `LF_MapFullscreen.conf` is vanilla's
//! fullscreen config (a copy: an inherited array cannot drop items, seen 2026-11-17) with only the UI pieces a 1989 civilian map uses: no Conflict
//! (campaign) UI, no task list, no journal (it needs the task list and the briefing), no military
//! debug icons. Town names and roads are looked at in the emulator and the visual cards' final
//! `map-shots` (card M0b); this test only proves the wiring.
class LF_Test_MapConfig : LF_Test
{
	static const ResourceName FULLSCREEN = "{92FEABB35503403C}Configs/Map/LF_MapFullscreen.conf";
	static const ResourceName MARKERS = "{F5D82559BE078538}Configs/Map/LF_MapMarkerConfig.conf";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/config";
	}

	//------------------------------------------------------------------------------------------------
	//! Does the config's UI component list hold an instance of this class?
	protected static bool HasUIComponent(notnull SCR_MapConfig config, typename type)
	{
		if (!config.m_aUIComponents)
			return false;

		foreach (SCR_MapUIBaseComponent component : config.m_aUIComponents)
		{
			if (component && component.Type().IsInherited(type))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		ctx.NotNull(gameMode, "a game mode is running");
		if (!gameMode)
			return;

		SCR_MapConfigComponent configComponent = SCR_MapConfigComponent.Cast(gameMode.FindComponent(SCR_MapConfigComponent));
		SCR_MapMarkerManagerComponent markerManager = SCR_MapMarkerManagerComponent.Cast(gameMode.FindComponent(SCR_MapMarkerManagerComponent));
		if (RedProof())
		{
			configComponent = null;
			markerManager = null;
		}

		ctx.NotNull(configComponent, "the game mode carries SCR_MapConfigComponent");
		ctx.NotNull(markerManager, "the game mode carries SCR_MapMarkerManagerComponent");
		if (!configComponent || !markerManager)
			return;

		ctx.EqualStr(FULLSCREEN, configComponent.GetGadgetMapConfig(), "the gadget map opens LF_MapFullscreen.conf");
		ctx.NotNull(markerManager.GetMarkerConfig(), "the marker manager loaded its marker config");

		Resource markerRes = BaseContainerTools.LoadContainer(MARKERS);
		ctx.True(markerRes && markerRes.IsValid(), "LF_MapMarkerConfig.conf loads");
		if (markerRes && markerRes.IsValid())
		{
			SCR_MapMarkerConfig markerConfig = SCR_MapMarkerConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(markerRes.GetResource().ToBaseContainer()));
			ctx.NotNull(markerConfig, "LF_MapMarkerConfig.conf is a SCR_MapMarkerConfig");
			if (markerConfig)
			{
				ctx.True(markerConfig.GetMarkerEntryConfigs() && markerConfig.GetMarkerEntryConfigs().Count() > 0, "the marker config keeps vanilla's placed marker set");
				int dynamic;
				foreach (SCR_MapMarkerEntryConfig entry : markerConfig.GetMarkerEntryConfigs())
				{
					if (SCR_MapMarkerEntryDynamic.Cast(entry))
						dynamic++;
				}

				// The squad-leader entry's InitServerLogic reads the groups manager, which this game mode has
				// not: a dynamic entry crashes the manager's EOnInit (seen 2026-11-17).
				ctx.Equal(0, dynamic, "no dynamic marker entry (squad leader, air support): they need Conflict managers");
			}
		}

		Resource res = BaseContainerTools.LoadContainer(FULLSCREEN);
		if (!res || !res.IsValid())
		{
			ctx.Fail("LF_MapFullscreen.conf does not load: " + FULLSCREEN);
			return;
		}

		SCR_MapConfig config = SCR_MapConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(res.GetResource().ToBaseContainer()));
		ctx.NotNull(config, "LF_MapFullscreen.conf is a SCR_MapConfig");
		if (!config)
			return;

		ctx.Equal(EMapEntityMode.FULLSCREEN, config.m_iMapMode, "it is a fullscreen map config (inherited)");
		ctx.NotNull(config.m_LayersConfig, "it keeps vanilla's zoom layers (inherited)");
		ctx.NotNull(config.m_MapPropsConfig, "it keeps vanilla's map props (inherited)");
		ctx.NotNull(config.m_DescriptorVisibilityConfig, "it keeps vanilla's descriptor visibility, which draws the town names (inherited)");

		ctx.False(HasUIComponent(config, SCR_MapCampaignUI), "no Conflict map UI");
		ctx.False(HasUIComponent(config, SCR_MapTaskListUI), "no task list");
		ctx.False(HasUIComponent(config, SCR_MapJournalUI), "no journal");
		ctx.False(HasUIComponent(config, SCR_MapDebugUI), "no military debug icons");

		ctx.True(HasUIComponent(config, SCR_MapMarkersUI), "the marker UI stays");
		ctx.True(HasUIComponent(config, SCR_MapDrawingUI), "pencil drawing stays");
		ctx.True(HasUIComponent(config, SCR_MapCompassUI), "the map compass stays");
		ctx.True(HasUIComponent(config, SCR_MapRulerUI), "the ruler stays");
		ctx.True(HasUIComponent(config, SCR_MapWatchUI), "the watch stays");
	}
}
