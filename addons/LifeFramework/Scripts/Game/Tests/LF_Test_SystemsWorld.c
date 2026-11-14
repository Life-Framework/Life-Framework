// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_Systems,LF_Test_SystemsWorld,LF_Test_SystemsKeepData`:
// with `bool m_bEnabled = true;` on LF_SystemSwitch the member initializer beat the overlay's `m_bEnabled 0`
// and systems/world-gates failed `- the pack overlay turns the pager off` and `- and the log names the pack: `.
// The initializer is gone (the attribute default is the only default); green.
// #1073 vault and heist block, observed 2026-11-14 with the BankAction offer gate, the LF_Bank.Rob refusal and the SpawnWave gate each removed:
// `- with Robbery off the vault offers nothing (expected 0, got 1)`, `- the cash van box offers nothing (expected 0, got 1)`,
// `- but a heist that ends after Robbery went off spills no bags (expected 0, got 2)`, `- and keeps its stock (expected 3, got 2)`
// and `- a heist robbery is refused while Robbery is off`. Restored; green.
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
	static const ResourceName BANK_VAULT = "{6177818988438527}Prefabs/Props/Roleplay/BankVault/LF_BankVault.et";
	static const ResourceName CASH_VAN_BOX = "{8FABDE609C994BC2}Prefabs/Vehicles/LF_CashVanBox.et";
	static const ResourceName ARMORY_STORE = "{700A3E2CC5C68520}Prefabs/Police/LF_ArmoryStore.et";
	static const ResourceName TILL = "{9D3A51E060EFE982}Prefabs/Props/Commercial/LF_FuelTill_Attended.et";

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
	protected LF_RobAction FindRob(IEntity entity)
	{
		if (!entity)
			return null;

		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_RobAction rob = LF_RobAction.Cast(action);
			if (rob)
				return rob;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives the user the police kit's direct items through the real loadout path. eturn Pagers after.
	protected int GiveDirectPager(IEntity user)
	{
		LF_SpawnLogic spawnLogic = LF_SpawnLogic.GetInstance();
		InventoryStorageManagerComponent storage = InventoryStorageManagerComponent.Cast(user.FindComponent(InventoryStorageManagerComponent));
		if (!spawnLogic || !storage)
			return -1;

		array<ResourceName> kit = {PAGER};
		spawnLogic.SpawnDirectCharacterItems(storage, kit);
		return LF_InventoryUtils.GetAmount(user, PAGER);
	}

	//------------------------------------------------------------------------------------------------
	//! The bank vault and the heist targets (cash van box, armory store) obey Robbery (#1073): the
	//! action hides, a heist drill that ends after the switch went off spills nothing and keeps its stock.
	protected void RunVaults(LF_TestContext ctx, IEntity user)
	{
		Owner(string.Empty);
		IEntity bankVault = LF_Utils.SpawnEntityPrefab(BANK_VAULT, user.GetOrigin());
		IEntity cashVan = LF_Utils.SpawnEntityPrefab(CASH_VAN_BOX, user.GetOrigin());
		IEntity armoryStore = LF_Utils.SpawnEntityPrefab(ARMORY_STORE, user.GetOrigin());
		ctx.True(bankVault != null && cashVan != null && armoryStore != null, "the vault, the cash van box and the armory store spawn");
		if (bankVault && cashVan && armoryStore)
		{
			ctx.True(ShownActions(bankVault, user, LF_BankRobAction) > 0, "with Robbery on the vault offers its robbery");
			ctx.True(ShownActions(cashVan, user, LF_HeistRobAction) > 0, "and the cash van box offers its break-in");
			ctx.True(ShownActions(armoryStore, user, LF_HeistRobAction) > 0, "and the armory store offers its breach");

			Owner("{\"Robbery\":{\"enabled\":false}}");
			ctx.Equal(0, ShownActions(bankVault, user, LF_BankRobAction), "with Robbery off the vault offers nothing");
			ctx.Equal(0, ShownActions(cashVan, user, LF_HeistRobAction), "the cash van box offers nothing");
			ctx.Equal(0, ShownActions(armoryStore, user, LF_HeistRobAction), "and the armory store offers nothing");

			Owner(string.Empty);
			ctx.True(ShownActions(cashVan, user, LF_HeistRobAction) > 0, "on again the cash van box offers its break-in");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(bankVault);
		SCR_EntityHelper.DeleteEntityAndChildren(cashVan);
		SCR_EntityHelper.DeleteEntityAndChildren(armoryStore);

		// A heist whose fuse runs out after Robbery went off pays no haul and keeps its stock.
		string heistId = "systems-world-heist";
		LF_Bank.Configure(heistId, 2, 400, 60, 0, 5);
		LF_Bank.ConfigureHeist(heistId, LF_BankLoot.KIND_BAGS, "", "", false, 3, 0);
		ctx.True(LF_Bank.Rob(heistId, "systems-world-robber"), "a heist with a fuse arms");
		Owner("{\"Robbery\":{\"enabled\":false}}");
		ctx.True(LF_Bank.Advance(heistId, 5), "the fuse still runs out");
		ctx.Equal(0, LF_Bank.GetLooseBagCount(heistId), "but a heist that ends after Robbery went off spills no bags");
		ctx.Equal(3, LF_Bank.GetStock(heistId), "and keeps its stock");
		string refusedId = "systems-world-heist-refused";
		LF_Bank.Configure(refusedId, 2, 400, 60, 0, 5);
		LF_Bank.ConfigureHeist(refusedId, LF_BankLoot.KIND_BAGS, "", "", false, 3, 0);
		ctx.False(LF_Bank.Rob(refusedId, "systems-world-robber-b"), "a heist robbery is refused while Robbery is off");
		ctx.Equal(3, LF_Bank.GetStock(refusedId), "and keeps its stock");
		Owner(string.Empty);
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
		int pagersBefore = LF_InventoryUtils.GetAmount(user, PAGER);
		ctx.Equal(pagersBefore, GiveDirectPager(user), "the loadout spawn gives no pager while Pager is off");

		// A till hold that began before the switch pays nothing when it ends.
		IEntity tillEntity = LF_Utils.SpawnEntityPrefab(TILL, "74 1 74");
		LF_RobAction till = FindRob(tillEntity);
		ctx.True(till != null, "the attended till carries LF_RobAction");
		if (till)
		{
			ctx.Equal(0, till.PayHeld(user), "a held robbery that ends after Robbery went off pays nothing");
			ctx.EqualStr(LF_RobAction.REFUSED_OFF, LF_RobAction.GetLastRefusal(), "and is refused because Robbery is off");
		}

		// On again: everything comes back.
		Owner("{\"Farming\":{\"enabled\":true},\"Robbery\":{\"enabled\":true},\"Pager\":{\"enabled\":true}}");
		ctx.Equal(1, ShownActions(plotEntity, user, LF_FarmPlotAction), "turned on again, the plot offers its action");
		ctx.True(plot.ServerPlant(user) != LF_EFarmPlot.SYSTEM_OFF, "and planting is no longer refused for the switch");
		ctx.Equal(1, ShownActions(pager, user, LF_ReadPagerAction), "and the pager reads again");
		ctx.True(LF_Systems.IsItemAllowed(PAGER), "and the loadout gives it");
		ctx.Equal(pagersBefore + 1, GiveDirectPager(user), "the loadout spawn gives the pager again");
		SCR_EntityHelper.DeleteEntityAndChildren(tillEntity);

		RunVaults(ctx, user);

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

		// The config stream: the server's answer, taken the way a client takes it (OnArrived, as the join
		// snapshot and the change RPC do), wins over this machine's own owner layer, which is gone.
		Owner("{\"Robbery\":{\"enabled\":false}}");
		string payload = LF_ConfigStream.BuildPayload(null, true, null, null, null, null, null, true);
		ctx.True(payload.Contains("\"Robbery\":{\"enabled\":false}"), "the stream carries Robbery off");
		Owner(string.Empty);
		ctx.True(LF_Systems.IsEnabled(LF_RobAction.SYSTEM), "without the owner layer this machine has Robbery on");
		ctx.Equal(LF_EConfigStreamStatus.APPLIED, LF_ConfigStream.OnArrived(LF_ConfigStream.FORMAT_VERSION, payload.Hash(), payload), "the stream is accepted");
		ctx.False(LF_Systems.IsEnabled(LF_RobAction.SYSTEM), "the stream's arrival turns Robbery off here");
		ctx.True(LF_Systems.IsEnabled(LF_Pager.SYSTEM), "and leaves the pager on");

		LF_TestCharacterFixture.Delete(user);
		SCR_EntityHelper.DeleteEntityAndChildren(pager);
		Owner(string.Empty);
	}
}
