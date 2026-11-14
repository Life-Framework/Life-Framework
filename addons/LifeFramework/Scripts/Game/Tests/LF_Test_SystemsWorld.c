// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_Systems,LF_Test_SystemsWorld,LF_Test_SystemsKeepData`:
// with `bool m_bEnabled = true;` on LF_SystemSwitch the member initializer beat the overlay's `m_bEnabled 0`
// and systems/world-gates failed `- the pack overlay turns the pager off` and `- and the log names the pack: `.
// The initializer is gone (the attribute default is the only default); green.
//------------------------------------------------------------------------------------------------
//! A stand-in pack source: one LF_SystemsConfig overlay that turns the Pager off.
class LF_Test_SystemsPackStub : LF_ConfigPackSource
{
	static const ResourceName OVERLAY = "{F20A406C42E30D08}Configs/Tests/LF_Test_SystemsOverlay.conf";

	override void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		overlays.Insert(OVERLAY);
	}

	override string GetName()
	{
		return "systems-test-stub";
	}
};

//------------------------------------------------------------------------------------------------
//! Systems (#960) on the shipped catalog and real world objects. With Farming, Robbery and Pager
//! turned off in the owner config, the DebugWorld farm plot offers no action and refuses to plant,
//! a weapon or till robbery refuses with nothing given, the till hold refuses, a pager offers no
//! read action and the police loadout leaves the pager out. Turned on again, all of it comes back.
//! A pack overlay turns the pager off and the owner turns it back on; the config stream carries
//! the server's answer to a client.
// tier: WORLD
class LF_Test_SystemsWorld : LF_Test
{
	static const ResourceName PAGER = "{747E7E1669676AA1}Prefabs/Items/Roleplay/LF_Pager.et";
	static const ResourceName WEAPON = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";

	override string GetName()
	{
		return "systems/world-gates";
	}

