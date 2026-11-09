// red-proof: observed 2026-11-08, `tools\cli test --no-build --only` on the six tests below, one
// break per test in the code under test, then restored (SUMMARY passed=0 failed=6 total=6):
//   atm/keypad, limit guard off in LF_ATMKeypad.Press:
//     "- a digit past the transaction limit is refused (expected 1000000, got 10000005)"
//   counter/refusal-text, NOT_ENOUGH_BALANCE mapped to #LF-ATM_RefuseCash:
//     "- ATM refusal 3 says something the others do not"
//   counter/local-only, LF_ATMAction.HasLocalEffectOnlyScript returning false:
//     "- the ATM action runs only on the acting player's machine"
//   atm/refusal-reasons, the payout rollback returning NOT_ENOUGH_BALANCE:
//     "- a payout that does not fit says the pockets are full (expected 4, got 3)"
//   shop/refusal-reasons, the empty sale returning NOT_SOLD_HERE:
//     "- selling what you do not carry says so (expected 5, got 1)"
//   counter/screen-layout, LF_UIScreen.AddKeys not remembering its keys:
//     "- the terminal has a DEPOSIT key", "- the keypad has a 0 key", "- the till has a Pay key"

//------------------------------------------------------------------------------------------------
//! The ATM keypad: digits append, the transaction limit holds, DEL drops the last digit.
// tier: LOGIC
class LF_Test_ATMKeypad : LF_Test
{
	override string GetName()
	{
		return "atm/keypad";
	}

	override void Run(LF_TestContext ctx)
	{
		int amount = 0;
		amount = LF_ATMKeypad.Press(amount, 1);
		amount = LF_ATMKeypad.Press(amount, 0);
		amount = LF_ATMKeypad.Press(amount, 0);
		ctx.Equal(100, amount, "keys 1 0 0 make 100");
		ctx.Equal(10, LF_ATMKeypad.Back(amount), "DEL drops the last digit");
		ctx.Equal(0, LF_ATMKeypad.Back(0), "DEL on an empty entry stays at 0");
		ctx.Equal(0, LF_ATMKeypad.Press(0, 0), "a leading zero adds nothing");
		ctx.Equal(7, LF_ATMKeypad.Press(7, 12), "a key that is not a digit changes nothing");

		int limit = LF_ATMManager.MAX_TRANSACTION_AMOUNT;
		ctx.Equal(limit, LF_ATMKeypad.Press(limit / 10, 0), "the entry reaches the transaction limit exactly");
		ctx.Equal(limit, LF_ATMKeypad.Press(limit, 5), "a digit past the transaction limit is refused");

		ctx.Equal(7, LF_ATMKeypad.DigitOf("Key7Button"), "Key7Button is the 7 key");
		ctx.Equal(0, LF_ATMKeypad.DigitOf("Key0Button"), "Key0Button is the 0 key");
		ctx.Equal(-1, LF_ATMKeypad.DigitOf(LF_ATMTerminal.CLEAR), "CLR is not a digit");
		ctx.Equal(-1, LF_ATMKeypad.DigitOf("KeyXButton"), "a letter key is not a digit");
		ctx.Equal(-1, LF_ATMKeypad.DigitOf(LF_ATMTerminal.DEPOSIT), "DEPOSIT is not a digit");
	}
}

