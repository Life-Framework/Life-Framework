// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_SystemsEconomy`: with the SpawnWave gate in LF_Bank, the AddXp gate in LF_JobLevels, the Whitelist check in IsFactionRestricted and the Buy gate in LF_ShopComponent each removed it failed
// `- but a drill that ends after Robbery went off pays no loot (expected 0, got 2)`, `- with Level off, no xp is added (expected 0, got 10)`,
// `- with Whitelist off, nothing is restricted` and `- a buy is refused on the server (expected 17, got 13)`. Restored; green.
//------------------------------------------------------------------------------------------------
//! The economy switches (#1073, #1074) on the shipped catalog and real world objects. With Robbery,
//! Banking, ATM, Shop, Catalogue, Jobs, Level, License, Whitelist and Organizations turned off in the
//! owner config, the vault, ATM, counter and desk offer nothing, every server path refuses and
//! changes no saved value; turned on again it all works. A vault that was mid-drill when Robbery went
//! off pays no loot. ATM and Organizations follow Banking.
// tier: WORLD
class LF_Test_SystemsEconomy : LF_Test
{
	static const string UID = "systems-economy-tester";
	static const ResourceName GUN_COUNTER = "{F7EEC900E44C3CCA}Prefabs/Props/Commercial/LF_GunCounter.et";
	static const ResourceName TRADER = "{5DDBFA4225659FFE}Prefabs/Trader/UniversalTrader.et";
	static const ResourceName PISTOL = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";