	//------------------------------------------------------------------------------------------------
	protected void Owner(string switchesJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		if (!switchesJson.IsEmpty())
			LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"systems\":{\"switches\":" + switchesJson + "}}", "test");
		LF_Systems.Reload();
	}

	//------------------------------------------------------------------------------------------------
	//! How many actions of one class an entity offers this user right now.
	protected int ShownActions(IEntity entity, IEntity user, typename actionType)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return -1;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		int shown;
		foreach (BaseUserAction action : actions)
		{
			ScriptedUserAction scripted = ScriptedUserAction.Cast(action);
			if (scripted && scripted.IsInherited(actionType) && scripted.CanBeShownScript(user))
				shown++;
		}

		return shown;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The shipped catalog with no layer: the three switchable systems are on.
		Owner(string.Empty);
		ctx.True(LF_Systems.IsEnabled(LF_FarmPlots.SYSTEM), "Farming is on by default");
		ctx.True(LF_Systems.IsEnabled(LF_RobAction.SYSTEM), "Robbery is on by default");
		ctx.True(LF_Systems.IsEnabled(LF_Pager.SYSTEM), "Pager is on by default");
		ctx.True(LF_Systems.IsItemAllowed(PAGER), "the police loadout gives the pager");

		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		IEntity pager = LF_Utils.SpawnEntityPrefab(PAGER, vector.Zero);
		LF_FarmPlotComponent plot = LF_FarmPlots.Find("farm_plot_1");
		ctx.NotNull(plot, "farm plot 1 is in the DebugWorld");
		ctx.NotNull(pager, "a pager spawns");
		if (!user || !plot || !pager)
		{
			LF_TestCharacterFixture.Delete(user);
			SCR_EntityHelper.DeleteEntityAndChildren(pager);
			Owner(string.Empty);
			return;
		}

		IEntity plotEntity = plot.GetOwner();
		ctx.Equal(1, ShownActions(plotEntity, user, LF_FarmPlotAction), "with Farming on the plot offers one action (plant or harvest)");
		ctx.Equal(1, ShownActions(pager, user, LF_ReadPagerAction), "with Pager on the pager offers its read action");

		// The owner turns the three systems off.
		Owner("{\"Farming\":{\"enabled\":false},\"Robbery\":{\"enabled\":false},\"Pager\":{\"enabled\":false}}");
		ctx.False(LF_Systems.IsEnabled(LF_FarmPlots.SYSTEM), "the owner turns Farming off");
		ctx.Equal(0, ShownActions(plotEntity, user, LF_FarmPlotAction), "the plot offers no action");
		ctx.Equal(LF_EFarmPlot.SYSTEM_OFF, plot.ServerPlant(user), "planting is refused on the server");
		ctx.Equal(LF_EFarmPlot.SYSTEM_OFF, plot.ServerHarvest(user), "harvesting is refused on the server");
		ctx.EqualStr("#LF-Systems_Off", LF_FarmPlotRules.RefusalKey(LF_EFarmPlot.SYSTEM_OFF), "the refusal has words");

		int weapons = LF_InventoryUtils.GetAmount(user, WEAPON);
		ctx.False(LF_RobWeaponAction.CommitWeaponRobbery("systems-test-robber", user, WEAPON), "a weapon robbery is refused on the server");
		ctx.Equal(weapons, LF_InventoryUtils.GetAmount(user, WEAPON), "and no weapon is given");
		float next;
		ctx.Equal(0, LF_RobAction.CommitRobbery("systems-test-robber", null, 100, 5, 0, 0, 0, 100000, next), "a till robbery pays nothing");
		ctx.EqualStr(LF_RobAction.REFUSED_OFF, LF_RobAction.GetLastRefusal(), "and is refused because Robbery is off");
		ctx.EqualStr(LF_RobAction.REFUSED_OFF, LF_RobHold.Refusal(user, null), "the till hold is refused before anything else");
		ctx.EqualStr("#LF-Systems_Off", LF_RobHoldRules.RefusalKey(LF_RobAction.REFUSED_OFF), "and the attendant's caption says so");

		ctx.Equal(0, ShownActions(pager, user, LF_ReadPagerAction), "a pager already carried offers no read action");
		ctx.False(LF_Systems.IsItemAllowed(PAGER), "the police loadout leaves the pager out");

		// On again: everything comes back.
		Owner("{\"Farming\":{\"enabled\":true},\"Robbery\":{\"enabled\":true},\"Pager\":{\"enabled\":true}}");
		ctx.Equal(1, ShownActions(plotEntity, user, LF_FarmPlotAction), "turned on again, the plot offers its action");
		ctx.True(plot.ServerPlant(user) != LF_EFarmPlot.SYSTEM_OFF, "and planting is no longer refused for the switch");
		ctx.Equal(1, ShownActions(pager, user, LF_ReadPagerAction), "and the pager reads again");
		ctx.True(LF_Systems.IsItemAllowed(PAGER), "and the loadout gives it");

		// A pack overlay turns the pager off; the owner turns it back on over the pack.
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ConfigService.SetPackSource(new LF_Test_SystemsPackStub());
		LF_Systems.Reload();
		ctx.False(LF_Systems.IsEnabled(LF_Pager.SYSTEM), "the pack overlay turns the pager off");
		ctx.True(LF_Systems.WhyOff(LF_Pager.SYSTEM).Contains("pack"), "and the log names the pack: " + LF_Systems.WhyOff(LF_Pager.SYSTEM));
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"systems\":{\"switches\":{\"Pager\":{\"enabled\":true}}}}", "test");
		LF_ConfigService.SetPackSource(new LF_Test_SystemsPackStub());
		LF_Systems.Reload();
		ctx.True(LF_Systems.IsEnabled(LF_Pager.SYSTEM), "the owner's value wins over the pack's");

		// The config stream: the server's answer reaches a client that has no owner file.
		Owner("{\"Robbery\":{\"enabled\":false}}");
		string payload = LF_ConfigStream.BuildPayload(null, true, null, null, null, null, true);
		ctx.True(payload.Contains("\"Robbery\":{\"enabled\":false}"), "the stream carries Robbery off");
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		ctx.Equal(LF_EConfigStreamStatus.APPLIED, LF_ConfigStream.Receive(LF_ConfigStream.FORMAT_VERSION, payload.Hash(), payload), "the client accepts the stream");
		LF_Systems.Reload();
		ctx.False(LF_Systems.IsEnabled(LF_RobAction.SYSTEM), "the client has Robbery off from the stream");
		ctx.True(LF_Systems.IsEnabled(LF_Pager.SYSTEM), "and the pager on");

		LF_TestCharacterFixture.Delete(user);
		SCR_EntityHelper.DeleteEntityAndChildren(pager);
		Owner(string.Empty);
	}
}
