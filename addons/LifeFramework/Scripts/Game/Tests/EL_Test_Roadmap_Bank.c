// red-proof: expected bank balance 1 after Rob and ran `tools\cli test --no-build --tier fast`
// on 2026-10-11. The suite reported
// `[ELTEST] - payout is cash and the bank balance stays 0 (expected 1, got 0)`.
// Restored the expected value to 0.

// tier: LOGIC
class EL_Test_Roadmap_Bank : EL_Test
{
	override string GetName()
	{
		return "roadmap/bank";
	}

	override void Run(EL_TestContext ctx)
	{
		string uid = "roadmap-bank-logic";
		string policeUid = "roadmap-bank-logic-police";
		string officerUid = "roadmap-bank-logic-officer";
		string shortUid = "roadmap-bank-logic-short";
		string fuseUid = "roadmap-bank-logic-fuse";
		string vaultId = "logic-bank-vault";
		string officerVault = "logic-bank-officer";
		string shortVault = "logic-bank-short";
		string fuseVault = "logic-bank-fuse";

		ctx.False(LF_Bank.Rob("missing-bank-vault", uid), "unconfigured vault refuses");
		ctx.Equal(2, LF_Bank.CappedBagCount(2, 50), "over-capacity spawn is capped by config");
		ctx.Equal(1, LF_Bank.CappedBagCount(2, 1), "a smaller request stays under the config");
		ctx.Equal(0, LF_Bank.CappedBagCount(2, -1), "a negative request spawns nothing");
		ctx.True(LF_Bank.CooldownBlocks(60, 59), "fifty-nine seconds is inside a sixty second cooldown");
		ctx.False(LF_Bank.CooldownBlocks(60, 60), "sixty seconds elapses a sixty second cooldown");
		ctx.False(LF_Bank.CooldownBlocks(0, 0), "a zero cooldown does not block");
		ctx.True(LF_Bank.CivilianAllowed(null), "a missing account passes the civilian gate");
		ctx.False(LF_Bank.PoliceOnDutyAllows(0, 2), "zero police does not meet a minimum of two");
		ctx.True(LF_Bank.PoliceOnDutyAllows(2, 2), "two police meets a minimum of two");

		EL_PlayerAccount officer = EL_PlayerAccountManager.GetOrCreate(officerUid);
		officer.SetFaction(EL_Faction.POLICE);
		ctx.False(LF_Bank.CivilianAllowed(officer), "police fail the civilian gate");

		LF_Bank.Configure(vaultId, 2, 400, 60, 0, 0, "");
		ctx.Equal(2, LF_Bank.GetConfiguredBagCount(vaultId), "configured bag count is 2");
		ctx.Equal(400, LF_Bank.GetCashPerBag(vaultId), "configured cash per bag is 400");

		EL_BankAccount bank = EL_ATMManager.GetOrCreate(uid);
		ctx.NotNull(bank, "robber bank account exists");
		bank.SetBalance(0);
		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
		ctx.NotNull(account, "robber account exists");
		ctx.Equal(EL_Faction.CIVILIAN, account.GetFaction(), "robber starts civilian");
		ctx.Equal(0, account.GetWantedLevel(), "robber starts clean");

		ctx.False(LF_Bank.Rob(vaultId, ""), "empty player id refuses");
		ctx.True(LF_Bank.Rob(vaultId, uid), "zero fuse robbery succeeds");
		ctx.Equal(2, LF_Bank.GetSpawnedBagCount(vaultId), "spawned bag count matches config");
		ctx.Equal(400, LF_Bank.GetBagCash(vaultId, 0), "first bag cash matches config");
		ctx.Equal(400, LF_Bank.GetBagCash(vaultId, 1), "second bag cash matches config");
		ctx.True(LF_Bank.IsBagOnGround(vaultId, 0), "a new bag starts on the ground");
		ctx.Equal(1, account.GetWantedLevel(), "wanted rises once");
		ctx.Equal(1, LF_Breach.GetPoliceNotifyCount(vaultId), "police notify fires once from the breach");
		ctx.Equal(0, bank.GetBalance(), "payout is cash and the bank balance stays 0");

		int spawned = LF_Bank.GetSpawnedBagCount(vaultId);
		ctx.False(LF_Bank.Rob(vaultId, uid), "second breach inside the cooldown refuses");
		ctx.Equal(spawned, LF_Bank.GetSpawnedBagCount(vaultId), "second breach inside the cooldown spawns nothing");
		ctx.Equal(1, account.GetWantedLevel(), "cooldown refusal does not raise wanted");
		ctx.Equal(1, LF_Breach.GetPoliceNotifyCount(vaultId), "cooldown refusal does not notify again");
		ctx.Equal(0, bank.GetBalance(), "cooldown refusal leaves the bank balance at 0");

		ctx.True(LF_Bank.CarryBag(vaultId, 0, uid), "the bag is carried in hand");
		ctx.False(LF_Bank.IsBagOnGround(vaultId, 0), "a carried bag is not on the ground");
		ctx.Equal(400, LF_Bank.GetBagCash(vaultId, 0), "cash stays on the bag in hand");
		ctx.Equal(0, bank.GetBalance(), "carrying the bag does not pay the bank");
		ctx.True(LF_Bank.DropBag(vaultId, 0), "dropping the bag leaves it in the world");
		ctx.True(LF_Bank.IsBagOnGround(vaultId, 0), "a dropped bag is world loot");
		ctx.Equal(400, LF_Bank.GetBagCash(vaultId, 0), "dropped cash stays on the bag");
		ctx.Equal(0, bank.GetBalance(), "dropping the bag does not pay the bank");

		LF_BankRecord saved = LF_Bank.Export(vaultId);
		ctx.NotNull(saved, "export returns the ground bags");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(2, saved.m_aBags.Count(), "export keeps both bags");
		ctx.True(LF_Bank.CarryBag(vaultId, 0, uid), "live state picks the bag up before apply");
		ctx.False(LF_Bank.Apply(LF_BankRecord.Create("other-bank")), "a record for an unknown vault is rejected");
		ctx.True(LF_Bank.Apply(saved), "apply restores the saved bags");
		ctx.True(LF_Bank.IsBagOnGround(vaultId, 0), "applied bag is world loot again");
		ctx.Equal(400, LF_Bank.GetBagCash(vaultId, 0), "applied bag keeps its cash");
		ctx.Equal(0, bank.GetBalance(), "apply does not create a bank balance");
		ctx.True(LF_Bank.Apply(saved), "apply is idempotent");
		ctx.Equal(2, LF_Bank.GetSpawnedBagCount(vaultId), "a second apply does not duplicate bags");
		ctx.Equal(0, bank.GetBalance(), "a second apply still leaves the bank at 0");

		LF_Bank.Advance(vaultId, 60);
		EL_PlayerAccount police = EL_PlayerAccountManager.GetOrCreate(policeUid);
		police.SetFaction(EL_Faction.POLICE);
		ctx.True(LF_Breach.Repair(vaultId, policeUid), "police can lock the vault again");
		ctx.True(LF_Bank.Rob(vaultId, uid), "a breach after the cooldown refills the vault");
		ctx.Equal(4, LF_Bank.GetSpawnedBagCount(vaultId), "the refill spawns another configured wave");
		ctx.Equal(400, LF_Bank.GetBagCash(vaultId, 2), "refilled bags use the configured cash");
		ctx.Equal(0, bank.GetBalance(), "the refill does not pay the bank");
		ctx.Equal(2, account.GetWantedLevel(), "the second heist raises wanted once more");

		LF_Bank.Configure(officerVault, 1, 100, 60, 0, 0, "");
		ctx.False(LF_Bank.Rob(officerVault, officerUid), "a police robber is refused");
		ctx.Equal(0, LF_Bank.GetSpawnedBagCount(officerVault), "a refused officer spawns nothing");
		ctx.Equal(0, officer.GetWantedLevel(), "a refused officer does not take the wanted bump");

		LF_Bank.Configure(shortVault, 1, 100, 60, 2, 0, "");
		ctx.False(LF_Bank.Rob(shortVault, shortUid), "minimum police gate refuses");
		ctx.Equal(0, LF_Bank.GetSpawnedBagCount(shortVault), "a short police count spawns nothing");
		EL_PlayerAccount shortAccount = EL_PlayerAccountManager.GetOrCreate(shortUid);
		ctx.Equal(0, shortAccount.GetWantedLevel(), "a short police count does not raise wanted");

		LF_Bank.Configure(fuseVault, 2, 400, 60, 0, 5, "");
		ctx.True(LF_Bank.Rob(fuseVault, fuseUid), "positive fuse robbery arms");
		ctx.Equal(0, LF_Bank.GetSpawnedBagCount(fuseVault), "bags wait until the vault opens");
		ctx.True(LF_Breach.IsLocked(fuseVault), "the vault stays locked during the fuse");
		ctx.False(LF_Bank.Advance(fuseVault, 4), "four seconds of a five second fuse stays locked");
		ctx.Equal(0, LF_Bank.GetSpawnedBagCount(fuseVault), "an unfinished fuse spawns nothing");
		ctx.True(LF_Bank.Advance(fuseVault, 1), "the fifth second opens the vault");
		ctx.Equal(2, LF_Bank.GetSpawnedBagCount(fuseVault), "opening the vault spawns the configured bags");
		ctx.Equal(400, LF_Bank.GetBagCash(fuseVault, 0), "fuse bags carry the configured cash");
		EL_BankAccount fuseBank = EL_ATMManager.GetOrCreate(fuseUid);
		ctx.Equal(0, fuseBank.GetBalance(), "a fused haul does not pay the bank");
	}
}

