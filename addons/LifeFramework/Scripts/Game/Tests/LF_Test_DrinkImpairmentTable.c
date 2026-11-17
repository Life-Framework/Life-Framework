// red-proof: observed 2026-11-16 with `toolsi test --no-build --red LF_Test_DrinkImpairmentTable` (RedProof skips the overlay apply): failed `add: the overlay's new prefab has a row`, `two core rows plus one added (expected 3, got 2)` and `after the overlay the added row wins over the component`; green without the hook.
// tier: LOGIC
//! #2002: a drink's impairment values come from a table keyed by prefab that packs overlay. A row
//! the core table has is read, a pack overlay patches one row and adds another, a drink with no row
//! keeps the component's values, and a row naming a prefab that does not load is skipped.
class LF_Test_DrinkImpairmentTable : LF_Test
{
	static const ResourceName OVERLAY = "{81D226215C0DD606}Configs/Tests/LF_Test_DrinkImpairmentOverlay.conf";
	static const ResourceName WATER = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";
	static const ResourceName JUICE = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";
	static const ResourceName BASE = "{693D54F74C95B3D6}Prefabs/Items/Drinks/Drink_Base.et";
	static const ResourceName MISSING = "{0000000000000BAD}Prefabs/Items/Drinks/NoSuchDrink.et";

	override string GetName()
	{
		return "survival/drink-impairment-table";
	}

	//------------------------------------------------------------------------------------------------
	//! A stand-in for the core table: two rows.
	protected LF_DrinkImpairmentConfig MakeCore()
	{
		LF_DrinkImpairmentConfig config = new LF_DrinkImpairmentConfig();
		config.m_aDrinks = {};
		config.m_aDrinks.Insert(LF_DrinkImpairmentRow.Make(WATER, 10, 300, LF_EImpairmentCurve.LINEAR, 40));
		config.m_aDrinks.Insert(LF_DrinkImpairmentRow.Make(JUICE, 5, 120, LF_EImpairmentCurve.LINEAR, 20));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The shipped core file resolves, and ships neutral (no rows).
		Resource shipped = BaseContainerTools.LoadContainer(LF_DrinkImpairmentConfig.CONFIG_PATH);
		ctx.True(shipped && shipped.IsValid() && shipped.GetResource(), "the core table file loads");

		LF_DrinkImpairmentConfig config = MakeCore();

		// A core row is read.
		LF_DrinkImpairmentRow water = config.Find(WATER);
		ctx.NotNull(water, "core: the water row is found by prefab");
		if (water)
		{
			ctx.EqualFloat(10, water.m_fStrength, 0.001, "core: water strength");
			ctx.EqualFloat(300, water.m_fDurationSeconds, 0.001, "core: water duration");
			ctx.EqualFloat(40, water.m_fCap, 0.001, "core: water cap");
		}
		ctx.NotNull(config.Find("{A231A5F8D479B5DC}prefabs/items/drinks/waterbottle.et"), "the key is the GUID, so the path's casing does not split it");

		// A prefab with no row falls back to the component's values.
		LF_DrinkImpairmentRow component = LF_DrinkImpairmentRow.Make(BASE, 20, 600, LF_EImpairmentCurve.EXPONENTIAL, 60);
		ctx.True(config.Find(BASE) == null, "no row for a prefab the table does not list");
		LF_DrinkImpairmentRow resolved = config.Resolve(BASE, component);
		ctx.True(resolved == component, "no row: the component's own values are used");

		// A pack overlay changes one row, adds another, and drops the one naming a missing prefab.
		Resource overlay = BaseContainerTools.LoadContainer(OVERLAY);
		ctx.True(overlay && overlay.IsValid() && overlay.GetResource(), "the overlay fixture loads");
		if (!overlay || !overlay.IsValid() || !overlay.GetResource())
			return;

		int changed;
		if (!RedProof())
			changed = config.ApplyContainer(overlay.GetResource().ToBaseContainer(), OVERLAY);
		ctx.Equal(2, changed, "the overlay patches one row and adds one; the missing prefab is skipped");

		water = config.Find(WATER);
		ctx.NotNull(water, "patch: the water row is still there");
		if (water)
		{
			ctx.EqualFloat(35, water.m_fStrength, 0.001, "patch: strength is the overlay's");
			ctx.True(water.m_eCurve == LF_EImpairmentCurve.EXPONENTIAL, "patch: curve is the overlay's");
			ctx.EqualFloat(300, water.m_fDurationSeconds, 0.001, "patch: a field the overlay does not set keeps the core value");
			ctx.EqualFloat(40, water.m_fCap, 0.001, "patch: the cap is kept too");
		}

		LF_DrinkImpairmentRow added = config.Find(BASE);
		ctx.NotNull(added, "add: the overlay's new prefab has a row");
		if (added)
		{
			ctx.EqualFloat(25, added.m_fStrength, 0.001, "add: strength");
			ctx.EqualFloat(900, added.m_fDurationSeconds, 0.001, "add: duration");
			ctx.EqualFloat(80, added.m_fCap, 0.001, "add: cap");
		}

		ctx.True(config.Find(MISSING) == null, "a row naming a missing prefab is not in the table");
		ctx.Equal(3, config.m_aDrinks.Count(), "two core rows plus one added");
		LF_DrinkImpairmentRow juice = config.Find(JUICE);
		ctx.NotNull(juice, "a row the overlay does not name is still there");
		if (juice)
			ctx.EqualFloat(5, juice.m_fStrength, 0.001, "a row the overlay does not name is untouched");
		ctx.True(config.Resolve(BASE, component) == added, "after the overlay the added row wins over the component");

		// The core-file check drops a broken row too.
		LF_DrinkImpairmentConfig broken = MakeCore();
		broken.m_aDrinks.Insert(LF_DrinkImpairmentRow.Make(MISSING, 50, 100, LF_EImpairmentCurve.LINEAR, 60));
		ctx.Equal(1, broken.DropBrokenRows("test core"), "a core row naming a missing prefab is dropped");
		ctx.Equal(2, broken.m_aDrinks.Count(), "the good core rows stay");
	}
}
