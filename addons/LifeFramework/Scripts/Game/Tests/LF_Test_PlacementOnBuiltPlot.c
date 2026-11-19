// red-proof: 2026-11-19 `tools\cli test --no-build --red LF_Test_PlacementOnBuiltPlot` printed RED PROOF OK: with the hook on the plot stays unfinished (as every plot reads on main, where only LF_HouseManagerComponent houses count), so `a key holder places inside the cottage (expected '', got 'is not inside a property (no house within 30 m of the spot, zone OWNED_PROPERTY)')`.
//! placement/on-built-plot (#3173): furniture goes down inside the cottage a player built. plot-a's sign is
//! spawned beside a fixture character and every stage of its tier is marked built. A spot within
//! LF_PlacementZones.PLOT_RADIUS_M of the shell's origin is the plot: its owner may place there (zone
//! OWNED_PROPERTY and rule HOUSE_OWNER_OR_KEY), so may a character carrying a key bound to the plot, a
//! stranger may not. A spot past the radius is not the plot, an unfinished plot is no property, and a plot
//! sold mid-session refuses its old owner at once. Spots are measured from the shell origin, never typed.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlacementOnBuiltPlot : LF_Test
{
	protected static const ResourceName KEY_PREFAB = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";
	protected static const string PLOT = "plot-a";
	protected static const string OWNER = "lf-test-build-owner";
	protected static const string FRIEND = "lf-test-build-friend";
	protected static const string STRANGER = "lf-test-build-stranger";
	protected static const string NEW_OWNER = "lf-test-build-new-owner";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "placement/on-built-plot";
	}

	//------------------------------------------------------------------------------------------------
	//! The zone's verdict at `spot` for `characterKey` carrying keys to `heldKeys`; empty when allowed.
	protected string Where(vector spot, string characterKey, array<string> heldKeys, out string houseId)
	{
		LF_PlacementWhereFacts facts = new LF_PlacementWhereFacts();
		LF_PlacementZones.GatherHouseFor(spot, characterKey, heldKeys, facts);
		houseId = facts.m_sHouseId;
		string toast;
		return LF_PlacementZones.Judge(LF_EPlacementWhere.OWNED_PROPERTY, facts, toast);
	}

	//------------------------------------------------------------------------------------------------
	//! The who rule's verdict for a player standing at `at`; empty when allowed.
	protected string Who(vector at, string characterKey, array<string> heldKeys)
	{
		LF_PlacementWhoFacts facts = new LF_PlacementWhoFacts();
		LF_PlacementZones.GatherPlot(at, characterKey, heldKeys, facts);
		string toast;
		return LF_PlacementPolicy.Judge(LF_EPlacementWho.HOUSE_OWNER_OR_KEY, facts, toast);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PropertyConfig.Reload();
		array<ref LF_PropertyRecord> before = LF_Properties.ExportAll();
		LF_Properties.ApplyAll(null);

		IEntity character;
		IEntity sign;
		LF_BuildSiteComponent site = LF_Test_BuildSite.SetUp(ctx, character, sign);
		string tier = LF_PropertyConfig.PlotTierId(PLOT);
		int stages = LF_PropertyConfig.StageCount(tier);
		ctx.True(stages > 1, "the example plot's tier has more than one stage");
		if (!site || stages <= 1)
		{
			Cleanup(character, sign, before);
			return;
		}

		if (!RedProof())
			LF_Properties.SetStage(PLOT, stages);

		vector shell = site.GetShellOrigin();
		vector inside = shell + "1.5 0 1";
		array<string> noKeys = {};
		string houseId;

		// 1. The owner of the finished plot places inside the shell.
		ctx.EqualStr("", Where(inside, OWNER, noKeys, houseId), "the owner places inside the cottage they built");
		ctx.EqualStr(PLOT, houseId, "the spot inside the shell belongs to the plot");
		ctx.EqualStr("", Who(inside, OWNER, noKeys), "the owner standing in the cottage passes the house rule");

		// 2. A key holder of the plot is accepted too.
		ctx.Equal(1, LF_InventoryUtils.AddAmount(character, KEY_PREFAB, 1), "the character is handed a house key");
		array<IEntity> keyItems = LF_InventoryUtils.FindItemsByPrefab(LF_InventoryUtils.GetResponsibleStorageManager(character), KEY_PREFAB);
		LF_KeyComponent key;
		if (!keyItems.IsEmpty())
			key = LF_Component<LF_KeyComponent>.Find(keyItems[0]);

		ctx.NotNull(key, "the key item carries LF_KeyComponent");
		if (key)
		{
			key.SetHouseIdentifier(PLOT);
			ctx.True(LF_BuildSiteComponent.UserHoldsKey(character, PLOT), "the key is bound to the plot");
			array<string> heldKeys = {};
			LF_PlacementPolicy.HeldKeyHouseIds(character, heldKeys);
			ctx.EqualStr("", Where(inside, FRIEND, heldKeys, houseId), "a key holder places inside the cottage");
			ctx.EqualStr("", Who(inside, FRIEND, heldKeys), "a key holder passes the house rule");
		}

		// 3. A stranger is refused, and the reason names the plot.
		string why = Where(inside, STRANGER, noKeys, houseId);
		ctx.True(why.IndexOf("neither own nor hold a key") != -1 && why.IndexOf(PLOT) != -1, "a stranger is refused in the cottage: " + why);
		ctx.False(Who(inside, STRANGER, noKeys).IsEmpty(), "a stranger fails the house rule");

		// 4. Past the radius the spot is not the plot.
		vector outside = shell + Vector(LF_PlacementZones.PLOT_RADIUS_M + 3, 0, 0);
		why = Where(outside, OWNER, noKeys, houseId);
		ctx.False(why.IsEmpty(), "a spot past the shell's radius is refused for the owner: " + why);
		ctx.False(houseId == PLOT, "a spot past the radius does not belong to the plot");

		// 5. An unfinished plot is no property.
		LF_Properties.SetStage(PLOT, stages - 1);
		why = Where(inside, OWNER, noKeys, houseId);
		ctx.False(why.IsEmpty(), "an unfinished plot is refused for the owner: " + why);
		ctx.False(houseId == PLOT, "an unfinished plot is no property");

		// 6. A plot sold mid-session stops accepting its old owner at once.
		LF_Properties.SetStage(PLOT, stages);
		ctx.EqualStr("", Where(inside, OWNER, noKeys, houseId), "finished again, the owner places");
		ctx.True(LF_Properties.SetOwner(PLOT, NEW_OWNER), "the plot is sold");
		ctx.False(Where(inside, OWNER, noKeys, houseId).IsEmpty(), "the old owner is refused at once");
		ctx.EqualStr("", Where(inside, NEW_OWNER, noKeys, houseId), "the new owner places");

		Cleanup(character, sign, before);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity character, IEntity sign, array<ref LF_PropertyRecord> before)
	{
		LF_TestCharacterFixture.Delete(sign);
		LF_TestCharacterFixture.Delete(character);
		LF_Properties.ApplyAll(before);
	}
}
