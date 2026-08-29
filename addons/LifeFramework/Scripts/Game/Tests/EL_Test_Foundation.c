// red-proof: perturb any expected value below (e.g. "$12,500" -> "$12,5000")
// or break the thousands-separator rule and run the fast tier; the affected
// case fails.

class EL_Test_MoneyFormat : EL_Test
{
	override string GetName()
	{
		return "core/money-format";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.EqualStr("$0", EL_MoneyFormat.FormatMoney(0), "zero formats as $0");
		ctx.EqualStr("$999", EL_MoneyFormat.FormatMoney(999), "no separator under 1000");
		ctx.EqualStr("$12,500", EL_MoneyFormat.FormatMoney(12500), "thousands separator");
		ctx.EqualStr("$1,000,000", EL_MoneyFormat.FormatMoney(1000000), "two separators");
		ctx.EqualStr("-$150", EL_MoneyFormat.FormatMoney(-150), "minus sign outside symbol");
		ctx.EqualStr("", EL_MoneyFormat.FormatDelta(0), "no delta shows nothing");
		ctx.EqualStr("+$500", EL_MoneyFormat.FormatDelta(500), "positive delta signed");
		ctx.EqualStr("-$150", EL_MoneyFormat.FormatDelta(-150), "negative delta signed");
	}
};

// red-proof: break the first-observation seed rule (e.g. return the delta on
// the very first Update) and the "opening the HUD is not a transaction" case
// fails.

class EL_Test_MoneyDeltaTracker : EL_Test
{
	override string GetName()
	{
		return "core/money-delta-tracker";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_MoneyDeltaTracker tracker = new EL_MoneyDeltaTracker();

		tracker.Update(100, 0);
		ctx.Equal(0, tracker.GetDelta(), "first observation only seeds the baseline");
		ctx.False(tracker.IsVisible(), "first observation shows nothing");
		ctx.EqualStr("", tracker.GetText(), "first observation draws nothing");

		tracker.Update(150, 0.1);
		ctx.Equal(50, tracker.GetDelta(), "a change accumulates into the delta");
		ctx.True(tracker.IsVisible(), "a change makes the ticker visible");
		ctx.EqualStr("+$50", tracker.GetText(), "delta renders signed");

		tracker.Update(150, 3.0);
		ctx.Equal(50, tracker.GetDelta(), "unchanged money does not add to the delta");

		tracker.Update(150, 2.0);
		ctx.Equal(0, tracker.GetDelta(), "delta clears after reset seconds with no change");
		ctx.False(tracker.IsVisible(), "a cleared ticker hides");

		EL_MoneyDeltaTracker combo = new EL_MoneyDeltaTracker();
		combo.Update(100, 0);
		combo.Update(120, 0);
		combo.Update(90, 0);
		ctx.Equal(-10, combo.GetDelta(), "two quick changes read as one combined delta");
		ctx.EqualStr("-$10", combo.GetText(), "combined negative delta renders");

		EL_MoneyDeltaTracker slow = new EL_MoneyDeltaTracker();
		slow.SetResetSeconds(10);
		slow.Update(0, 0);
		slow.Update(1, 0);
		ctx.Equal(1, slow.GetDelta(), "SetResetSeconds lengthens the visible window");
		slow.Update(1, 9);
		ctx.Equal(1, slow.GetDelta(), "still visible inside the custom window");
		slow.Update(1, 2);
		ctx.Equal(0, slow.GetDelta(), "clears past the custom window");
	}
};

// red-proof: flip a predicate (e.g. CanSellItem returns true for an equipped
// item) and the fast tier fails the equipped-item case.

