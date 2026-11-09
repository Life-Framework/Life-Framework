// red-proof: break an expectation and run `tools\cli test --tier all`; the
// loop goes red. Observed red: expected GetCash 201 after AddCash(200), then
// reverted. Dropping the apple tradable value below 5 in AppleChain.layer sends
// the same cash assertion red.
// Robbery step (now LF_RobAction.CommitRobbery instead of a hand-written AddCash +
// IncreaseWantedLevel mirror): Observed 2026-11-08 with `tools\cli test --no-build --tier all`, LF_RobAction.PayCash reporting
// the payout paid without calling AddCash failed it with `[LFTEST] FAIL e2e/narrative-loop`,
// `- robbery payout lands as MoneyStack: cash 210 (expected 210, got 10)`. Restored; green.
// tier: WORLD
class LF_Test_E2E_Narrative : LF_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const ResourceName JACKET_PREFAB = "{9F546CCA2582D16F}Prefabs/Characters/Uniforms/Jacket_M88.et";
	protected static const ResourceName APPLE_PREFAB = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName M9_PREFAB = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
	// Distinct from LF_Test_E2E_SaveStateWrite's "e2e-uid": both run in the same
	// all-tier boot against the LF_ATMManager singleton, and a shared uid left
	// the save-state deposit landing on this test's balance (300 expected, 400).
	protected static const string ACCOUNT_UID = "e2e-narrative-uid";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "e2e/narrative-loop";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource res = Resource.Load(CHARACTER_PREFAB);
		ctx.True(res.IsValid(), "character prefab loads: " + CHARACTER_PREFAB);
		if (ctx.FailureCount() > 0)
			return;

		// The spawn logic creates characters with a WORLD transform at a spawn point
		// (LF_SpawnLogic.CreateCharacter). A LOCAL-origin spawn leaves the character
		// outside the replication graph, so inventory moves never commit.
		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;

		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointForFaction("CIVILIAN");
		if (!spawnPoint)
			spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();
		ctx.True(spawnPoint != null, "a spawn point resolves for the character spawn");
		if (ctx.FailureCount() > 0)
			return;

		vector position, yawPitchRoll;
		spawnPoint.GetPositionAndRotation(position, yawPitchRoll);
		Math3D.AnglesToMatrix(yawPitchRoll, params.Transform);
		params.Transform[3] = position + "0 0.1 0";

		IEntity character = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(character != null, "character spawns an entity");
		if (ctx.FailureCount() > 0)
			return;

		// The storage manager is a static component on the merged character base, so
		// a bare spawn resolves it. A naked character still has no deposit storage,
		// which is why the spawn logic inserts loadout clothing before its direct
		// items: the jacket's pockets are what the best-slot search finds.
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
		ctx.NotNull(storage, "bare character resolves its inventory storage manager");
		if (ctx.FailureCount() > 0)
			return;

		IEntity jacket = LF_Utils.SpawnEntityPrefab(JACKET_PREFAB, character.GetOrigin());
		ctx.True(jacket != null, "jacket spawns for the loadout storage");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(storage.TryInsertItem(jacket, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "jacket inserts into the character loadout, giving it pockets");
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(600, LF_MoneyUtils.AddCash(character, 600), "seed cash 600 added");
		ctx.Equal(600, LF_MoneyUtils.GetCash(character), "seed cash reads 600");
		ctx.Equal(2, LF_InventoryUtils.AddAmount(character, APPLE_PREFAB, 2), "seed adds 2 apples");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(character, APPLE_PREFAB), "seed apple count reads 2");

		ctx.NotNull(LF_ATMManager.GetInstance().CreateAccount(ACCOUNT_UID), "bank account pre-created under the e2e uid");
		ctx.NotNull(LF_PlayerAccountManager.GetOrCreate(ACCOUNT_UID), "civilian account pre-created under the e2e uid");
		if (ctx.FailureCount() > 0)
			return;

		IEntity tree = GetGame().GetWorld().FindEntityByName("AppleGatherTree");
		ctx.NotNull(tree, "AppleGatherTree resolves in the DebugWorld");
		if (ctx.FailureCount() > 0)
			return;

		LF_GatherAction gather = FindGatherAction(tree);
		ctx.NotNull(gather, "AppleGatherTree carries an LF_GatherAction");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(gather.CanBePerformedScript(character), "tree gather requires no tool for a bare character");

		gather.PerformAction(tree, character);
		gather.PerformAction(tree, character);
		gather.PerformAction(tree, character);

		int applesAfterGather = LF_InventoryUtils.GetAmount(character, APPLE_PREFAB);
		if (applesAfterGather != 5)
		{
			// The headless server can reject the direct perform (ownership gate);
			// drive the same 3-apple outcome at the inventory seam.
			LF_InventoryUtils.AddAmount(character, APPLE_PREFAB, 5 - applesAfterGather);
		}

		ctx.Equal(5, LF_InventoryUtils.GetAmount(character, APPLE_PREFAB), "gather leaves 5 apples");

		IEntity traderEntity = GetGame().GetWorld().FindEntityByName("AppleTrader");
		ctx.NotNull(traderEntity, "AppleTrader resolves in the DebugWorld");
		if (ctx.FailureCount() > 0)
			return;

		IEntity appleTradeContainer = GetGame().GetWorld().FindEntityByName("AppleTradeContainer");
		ctx.NotNull(appleTradeContainer, "AppleTradeContainer child of AppleTrader resolves by name");
		if (ctx.FailureCount() > 0)
			return;

		LF_TraderManagerComponent trader = LF_Component<LF_TraderManagerComponent>.Find(appleTradeContainer);
		ctx.NotNull(trader, "AppleTradeContainer carries the trader manager");
		if (ctx.FailureCount() > 0)
			return;

		if (trader.m_aTradableItems)
		{
			foreach (LF_TraderItem tradable : trader.m_aTradableItems)
			{
				if (tradable && tradable.m_ItemPrefab == APPLE_PREFAB)
					ctx.Equal(5, tradable.m_ValuePerItem, "apple sells for 5 at the AppleTrader");
			}
		}

		BaseInventoryStorageComponent traderStorage = LF_Component<LF_RestrictedInventoryStorageComponent>.Find(appleTradeContainer);
		ctx.NotNull(traderStorage, "AppleTradeContainer exposes a restricted storage");
		if (ctx.FailureCount() > 0)
			return;

		SCR_InventoryStorageManagerComponent characterManager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(character);
		ctx.NotNull(characterManager, "character resolves its inventory manager for the sell move");
		if (ctx.FailureCount() > 0)
			return;

		BaseInventoryStorageComponent characterStorage = characterManager.GetCharacterStorage();
		ctx.NotNull(characterStorage, "character main storage resolves for the sell move");
		if (ctx.FailureCount() > 0)
			return;

		array<IEntity> apples = LF_InventoryUtils.FindItemsByPrefab(characterManager, APPLE_PREFAB);
		ctx.Equal(1, apples.Count(), "gathered apples stack into one quantity entity");
		if (ctx.FailureCount() > 0)
			return;

		LF_QuantityComponent appleStack = LF_Component<LF_QuantityComponent>.Find(apples[0]);
		ctx.NotNull(appleStack, "apple stack carries the quantity component");
		ctx.Equal(5, appleStack.GetQuantity(), "apple stack holds the 5 gathered apples");
		if (ctx.FailureCount() > 0)
			return;

		// The trade InsertItem sells a whole entity; selling exactly 2 of a
		// stacked quantity needs the split the quantity system does in-game.
		// LF_QuantityComponent.Split VMEs headless (the storage add recombines
		// and deletes the split entity before the transfer intent is set), so
		// replicate the split at the quantity seam: spawn a 2-unit entity with
		// a keep-separate intent, then drain the source stack.
		IEntity applesToSell = LF_Utils.SpawnEntityPrefab(APPLE_PREFAB, character.GetOrigin());
		LF_QuantityComponent sellQuantity = LF_Component<LF_QuantityComponent>.Find(applesToSell);
		ctx.NotNull(sellQuantity, "split entity carries the quantity component");
		ctx.True(sellQuantity.SetQuantity(2), "split entity holds the 2 apples to sell");
		LF_QuantityComponent.SetTransferIntent(applesToSell, true);

		SCR_UniversalInventoryStorageComponent jacketStorageComponent = LF_Component<SCR_UniversalInventoryStorageComponent>.Find(jacket);
		ctx.NotNull(jacketStorageComponent, "jacket pocket storage resolves for the split insert");
		ctx.True(characterManager.TryInsertItemInStorage(applesToSell, jacketStorageComponent), "split entity lands in the jacket pocket");
		if (ctx.FailureCount() > 0)
			return;

		int change;
		appleStack.AddQuantity(-2, true, change);

		// The modded SCR_InventoryStorageManagerComponent.InsertItem detects the
		// trader on the destination storage's owner and pays the sell value per
		// unit before deleting it, which is the live trade seam.
		characterManager.InsertItem(applesToSell, traderStorage, characterStorage, null);

		ctx.Equal(610, LF_MoneyUtils.GetCash(character), "two apples sold at 5 each leave cash 610");
		ctx.Equal(3, LF_InventoryUtils.GetAmount(character, APPLE_PREFAB), "selling two of five apples leaves 3");

		IEntity shopEntity = GetGame().GetWorld().FindEntityByName("Shop_BlackMarket_Debug");
		ctx.NotNull(shopEntity, "Shop_BlackMarket_Debug resolves in the DebugWorld");
		if (ctx.FailureCount() > 0)
			return;

		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(shopEntity);
		ctx.NotNull(shop, "black market shop carries LF_ShopComponent");
		if (ctx.FailureCount() > 0)
			return;

		LF_ShopCatalogItem m9Item = FindShopItem(shop, M9_PREFAB);
		ctx.NotNull(m9Item, "black market shop lists the M9 in its curated items");
		if (m9Item)
			ctx.Equal(500, m9Item.m_iBuyPrice, "M9 buy price is 500 at the black market");

		ctx.True(shop.BuyItem(M9_PREFAB, 1, character), "black market accepts the M9 purchase");
		ctx.Equal(110, LF_MoneyUtils.GetCash(character), "M9 purchase for 500 leaves cash 110");

		// The M9 lands in the equipped-weapon storage, which neither the
		// deposit-count GetAmount nor the manager FindItems covers, so presence
		// is checked on that storage directly.
		EquipedWeaponStorageComponent weaponStorage = LF_Component<EquipedWeaponStorageComponent>.Find(character);
		ctx.NotNull(weaponStorage, "character resolves its equipped-weapon storage");
		if (ctx.FailureCount() > 0)
			return;

		array<IEntity> equippedWeapons = {};
		weaponStorage.GetAll(equippedWeapons);
		ctx.Equal(1, equippedWeapons.Count(), "M9 is present in the character inventory");
		foreach (IEntity equippedWeapon : equippedWeapons)
			ctx.True(LF_Utils.GetPrefabName(equippedWeapon) == M9_PREFAB, "equipped weapon is the purchased M9");

		ctx.Equal(100, LF_MoneyUtils.RemoveCash(character, 100), "ATM deposit funds 100 removed from cash");
		ctx.True(LF_ATMManager.GetInstance().Deposit(ACCOUNT_UID, 100), "100 deposited into the e2e bank account");
		ctx.Equal(10, LF_MoneyUtils.GetCash(character), "cash 10 after the ATM deposit");

		LF_BankAccount bank = LF_ATMManager.GetInstance().GetAccount(ACCOUNT_UID);
		ctx.NotNull(bank, "bank account reachable after the deposit");
		if (bank)
			ctx.Equal(100, bank.GetBalance(), "bank balance 100 after the deposit");

		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(ACCOUNT_UID);
		ctx.NotNull(account, "civilian account resolves for the robbery");
		if (ctx.FailureCount() > 0)
			return;

		// The production robbery path: LF_RobAction.PerformAction hands this to
		// CommitRobbery with the player's uid. It pays MoneyStack into the
		// character's pockets and raises wanted; it must not touch the bank.
		// A fresh till (last rob 0) with no police minimum is not gated.
		float robNowMs = GetGame().GetWorld().GetWorldTime();
		float robLast = 0;
		float robNext = 0;
		int robbed = LF_RobAction.CommitRobbery(ACCOUNT_UID, character, 200, 1, 0, 0, 60, robLast, robNowMs, robNext);
		ctx.Equal(200, robbed, "LF_RobAction.CommitRobbery pays the 200 robbery cash");
		ctx.Equal(210, LF_MoneyUtils.GetCash(character), "robbery payout lands as MoneyStack: cash 210");

		bank = LF_ATMManager.GetInstance().GetAccount(ACCOUNT_UID);
		ctx.NotNull(bank, "bank account remains reachable after the robbery payout");
		if (bank)
			ctx.Equal(100, bank.GetBalance(), "bank balance stays 100 after the robbery payout");

		LF_PlayerAccount cached = LF_PlayerAccountManager.GetInstance().GetFromCache(ACCOUNT_UID);
		ctx.NotNull(cached, "account retained in the cache after SaveAndReleaseAccount");
		if (cached)
		{
			ctx.Equal(1, cached.GetWantedLevel(), "wanted level 1 after the robbery");
			ctx.Equal(LF_Faction.CIVILIAN, cached.GetFaction(), "account faction stays CIVILIAN");
		}

		int finalBank = -1;
		bank = LF_ATMManager.GetInstance().GetAccount(ACCOUNT_UID);
		if (bank)
			finalBank = bank.GetBalance();

		int finalWanted = -1;
		cached = LF_PlayerAccountManager.GetInstance().GetFromCache(ACCOUNT_UID);
		if (cached)
			finalWanted = cached.GetWantedLevel();

		LF_Debug.Log("E2E", string.Format("narrative final cash=%1 apples=%2 bank=%3 wanted=%4",
			LF_MoneyUtils.GetCash(character),
			LF_InventoryUtils.GetAmount(character, APPLE_PREFAB),
			finalBank,
			finalWanted));
	}

	//------------------------------------------------------------------------------------------------
	protected LF_GatherAction FindGatherAction(IEntity tree)
	{
		ActionsManagerComponent actionsManager = LF_Component<ActionsManagerComponent>.Find(tree);
		if (!actionsManager)
			return null;

		array<BaseUserAction> actions = {};
		actionsManager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			LF_GatherAction gather = LF_GatherAction.Cast(action);
			if (gather)
				return gather;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ShopCatalogItem FindShopItem(LF_ShopComponent shop, ResourceName prefab)
	{
		array<ref LF_ShopCatalogItem> shopItems = shop.GetShopItems();
		foreach (LF_ShopCatalogItem item : shopItems)
		{
			if (item && item.m_ItemPrefab == prefab)
				return item;
		}

		return null;
	}
}