	override string GetName()
	{
		return "systems/economy-gates";
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
		if (!entity)
			return -1;

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
	protected IEntity Named(string name, IEntity user)
	{
		IEntity entity = GetGame().GetWorld().FindEntityByName(name);
		if (entity && user)
			user.SetOrigin(entity.GetOrigin());

		return entity;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Owner(string.Empty);
		ctx.True(LF_Systems.IsEnabled(LF_ATMManager.SYSTEM), "Banking is on by default");
		ctx.True(LF_Systems.IsEnabled(LF_ATMAction.SYSTEM), "ATM is on by default");
		ctx.True(LF_Systems.IsEnabled(LF_ShopRefusal.SYSTEM), "Shop is on by default");
		ctx.True(LF_Systems.IsEnabled(LF_ItemCatalogue.SYSTEM), "Catalogue is on by default");
		ctx.True(LF_Systems.IsEnabled(LF_OpenTraderAction.SYSTEM), "Trader is on by default");
		ctx.True(LF_Systems.IsEnabled(LF_CivilianJob.SYSTEM), "Jobs is on by default");
		ctx.True(LF_Systems.IsEnabled(LF_JobLevels.SYSTEM), "Level is on by default");
		ctx.True(LF_Systems.IsEnabled(LF_LicenseGate.SYSTEM), "License is on by default");
		ctx.True(LF_Systems.IsEnabled(LF_WhitelistManager.SYSTEM), "Whitelist is on by default");
		ctx.True(LF_Systems.IsEnabled(LF_OrgRules.SYSTEM), "Organizations is on by default");

		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		if (!user)
			return;

		RunRobbery(ctx, user);
		RunBanking(ctx, user);
		RunShop(ctx, user);
		RunTrader(ctx, user);
		RunJobs(ctx, user);
		RunLevel(ctx);
		RunLicense(ctx, user);
		RunWhitelist(ctx);
		RunOrganizations(ctx, user);

		LF_TestCharacterFixture.Delete(user);
		Owner(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	//! Bank vault and heist robbery obey Robbery (#1073), a drill running when it goes off pays nothing.
	protected void RunRobbery(LF_TestContext ctx, IEntity user)
	{
		Owner(string.Empty);
		string vaultId = "systems-economy-vault";
		LF_Bank.Configure(vaultId, 2, 400, 60, 0, 0);
		Owner("{\"Robbery\":{\"enabled\":false}}");
		ctx.False(LF_Bank.Rob(vaultId, UID), "a vault robbery is refused while Robbery is off");
		ctx.Equal(0, LF_Bank.GetSpawnedBagCount(vaultId), "and spawns nothing");
		Owner(string.Empty);
		ctx.True(LF_Bank.Rob(vaultId, UID), "on again, the same vault opens");
		ctx.Equal(2, LF_Bank.GetSpawnedBagCount(vaultId), "and spawns its bags");

		string fuseVault = "systems-economy-fuse";
		LF_Bank.Configure(fuseVault, 2, 400, 60, 0, 5);
		ctx.True(LF_Bank.Rob(fuseVault, UID + "-b"), "a drill with a fuse arms");
		Owner("{\"Robbery\":{\"enabled\":false}}");
		ctx.True(LF_Bank.Advance(fuseVault, 5), "the fuse still runs out");
		ctx.Equal(0, LF_Bank.GetSpawnedBagCount(fuseVault), "but a drill that ends after Robbery went off pays no loot");

		// The annex example vault (core); the town vault kit is the pack's (#323).
		IEntity vault = Named(LF_TestAnnex.BANK_VAULT, user);
		IEntity kit = Named(LF_TestAnnex.BANK_ROB_KIT, user);
		Owner(string.Empty);
		int offered = ShownActions(vault, user, LF_BankRobAction);
		ctx.True(offered > 0, "with Robbery on the vault offers its robbery action");
		if (kit && LF_BankRobKit.Enabled())
			ctx.True(ShownActions(kit, user, LF_BankRobKitAction) > 0, "and the rob kit stand offers the kit");

		user.SetOrigin(vault.GetOrigin());
		Owner("{\"Robbery\":{\"enabled\":false}}");
		ctx.Equal(0, ShownActions(vault, user, LF_BankRobAction), "with Robbery off the vault (and a heist target) offers nothing");
		ctx.Equal(0, ShownActions(kit, user, LF_BankRobKitAction), "and the rob kit stand offers no kit");
		Owner(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	//! Banking (the ledger) and ATM (the machines on it, which need Banking).
	protected void RunBanking(LF_TestContext ctx, IEntity user)
	{
		Owner(string.Empty);
		LF_ATMManager atm = LF_ATMManager.GetInstance();
		ctx.NotNull(atm, "the account ledger exists");
		LF_BankAccount account = LF_ATMManager.GetOrCreate(UID);
		ctx.NotNull(account, "an account opens");
		if (!atm || !account)
			return;

		int balance = account.GetBalance();
		IEntity machine = Named(LF_TestAnnex.ATM, user);
		ctx.True(ShownActions(machine, user, LF_ATMAction) > 0, "with ATM on the machine offers its screen");

		// Banking alone cannot go off while ATM or Organizations need it (#279): the owner switches those too.
		Owner("{\"Banking\":{\"enabled\":false}}");
		ctx.True(LF_Systems.IsEnabled(LF_ATMManager.SYSTEM), "Banking stays on while ATM needs it");
		Owner("{\"Banking\":{\"enabled\":false},\"ATM\":{\"enabled\":false},\"Organizations\":{\"enabled\":false}}");
		ctx.False(atm.Deposit(UID, 100), "a deposit is refused while Banking is off");
		ctx.False(atm.Withdraw(UID, 100), "and so is a withdrawal");
		ctx.Equal(balance, account.GetBalance(), "the saved balance is untouched");
		ctx.False(LF_Systems.IsEnabled(LF_ATMAction.SYSTEM), "ATM is off with it");
		ctx.Equal(0, ShownActions(machine, user, LF_ATMAction), "the machine offers nothing");
		ctx.Equal(LF_EATMRefusal.SYSTEM_OFF, LF_CharacterATMComponent.ServeDepositReason(user, account, 100, UID), "an ATM deposit is refused with the caption");
		ctx.EqualStr("#LF-Systems_Off", LF_ATMRefusal.Key(LF_EATMRefusal.SYSTEM_OFF), "and the screen has words for it");

		// ATM off on its own: the ledger stays open, the machine closes.
		Owner("{\"ATM\":{\"enabled\":false}}");
		ctx.True(LF_Systems.IsEnabled(LF_ATMManager.SYSTEM), "Banking stays on when only ATM is off");
		ctx.Equal(0, ShownActions(machine, user, LF_ATMAction), "the machine offers nothing");
		ctx.Equal(LF_EATMRefusal.SYSTEM_OFF, LF_CharacterATMComponent.ServeWithdrawReason(user, account, 100, UID), "an ATM withdrawal is refused");
		ctx.Equal(balance, account.GetBalance(), "the balance is untouched");

		Owner(string.Empty);
		ctx.True(atm.Deposit(UID, 100), "on again, a deposit works");
		ctx.Equal(balance + 100, account.GetBalance(), "and moves the balance");
		ctx.True(atm.Withdraw(UID, 100), "and so does a withdrawal");
		Owner(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	//! Shop (counters, the shutter, the till) and Catalogue (outlet gates and stock).
	protected void RunShop(LF_TestContext ctx, IEntity user)
	{
		Owner(string.Empty);
		IEntity counter = LF_Utils.SpawnEntityPrefab(GUN_COUNTER, user.GetOrigin());
		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(counter);
		ctx.NotNull(shop, "the gun counter has a shop");
		if (!shop)
			return;

		ctx.True(ShownActions(counter, user, LF_ShopAction) > 0, "with Shop on the counter offers its price list");
		ctx.True(shop.GetShopItems().Count() > 0, "with Catalogue on the outlet stocks items");

		Owner("{\"Catalogue\":{\"enabled\":false}}");
		ctx.Equal(0, shop.GetShopItems().Count(), "with Catalogue off the outlet stocks nothing");
		ctx.True(ShownActions(counter, user, LF_ShopAction) > 0, "but the counter itself is a Shop matter");

		Owner("{\"Shop\":{\"enabled\":false}}");
		ctx.Equal(0, ShownActions(counter, user, LF_ShopAction), "with Shop off the counter offers nothing");
		int charged;
		string missing;
		ctx.Equal(LF_EShopRefusal.SYSTEM_OFF, shop.Buy(PISTOL, 1, user, charged, missing), "a buy is refused on the server");
		ctx.Equal(LF_EShopRefusal.SYSTEM_OFF, shop.Sell(PISTOL, 1, user, charged, missing), "and so is a sell");
		ctx.Equal(0, charged, "nothing is charged");
		ctx.EqualStr("#LF-Systems_Off", LF_ShopRefusal.Key(LF_EShopRefusal.SYSTEM_OFF), "the slip has words for it");
		ctx.True(LF_ShopRefusal.FromWire(LF_EShopRefusal.SYSTEM_OFF) == LF_EShopRefusal.SYSTEM_OFF, "and the wire keeps it");

		Owner(string.Empty);
		ctx.True(shop.Buy(PISTOL, 1, user, charged, missing) != LF_EShopRefusal.SYSTEM_OFF, "on again, a buy is no longer refused for the switch");
		SCR_EntityHelper.DeleteEntityAndChildren(counter);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunTrader(LF_TestContext ctx, IEntity user)
	{
		Owner(string.Empty);
		IEntity trader = LF_Utils.SpawnEntityPrefab(TRADER, user.GetOrigin());
		ctx.True(ShownActions(trader, user, LF_OpenTraderAction) > 0, "with Trader on the stall offers its storage");
		Owner("{\"Trader\":{\"enabled\":false}}");
		ctx.Equal(0, ShownActions(trader, user, LF_OpenTraderAction), "with Trader off it offers nothing");
		SCR_EntityHelper.DeleteEntityAndChildren(trader);
		Owner(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunJobs(LF_TestContext ctx, IEntity user)
	{
		Owner(string.Empty);
		IEntity depot = Named(LF_TestAnnex.JOB_DEPOT, user);
		int offered = ShownActions(depot, user, ScriptedUserAction);
		ctx.True(offered > 0, "with Jobs on the depot offers its actions");

		int cash = LF_MoneyUtils.GetCash(user);
		Owner("{\"Jobs\":{\"enabled\":false}}");
		ctx.Equal(0, ShownActions(depot, user, LF_JobSignUpAction) + ShownActions(depot, user, LF_JobStartRunAction) + ShownActions(depot, user, LF_JobDropoffAction) + ShownActions(depot, user, LF_JobAbandonAction), "with Jobs off the depot offers no job action");
		LF_CivilianJob.HandCash(user, 50);
		ctx.Equal(cash, LF_MoneyUtils.GetCash(user), "a run payout hands over nothing");

		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(user);
		if (jobs)
		{
			ctx.False(jobs.RequestJobFromClient(LF_EJobType.UNEMPLOYED), "a job request is refused");
			ctx.Equal(0, jobs.StartFruitCatcherRound(EHarvestJobType.APPLES), "and so is a fruit round");
		}

		Owner(string.Empty);
		LF_CivilianJob.HandCash(user, 50);
		ctx.Equal(cash + 50, LF_MoneyUtils.GetCash(user), "on again, the payout is handed over");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLevel(LF_TestContext ctx)
	{
		LF_JobsConfig.UseForTest(LF_Test_JobLevels.MakeConfig());
		LF_JobLevels.Forget(UID);
		Owner(string.Empty);
		ctx.Equal(90, LF_JobLevels.AddXp(UID, "WOOD", 90, "test"), "with Level on, work earns xp");

		Owner("{\"Level\":{\"enabled\":false}}");
		ctx.Equal(0, LF_JobLevels.AddXp(UID, "WOOD", 10, "test"), "with Level off, no xp is added");
		ctx.Equal(0, LF_JobLevels.Award(UID, "chop"), "and no award is paid");
		ctx.Equal(90, LF_JobLevels.XpOf(UID, "WOOD"), "saved xp is untouched");

		Owner(string.Empty);
		ctx.Equal(10, LF_JobLevels.AddXp(UID, "WOOD", 10, "test"), "on again, xp is earned from where it was");
		LF_JobLevels.Forget(UID);
		LF_JobsConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLicense(LF_TestContext ctx, IEntity user)
	{
		Owner(string.Empty);
		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(user);
		ctx.NotNull(licences, "the character carries a licence manager");
		if (!licences)
			return;

		Owner("{\"License\":{\"enabled\":false}}");
		ctx.False(licences.UnlockLicense(LF_ELicenseType.FIREARM_PERMIT, true), "with License off, no licence is granted");
		ctx.False(licences.HasLicense(LF_ELicenseType.FIREARM_PERMIT), "and none is held");
		ctx.False(licences.CanUnlockLicense(LF_ELicenseType.FIREARM_PERMIT), "and none can be unlocked");

		Owner(string.Empty);
		ctx.True(licences.UnlockLicense(LF_ELicenseType.FIREARM_PERMIT, true), "on again, it is granted");
		Owner("{\"License\":{\"enabled\":false}}");
		ctx.True(licences.HasLicense(LF_ELicenseType.FIREARM_PERMIT), "a licence held before the switch stays held");
		Owner(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWhitelist(LF_TestContext ctx)
	{
		Owner(string.Empty);
		ctx.True(LF_WhitelistManager.IsFactionRestricted(LF_EFactionType.POLICE), "with Whitelist on, police is restricted");
		ctx.False(LF_WhitelistManager.MayJoinFaction("systems-economy-nobody", LF_EFactionType.POLICE), "and a player not on the list may not join");

		Owner("{\"Whitelist\":{\"enabled\":false}}");
		ctx.False(LF_WhitelistManager.IsFactionRestricted(LF_EFactionType.POLICE), "with Whitelist off, nothing is restricted");
		ctx.True(LF_WhitelistManager.MayJoinFaction("systems-economy-nobody", LF_EFactionType.POLICE), "and anyone may join");
		ctx.True(LF_WhitelistManager.IsWhitelistedForJob("systems-economy-nobody", LF_EJobType.POLICE), "or take the police job");

		Owner(string.Empty);
		ctx.False(LF_WhitelistManager.MayJoinFaction("systems-economy-nobody", LF_EFactionType.POLICE), "on again, the list is back");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunOrganizations(LF_TestContext ctx, IEntity user)
	{
		Owner(string.Empty);
		IEntity desk = Named(LF_TestAnnex.ORG_DESK, user);
		ctx.True(ShownActions(desk, user, LF_OrganizationDeskAction) > 0, "with Organizations on the desk offers its ledger");

		Owner("{\"Organizations\":{\"enabled\":false}}");
		ctx.Equal(0, ShownActions(desk, user, LF_OrganizationDeskAction), "with Organizations off the desk offers nothing");
		LF_OrgInviteRole invite = LF_OrgInviteRole.Cast(LF_PlayerInteraction.GetRole(LF_OrgInviteRole.SLOT));
		if (invite)
			ctx.False(invite.CanOffer(user, user), "and the invite role offers nothing");

		// Organizations needs Banking: Banking cannot be turned off under it.
		Owner("{\"Banking\":{\"enabled\":false}}");
		ctx.True(LF_Systems.IsEnabled(LF_ATMManager.SYSTEM), "Banking cannot go off while Organizations is on");

		Owner(string.Empty);
		ctx.True(ShownActions(desk, user, LF_OrganizationDeskAction) > 0, "on again, the desk offers its ledger");
	}
}