class EL_Test_ShopRules : EL_Test
{
	override string GetName()
	{
		return "core/shop-rules";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.True(EL_ShopRules.ShopBuysFromPlayers(EL_ShopType.SHOP_GENERAL, false, 1.0), "general shop buys");
		ctx.False(EL_ShopRules.ShopBuysFromPlayers(EL_ShopType.SHOP_VEHICLE, false, 1.0), "vehicle shop does not buy");
		ctx.False(EL_ShopRules.ShopBuysFromPlayers(EL_ShopType.SHOP_GENERAL, true, 1.0), "procurement shop does not buy");
		ctx.False(EL_ShopRules.ShopBuysFromPlayers(EL_ShopType.SHOP_GUNDEALER, false, 0.0), "gun dealer buy disabled at zero multiplier");
		ctx.True(EL_ShopRules.ShopBuysFromPlayers(EL_ShopType.SHOP_GUNDEALER, false, 0.5), "gun dealer buy enabled at positive multiplier");

		ctx.EqualFloat(0.5, EL_ShopRules.GetSellMultiplier(EL_ShopType.SHOP_GUNDEALER, 0.5), 0.001, "gun dealer applies its multiplier");
		ctx.EqualFloat(1.0, EL_ShopRules.GetSellMultiplier(EL_ShopType.SHOP_GENERAL, 0.5), 0.001, "other shops ignore the multiplier");

		ctx.False(EL_ShopRules.CanSellItem(true, true, 100), "equipped items never sell");
		ctx.False(EL_ShopRules.CanSellItem(false, false, 100), "shop that does not buy rejects the sale");
		ctx.False(EL_ShopRules.CanSellItem(false, true, 0), "worthless items never sell");
		ctx.True(EL_ShopRules.CanSellItem(false, true, 100), "sellable item passes");

		ctx.EqualStr(EL_ShopRules.REASON_EQUIPPED, EL_ShopRules.GetBlockReasonKey(true, true, 100), "equipped block names its reason");
		ctx.EqualStr(EL_ShopRules.REASON_SHOP_DOES_NOT_BUY, EL_ShopRules.GetBlockReasonKey(false, false, 100), "not-bought-here names its reason");
		ctx.EqualStr(EL_ShopRules.REASON_NO_VALUE, EL_ShopRules.GetBlockReasonKey(false, true, 0), "no-value names its reason");
		ctx.EqualStr("", EL_ShopRules.GetBlockReasonKey(false, true, 100), "sellable item has no reason");
	}
};

// red-proof: make AddDef accept a duplicate id (or an empty id) and the fast
// tier fails the duplicate/empty cases.

class EL_Test_ResourceDefs : EL_Test
{
	override string GetName()
	{
		return "core/resource-defs";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_ResourceDefs defs = new EL_ResourceDefs();
		ctx.Equal(0, defs.AddDef("timber", 40, 25, 100, 1, 0), "first def gets index 0");
		ctx.Equal(1, defs.AddDef("cement", 30, 20, 200, 1, 0), "second def gets index 1");
		ctx.Equal(-1, defs.AddDef("timber", 1, 1, 1, 0, 0), "duplicate id is rejected");
		ctx.Equal(-1, defs.AddDef("", 1, 1, 1, 0, 0), "empty id is rejected");
		ctx.Equal(2, defs.AddDef("cocaine", 1, 0.1, 500, 0, 1), "illegal def accepted at index 2");

		ctx.Equal(0, defs.IndexOf("timber"), "IndexOf finds timber");
		ctx.Equal(-1, defs.IndexOf("steel"), "unknown id has no index");
		ctx.EqualStr("cement", defs.IdAt(1), "IdAt resolves the index back");
		ctx.EqualStr("", defs.IdAt(9), "out-of-range index yields empty id");
		ctx.Equal(3, defs.Count(), "count tracks appended defs");
		ctx.True(defs.Knows("timber"), "Knows resolves a known id");
		ctx.False(defs.Knows("steel"), "Knows rejects an unknown id");

		ctx.Equal(40, defs.LitresPerUnit("timber"), "litres per unit by id");
		ctx.EqualFloat(20, defs.KgPerUnit("cement"), 0.001, "kg per unit by id");
		ctx.Equal(200, defs.BasePriceAt(1), "base price by index");
		ctx.True(defs.IsImportable(0), "timber importable");
		ctx.False(defs.IsImportable(2), "cocaine not importable");
		ctx.True(defs.IsIllegal(2), "cocaine illegal");
		ctx.False(defs.IsIllegal(0), "timber not illegal");
	}
};