// tier: WORLD
class EL_Test_Roadmap_BankWorld : EL_Test
{
	override string GetName()
	{
		return "e2e/roadmap-bank";
	}

	override void Run(EL_TestContext ctx)
	{
		Resource door = Resource.Load(LF_Bank.DOOR_PREFAB);
		ctx.True(door.IsValid(), "EL_Door_Base.et loads");
		Resource shell = Resource.Load(LF_Bank.SHELL_PREFAB);
		ctx.True(shell.IsValid(), "EL_Bank.et loads");
		Resource bagPrefab = Resource.Load(LF_Bank.BAG_PREFAB);
		ctx.True(bagPrefab.IsValid(), "HandCarryItem_Base.et loads");
		Resource money = Resource.Load(EL_MoneyUtils.PREFAB_CASH);
		ctx.True(money.IsValid(), "MoneyStack.et loads");
		Resource crate = Resource.Load(LF_Bank.CRATE_MESH);
		ctx.True(crate.IsValid(), "Crate_01.xob loads");

		IEntity vault = GetGame().GetWorld().FindEntityByName("Roadmap_Bank_Vault");
		ctx.True(vault != null, "Roadmap_Bank_Vault is placed");
		IEntity bag = GetGame().GetWorld().FindEntityByName("Roadmap_Bank_Bag");
		ctx.True(bag != null, "Roadmap_Bank_Bag is placed");
		if (ctx.FailureCount() > 0)
			return;

		ctx.NotNull(EL_Component<EL_HandCarryComponent>.Find(bag), "the bag is a hand-carry item");
		EL_LockComponent doorLock = EL_Component<EL_LockComponent>.Find(vault);
		ctx.NotNull(doorLock, "vault carries EL_LockComponent");
		if (ctx.FailureCount() > 0)
			return;

		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(vault);
		ctx.NotNull(actions, "vault carries ActionsManagerComponent");
		if (ctx.FailureCount() > 0)
			return;

		array<BaseUserAction> actionList = {};
		actions.GetActionsList(actionList);
		bool robWired = false;
		LF_BankRobAction robAction;
		foreach (BaseUserAction action : actionList)
		{
			if (LF_BankRobAction.Cast(action))
			{
				robWired = true;
				robAction = LF_BankRobAction.Cast(action);
			}
		}

		ctx.True(robWired, "rob action is on the vault");
		ctx.Equal(1, LF_Bank.GetConfiguredBagCount("Roadmap_Bank_Vault"), "world vault spawns one bag");
		ctx.Equal(500, LF_Bank.GetCashPerBag("Roadmap_Bank_Vault"), "world vault cash per bag is 500");

		string uid = "roadmap-bank-e2e";
		string policeUid = "roadmap-bank-e2e-police";
		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
		account.SetFaction(EL_Faction.CIVILIAN);
		EL_PlayerAccount police = EL_PlayerAccountManager.GetOrCreate(policeUid);
		police.SetFaction(EL_Faction.POLICE);
		EL_BankAccount bank = EL_ATMManager.GetOrCreate(uid);
		bank.SetBalance(0);

		if (!doorLock.IsLocked())
			LF_Breach.Repair("Roadmap_Bank_Vault", policeUid);

		ctx.True(doorLock.IsLocked(), "vault is locked before the zero-fuse robbery");

		int notifiesBefore = LF_Breach.GetPoliceNotifyCount("Roadmap_Bank_Vault");
		if (robAction)
			robAction.CanBePerformedScript(vault);

		ctx.Equal(notifiesBefore, LF_Breach.GetPoliceNotifyCount("Roadmap_Bank_Vault"), "live offer refresh does not notify");
		ctx.True(LF_Bank.Rob("Roadmap_Bank_Vault", uid), "zero-fuse vault opens");
		ctx.False(doorLock.IsLocked(), "vault IsLocked() is false after the fuse");
		ctx.Equal(1, LF_Bank.GetSpawnedBagCount("Roadmap_Bank_Vault"), "one bag exists");
		ctx.Equal(500, LF_Bank.ReadWorldBagCash(bag), "bag MoneyStack quantity is the config value");
		ctx.NotNull(EL_Component<EL_HandCarryComponent>.Find(bag), "the filled bag is still hand-carry");
		ctx.Equal(0, bank.GetBalance(), "world haul leaves the bank balance at 0");
		ctx.Equal(1, account.GetWantedLevel(), "world haul raises wanted once");

		int cashAfter = LF_Bank.ReadWorldBagCash(bag);
		ctx.False(LF_Bank.Rob("Roadmap_Bank_Vault", uid), "second world breach inside the cooldown refuses");
		ctx.Equal(1, LF_Bank.GetSpawnedBagCount("Roadmap_Bank_Vault"), "second world breach spawns nothing");
		ctx.Equal(cashAfter, LF_Bank.ReadWorldBagCash(bag), "second world breach does not add cash");
		ctx.Equal(0, bank.GetBalance(), "second world breach leaves the bank balance at 0");
	}
}