//------------------------------------------------------------------------------------------------
//! Every refusal the server can send has its own line; an unknown wire value never reads as "OK".
// tier: LOGIC
class LF_Test_CounterRefusalText : LF_Test
{
	override string GetName()
	{
		return "counter/refusal-text";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("", LF_ATMRefusal.Key(LF_EATMRefusal.NONE), "an accepted ATM request has no refusal line");
		array<string> atmSeen = {};
		for (int reason = LF_EATMRefusal.INVALID_AMOUNT; reason <= LF_EATMRefusal.NO_ACCOUNT; reason++)
		{
			string key = LF_ATMRefusal.Key(reason);
			ctx.False(key.IsEmpty(), "ATM refusal " + reason.ToString() + " has a line");
			ctx.True(atmSeen.Find(key) < 0, "ATM refusal " + reason.ToString() + " says something the others do not");
			atmSeen.Insert(key);
		}

		ctx.Equal(LF_EATMRefusal.NO_ACCOUNT, LF_ATMRefusal.FromWire(99), "an unknown ATM reason is a refusal");
		ctx.Equal(LF_EATMRefusal.NOT_ENOUGH_BALANCE, LF_ATMRefusal.FromWire(LF_EATMRefusal.NOT_ENOUGH_BALANCE), "a known ATM reason survives the wire");

		ctx.EqualStr("", LF_ShopRefusal.Key(LF_EShopRefusal.NONE), "a paid order has no refusal line");
		array<string> shopSeen = {};
		for (int shopReason = LF_EShopRefusal.NOT_SOLD_HERE; shopReason <= LF_EShopRefusal.NO_SHOP; shopReason++)
		{
			string shopKey = LF_ShopRefusal.Key(shopReason);
			ctx.False(shopKey.IsEmpty(), "shop refusal " + shopReason.ToString() + " has a line");
			ctx.True(shopSeen.Find(shopKey) < 0, "shop refusal " + shopReason.ToString() + " says something the others do not");
			shopSeen.Insert(shopKey);
		}

		ctx.Equal(LF_EShopRefusal.NO_SHOP, LF_ShopRefusal.FromWire(-3), "an unknown shop reason is a refusal");

		ctx.Equal(1, LF_ShopRefusal.StepQuantity(1, -1), "the slip never goes below 1");
		ctx.Equal(3, LF_ShopRefusal.StepQuantity(2, 1), "One more adds one");
		ctx.Equal(LF_CharacterShopComponent.MAX_TRANSACTION_QUANTITY, LF_ShopRefusal.StepQuantity(LF_CharacterShopComponent.MAX_TRANSACTION_QUANTITY, 1), "the slip stops at the server's per-order cap");

		ctx.True(LF_ShopPriceList.Matches("Water Bottle", "water"), "the search ignores case");
		ctx.True(LF_ShopPriceList.Matches("Water Bottle", ""), "an empty search shows every line");
		ctx.False(LF_ShopPriceList.Matches("Apple", "water"), "the search hides lines that do not match");
		ctx.Equal(12, LF_ShopPriceList.RowIndex(LF_ShopPriceList.RowName(12)), "a row name maps back to its line");
		ctx.Equal(-1, LF_ShopPriceList.RowIndex(LF_ShopPriceList.PAY), "the Pay key is not a line");
		ctx.Equal(-1, LF_ShopPriceList.RowIndex("ItemXRow"), "a malformed row name is not a line");

		ctx.Equal(2, LF_UIFocus.Column(2, 3), "a keypad move keeps the column");
		ctx.Equal(1, LF_UIFocus.Column(2, 2), "a shorter row lands on its last key");
		ctx.Equal(-1, LF_UIFocus.Column(0, 0), "an empty row has no column");
	}
}

//------------------------------------------------------------------------------------------------
//! The ATM and shop screens open only on the machine of the player who used the counter: both
//! placed actions run PerformAction on the acting machine only, and the owner check refuses
//! anyone else. Reads the DebugWorld placements ATM_Debug and Shop_GeneralStore_Debug.
// tier: WORLD
class LF_Test_CounterLocalOnly : LF_Test
{
	override string GetName()
	{
		return "counter/local-only";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity atm = GetGame().GetWorld().FindEntityByName("ATM_Debug");
		IEntity shop = GetGame().GetWorld().FindEntityByName("Shop_GeneralStore_Debug");
		ctx.NotNull(atm, "DebugWorld places ATM_Debug");
		ctx.NotNull(shop, "DebugWorld places Shop_GeneralStore_Debug");
		if (!atm || !shop)
			return;

		ScriptedUserAction atmAction = FindAction(atm, "LF_ATMAction");
		ScriptedUserAction shopAction = FindAction(shop, "LF_ShopAction");
		ctx.NotNull(atmAction, "ATM_Debug carries LF_ATMAction");
		ctx.NotNull(shopAction, "Shop_GeneralStore_Debug carries LF_ShopAction");
		if (atmAction)
			ctx.True(atmAction.HasLocalEffectOnlyScript(), "the ATM action runs only on the acting player's machine");
		if (shopAction)
			ctx.True(shopAction.HasLocalEffectOnlyScript(), "the shop action runs only on the acting player's machine");

		// Two distinct entities stand in for two players.
		ctx.True(LF_CounterAccess.IsLocalActor(atm, atm), "the player who used the counter gets the screen");
		ctx.False(LF_CounterAccess.IsLocalActor(atm, shop), "another player's use does not open the screen here");
		ctx.False(LF_CounterAccess.IsLocalActor(atm, null), "a machine with no character (dedicated server) opens nothing");
		ctx.False(LF_CounterAccess.IsLocalActor(null, atm), "an action with no user opens nothing");
	}

	protected ScriptedUserAction FindAction(IEntity entity, string className)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (action && action.Type().ToString() == className)
				return ScriptedUserAction.Cast(action);
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The ATM's server seams name why they refused, so the terminal can say it.
// tier: WORLD
class LF_Test_ATMRefusalReasons : LF_Test
{
	override string GetName()
	{
		return "atm/refusal-reasons";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ATMManager.Reset();
		string uid = "test-atm-reasons";
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		LF_BankAccount account = LF_ATMManager.GetOrCreate(uid);
		ctx.Equal(30, LF_MoneyUtils.AddCash(character, 30), "seed cash 30 added");
		if (ctx.FailureCount() > 0)
		{
			LF_TestCharacterFixture.Delete(character);
			return;
		}

		ctx.Equal(LF_EATMRefusal.INVALID_AMOUNT, LF_CharacterATMComponent.ServeDepositReason(character, account, 0, uid), "a zero deposit says the amount is wrong");
		ctx.Equal(LF_EATMRefusal.NOT_ENOUGH_CASH, LF_CharacterATMComponent.ServeDepositReason(character, account, 100, uid), "an over-deposit says the cash is short");
		ctx.Equal(LF_EATMRefusal.NONE, LF_CharacterATMComponent.ServeDepositReason(character, account, 30, uid), "a covered deposit is accepted");
		ctx.Equal(LF_EATMRefusal.NOT_ENOUGH_BALANCE, LF_CharacterATMComponent.ServeWithdrawReason(character, account, 31, uid), "an over-withdrawal says the balance is short");
		ctx.Equal(LF_EATMRefusal.INVALID_AMOUNT, LF_CharacterATMComponent.ServeWithdrawReason(character, account, -5, uid), "a negative withdrawal says the amount is wrong");
		ctx.Equal(LF_EATMRefusal.NONE, LF_CharacterATMComponent.ServeWithdrawReason(character, account, 30, uid), "a covered withdrawal is accepted");
		ctx.Equal(30, LF_MoneyUtils.GetCash(character), "the round trip leaves the 30 cash in the pockets");
		LF_TestCharacterFixture.Delete(character);

		IEntity noInventory = LF_Utils.SpawnEntityPrefab(LF_TestCharacterFixture.JACKET_PREFAB, vector.Zero);
		ctx.NotNull(noInventory, "fixture: a jacket on the ground spawns");
		if (!noInventory)
			return;

		LF_BankAccount pocketsAccount = LF_ATMManager.GetOrCreate(uid + "-pockets");
		pocketsAccount.Deposit(40);
		ctx.Equal(LF_EATMRefusal.POCKETS_FULL, LF_CharacterATMComponent.ServeWithdrawReason(noInventory, pocketsAccount, 40, uid), "a payout that does not fit says the pockets are full");
		ctx.Equal(40, pocketsAccount.GetBalance(), "the refused payout stays in the account");
		LF_TestCharacterFixture.Delete(noInventory);
	}
}

//------------------------------------------------------------------------------------------------
//! The till names why it refused, so the order slip can say it. Drives the DebugWorld black
//! market (it sells only the M9) with a real character.
// tier: WORLD
class LF_Test_ShopRefusalReasons : LF_Test
{
	protected static const ResourceName M9_PREFAB = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
	protected static const ResourceName WATER_PREFAB = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";