// tier: PERSISTENCE
class EL_Test_Roadmap_BankPersistence : EL_Test
{
	override string GetName()
	{
		return "roadmap/bank-save";
	}

	override void Run(EL_TestContext ctx)
	{
		IEntity bag = GetGame().GetWorld().FindEntityByName("Roadmap_Bank_Bag");
		ctx.True(bag != null, "Roadmap_Bank_Bag is placed for the save round trip");
		if (ctx.FailureCount() > 0)
			return;

		string uid = "roadmap-bank-save";
		string vaultId = "Roadmap_Bank_Vault";
		EL_BankAccount bank = EL_ATMManager.GetOrCreate(uid);
		bank.SetBalance(0);

		if (LF_Bank.GetSpawnedBagCount(vaultId) == 0)
		{
			EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
			account.SetFaction(EL_Faction.CIVILIAN);
			ctx.True(LF_Bank.Rob(vaultId, uid), "rob leaves a bag on the ground");
		}

		ctx.True(LF_Bank.GetSpawnedBagCount(vaultId) > 0, "a bag exists to save");
		ctx.True(LF_Bank.IsBagOnGround(vaultId, 0), "the saved bag starts on the ground");
		int cash = LF_Bank.GetBagCash(vaultId, 0);
		ctx.Equal(LF_Bank.GetCashPerBag(vaultId), cash, "ground bag cash matches config");
		ctx.Equal(cash, LF_Bank.ReadWorldBagCash(bag), "ground MoneyStack matches the bag record");

		LF_BankRecord saved = LF_Bank.Export(vaultId);
		ctx.NotNull(saved, "export returns the ground bag");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(saved.m_aBags != null && saved.m_aBags.Count() > 0, "export has a bag");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(saved.m_aBags[0].m_bOnGround, "saved record is world loot");
		ctx.True(LF_Bank.CarryBag(vaultId, 0, uid), "live state picks the bag up before apply");
		ctx.False(LF_Bank.IsBagOnGround(vaultId, 0), "live bag is in hand before apply");
		ctx.True(LF_Bank.Apply(saved), "apply accepts the saved bags");
		ctx.True(LF_Bank.IsBagOnGround(vaultId, 0), "ground bag survives apply-save");
		ctx.Equal(cash, LF_Bank.GetBagCash(vaultId, 0), "ground cash survives apply-save");
		ctx.Equal(cash, LF_Bank.ReadWorldBagCash(bag), "world MoneyStack survives apply-save");
		ctx.Equal(0, bank.GetBalance(), "a bag on the ground is not a second bank balance");
		ctx.True(LF_Bank.Apply(saved), "apply is idempotent");
		ctx.Equal(0, bank.GetBalance(), "a second apply still leaves the bank at 0");
		ctx.True(LF_Bank.IsBagOnGround(vaultId, 0), "a second apply stays on the ground");
	}
}