	override string GetName()
	{
		return "shop/refusal-reasons";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity shopEntity = GetGame().GetWorld().FindEntityByName("Shop_BlackMarket_Debug");
		ctx.NotNull(shopEntity, "Shop_BlackMarket_Debug resolves in the DebugWorld");
		if (!shopEntity)
			return;

		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(shopEntity);
		ctx.NotNull(shop, "black market shop carries LF_ShopComponent");
		if (!shop)
			return;

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		int price = shop.GetBuyPrice(M9_PREFAB);
		int funds = price / 2;
		LF_MoneyUtils.AddCash(buyer, funds);

		int money;
		ctx.Equal(LF_EShopRefusal.NOT_ENOUGH_CASH, shop.Buy(M9_PREFAB, 1, buyer, money), "half the price says the cash is short");
		ctx.Equal(0, money, "a refused order charges nothing");
		ctx.Equal(LF_EShopRefusal.NOT_SOLD_HERE, shop.Buy(WATER_PREFAB, 1, buyer, money), "an item this counter does not stock says so");
		ctx.Equal(LF_EShopRefusal.BAD_QUANTITY, shop.Buy(M9_PREFAB, 0, buyer, money), "a zero order says the quantity is wrong");
		ctx.Equal(LF_EShopRefusal.NOTHING_TO_SELL, shop.Sell(M9_PREFAB, 1, buyer, money), "selling what you do not carry says so");
		ctx.Equal(funds, LF_MoneyUtils.GetCash(buyer), "no refusal moved the buyer's cash");

		LF_MoneyUtils.AddCash(buyer, price);
		ctx.Equal(LF_EShopRefusal.NONE, shop.Buy(M9_PREFAB, 1, buyer, money), "a covered order is paid");
		ctx.Equal(price, money, "the slip reports the price charged");

		LF_TestCharacterFixture.Delete(buyer);
	}
}

//------------------------------------------------------------------------------------------------
//! Both counter screens assemble from the kit on the server's workspace: every part and key
//! exists, the skins paint, and the keypad reaches the feature by key name.
// tier: WORLD
class LF_Test_CounterScreenLayout : LF_Test
{
	override string GetName()
	{
		return "counter/screen-layout";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource keyRow = Resource.Load(LF_UITheme.KEYS_LAYOUT);
		ctx.True(keyRow && keyRow.IsValid(), "the key row layout loads");

		if (!GetGame().GetWorkspace())
		{
			ctx.Fail("no workspace to assemble a screen on");
			return;
		}

		LF_UIScreen screen = LF_UIScreen.Open(LF_ATMTerminal.BuildParts(), LF_UISkin.Terminal());
		ctx.NotNull(screen, "the ATM terminal assembles");
		if (!screen)
			return;

		LF_TestButtonListener listener = new LF_TestButtonListener();
		screen.GetOnButton().Insert(listener.OnButton);

		ctx.True(screen.SetLine("Entry", LF_ATMTerminal.EntryText(250)), "the terminal has an entry line");
		ctx.True(screen.SetLine("Status", "#LF-ATM_RefuseCash"), "the terminal has a status line for refusals");
		ctx.True(screen.SetButtonText(LF_ATMTerminal.DEPOSIT, "#LF-ATM_KeyDeposit"), "the terminal has a DEPOSIT key");
		ctx.True(screen.SetButtonText("Key0Button", "0"), "the keypad has a 0 key");
		ctx.True(screen.SetButtonText(LF_ATMTerminal.BACK, "#LF-ATM_KeyBack"), "the keypad has a DEL key");
		ctx.EqualStr("terminal", screen.GetSkin().m_sName, "the terminal wears the terminal skin");

		screen.Notify("Key5Button");
		ctx.EqualStr("Key5Button", listener.m_sHeard, "a keypad key reaches the feature by name");
		screen.Close();
		ctx.False(screen.IsOpen(), "the terminal closes");

		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.CreateList("Prices"));
		parts.Insert(LF_ATMTerminal.Keys("Till", LF_ShopPriceList.PAY + "," + LF_ShopPriceList.SELL, "#LF-Shop_Pay,#LF-Shop_SellBack"));
		LF_UIScreen list = LF_UIScreen.Open(parts, LF_UISkin.PriceList());
		ctx.NotNull(list, "the price list assembles");
		if (!list)
			return;

		ctx.True(list.SetListHeight("Prices", LF_ShopPriceList.LIST_HEIGHT), "the price list sets its list height");
		ctx.True(list.AddListRow("Prices", LF_ShopPriceList.RowName(0), "Water", "$6", "#LF-Shop_Order"), "a price line is added");
		ctx.True(list.SetButtonText(LF_ShopPriceList.PAY, "#LF-Shop_Pay"), "the till has a Pay key");
		list.Close();
	}
}
