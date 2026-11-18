// tier: WORLD
// red-proof: 2026-11-17, `tools/cli test --no-build --red LF_Test_PlacementPolicyWhere` printed RED PROOF OK (the RedProof hook skips the zone gather, so house B reads as no house, the neighbour is not named and a key to it finds nothing); with the hook off it passes, and PlacementPolicyWho, PlacementConfirm and PlacementComposition stay green.
//! placement/policy-where (#2447): a kind's zone in the placement policy limits where it goes. A roads-only
//! kind is refused off the road and allowed on it; a kind for an owned property is refused in the neighbour's
//! house and allowed in the player's own or one they hold a key to; an organization-base kind is refused far
//! from the base. DebugWorld has no AI road network, so "off the road" runs the real probe and "on the road"
//! runs the pure rule over a road fact (LF_Test_PlacementRoad covers the road network on Everon). Two
//! neighbouring houses are spawned and bound; the spots are measured from their origins, never typed.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlacementPolicyWhere : LF_Test
{
	protected static const ResourceName BUILDING = "{74676598DF38F201}Prefabs/Structures/Core/EL_Building_Base.et";
	protected static const string HOUSE_A = "policy-where-a";
	protected static const string HOUSE_B = "policy-where-b";
	protected static const string OWNER_A = "policy-where-owner-a";
	protected static const string OWNER_B = "policy-where-owner-b";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "placement/policy-where";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunRoads(ctx);
		RunProperty(ctx);
		RunBase(ctx);
		RunTable(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlacementWhereFacts Facts(vector spot, LF_EPlacementWhere zone, float radius)
	{
		if (RedProof())
			return new LF_PlacementWhereFacts();

		return LF_PlacementZones.Gather(null, spot, zone, radius);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRoads(LF_TestContext ctx)
	{
		string toast;
		LF_PlacementWhereFacts onRoad = new LF_PlacementWhereFacts();
		onRoad.m_bOnRoad = true;
		ctx.EqualStr("", LF_PlacementZones.Judge(LF_EPlacementWhere.ROADS_ONLY, onRoad, toast), "a roads-only kind is allowed on the road");
		ctx.EqualStr("", toast, "with no toast");

		// DebugWorld has no AI road network: the real probe finds no road at the player's feet.
		LF_PlacementWhereFacts off = Facts("66 0 66", LF_EPlacementWhere.ROADS_ONLY, 0);
		ctx.False(off.m_bOnRoad, "the real probe finds no road on DebugWorld");
		string why = LF_PlacementZones.Judge(LF_EPlacementWhere.ROADS_ONLY, off, toast);
		ctx.True(why.IndexOf("not on a road") != -1 && why.IndexOf("ROADS_ONLY") != -1, "off the road is refused, the reason names the zone: " + why);
		ctx.EqualStr(LF_PlacementZones.TOAST_ROAD, toast, "with the road toast");

		// A spot with no zone entry is not limited.
		ctx.EqualStr("", LF_PlacementZones.Judge(LF_EPlacementWhere.ANYWHERE, off, toast), "an anywhere kind is allowed off the road");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunProperty(LF_TestContext ctx)
	{
		array<ref LF_HouseLockRecord> before = LF_Houses.ExportAll();
		LF_Houses.ApplyAll(null);

		IEntity buildingA = LF_Utils.SpawnEntityPrefab(BUILDING, "66 0 66");
		IEntity buildingB = LF_Utils.SpawnEntityPrefab(BUILDING, "126 0 66");
		LF_HouseManagerComponent houseA;
		LF_HouseManagerComponent houseB;
		if (buildingA)
			houseA = LF_Component<LF_HouseManagerComponent>.Find(buildingA);
		if (buildingB)
			houseB = LF_Component<LF_HouseManagerComponent>.Find(buildingB);

		ctx.NotNull(houseA, "house A spawns with a house manager");
		ctx.NotNull(houseB, "house B spawns with a house manager");
		if (houseA && houseB)
		{
			ctx.True(houseA.BindIdentifier(HOUSE_A), "house A is bound");
			ctx.True(houseB.BindIdentifier(HOUSE_B), "house B is bound");
			houseA.SetOwner(OWNER_A);
			houseB.SetOwner(OWNER_B);
			RunHouses(ctx, buildingA.GetOrigin(), buildingB.GetOrigin());
		}

		LF_TestCharacterFixture.Delete(buildingA);
		LF_TestCharacterFixture.Delete(buildingB);
		LF_Houses.ApplyAll(before);
	}

	//------------------------------------------------------------------------------------------------
	//! A chair in the owner's own house is allowed and in the neighbour's is refused, naming the house.
	protected void RunHouses(LF_TestContext ctx, vector spotA, vector spotB)
	{
		string toast;
		array<string> noKeys = {};
		LF_PlacementWhereFacts inOwn = new LF_PlacementWhereFacts();
		if (!RedProof())
			LF_PlacementZones.GatherHouseFor(spotA, OWNER_A, noKeys, inOwn);
		ctx.EqualStr(HOUSE_A, inOwn.m_sHouseId, "a spot inside house A is house A");
		ctx.EqualStr("", LF_PlacementZones.Judge(LF_EPlacementWhere.OWNED_PROPERTY, inOwn, toast), "the owner places in their own house");

		LF_PlacementWhereFacts inNeighbour = new LF_PlacementWhereFacts();
		if (!RedProof())
			LF_PlacementZones.GatherHouseFor(spotB, OWNER_A, noKeys, inNeighbour);
		ctx.EqualStr(HOUSE_B, inNeighbour.m_sHouseId, "a spot inside house B is house B, not the nearer-to-the-player one");
		string why = LF_PlacementZones.Judge(LF_EPlacementWhere.OWNED_PROPERTY, inNeighbour, toast);
		ctx.True(why.IndexOf("neither own nor hold a key") != -1 && why.IndexOf(HOUSE_B) != -1 && why.IndexOf("OWNED_PROPERTY") != -1, "the neighbour's house is refused, the reason names the house and the zone: " + why);
		ctx.EqualStr(LF_PlacementZones.TOAST_PROPERTY, toast, "with the property toast");

		LF_PlacementWhereFacts withKey = new LF_PlacementWhereFacts();
		array<string> keyToB = {HOUSE_B};
		if (!RedProof())
			LF_PlacementZones.GatherHouseFor(spotB, OWNER_A, keyToB, withKey);
		ctx.EqualStr("", LF_PlacementZones.Judge(LF_EPlacementWhere.OWNED_PROPERTY, withKey, toast), "a key to the neighbour's house lets them place there");

		// A spot with no house near it (the real gather finds none 200 m from both).
		LF_PlacementWhereFacts open = new LF_PlacementWhereFacts();
		LF_PlacementZones.GatherHouseFor(spotA + "200 0 0", OWNER_A, noKeys, open);
		why = LF_PlacementZones.Judge(LF_EPlacementWhere.OWNED_PROPERTY, open, toast);
		ctx.True(why.IndexOf("not inside a property") != -1, "open ground is refused for an owned-property kind: " + why);
	}

	//------------------------------------------------------------------------------------------------
	//! The base zone, pure: no organization, no base placed, too far, and inside the radius.
	protected void RunBase(LF_TestContext ctx)
	{
		string toast;
		LF_PlacementWhereFacts none = new LF_PlacementWhereFacts();
		string why = LF_PlacementZones.Judge(LF_EPlacementWhere.ORG_BASE, none, toast);
		ctx.True(why.IndexOf("not in an organization") != -1, "no organization is refused: " + why);
		ctx.EqualStr(LF_PlacementZones.TOAST_BASE, toast, "with the base toast");

		LF_PlacementWhereFacts noBase = new LF_PlacementWhereFacts();
		noBase.m_sOrgId = "crew";
		why = LF_PlacementZones.Judge(LF_EPlacementWhere.ORG_BASE, noBase, toast);
		ctx.True(why.IndexOf("has no base") != -1, "an organization with no base placed is refused: " + why);

		LF_PlacementWhereFacts far = new LF_PlacementWhereFacts();
		far.m_sOrgId = "crew";
		far.m_bBaseKnown = true;
		far.m_fBaseRadius = 30;
		far.m_fBaseDistance = 31;
		why = LF_PlacementZones.Judge(LF_EPlacementWhere.ORG_BASE, far, toast);
		ctx.True(why.IndexOf("from the base") != -1 && why.IndexOf("ORG_BASE") != -1, "past the radius is refused, naming the zone: " + why);

		LF_PlacementWhereFacts near = new LF_PlacementWhereFacts();
		near.m_sOrgId = "crew";
		near.m_bBaseKnown = true;
		near.m_fBaseRadius = 30;
		near.m_fBaseDistance = 30;
		ctx.EqualStr("", LF_PlacementZones.Judge(LF_EPlacementWhere.ORG_BASE, near, toast), "at the radius places");
	}

	//------------------------------------------------------------------------------------------------
	//! The table: a kind's entry carries its zone, the shipped table limits none (core names no kind),
	//! and the whole path (ZoneAt) refuses a roads-only kind on DebugWorld.
	protected void RunTable(LF_TestContext ctx)
	{
		LF_PlacementPolicyConfig.Reload();
		float radius;
		ctx.True(LF_PlacementPolicy.WhereFor("SpikeStrip", radius) == LF_EPlacementWhere.ANYWHERE, "a kind with no zone is anywhere");

		LF_PlacementPolicyConfig config = new LF_PlacementPolicyConfig();
		config.m_aEntries = new array<ref LF_PlacementPolicyEntry>();
		LF_PlacementPolicyEntry bench = new LF_PlacementPolicyEntry();
		bench.m_sKind = "Bench";
		bench.m_eWhere = LF_EPlacementWhere.ROADS_ONLY;
		config.m_aEntries.Insert(bench);
		LF_PlacementPolicyEntry hut = new LF_PlacementPolicyEntry();
		hut.m_sKind = "Hut";
		hut.m_eWhere = LF_EPlacementWhere.ORG_BASE;
		hut.m_fBaseRadius = 45;
		config.m_aEntries.Insert(hut);
		LF_PlacementPolicyConfig.UseForTest(config);

		ctx.True(LF_PlacementPolicy.WhereFor("Bench", radius) == LF_EPlacementWhere.ROADS_ONLY, "the table's entry carries the zone");
		ctx.True(LF_PlacementPolicy.WhereFor("Hut", radius) == LF_EPlacementWhere.ORG_BASE, "another entry carries its own zone");
		ctx.True(radius == 45, "and its base radius");

		string toast;
		string why = LF_PlacementPolicy.ZoneAt(null, "Bench", "66 0 66", toast);
		ctx.True(why.IndexOf("ROADS_ONLY") != -1, "ZoneAt refuses a roads-only kind off the road: " + why);
		ctx.EqualStr("", LF_PlacementPolicy.ZoneAt(null, "Chair", "66 0 66", toast), "ZoneAt lets a kind with no zone go anywhere");

		LF_PlacementPolicyConfig.Reload();
	}
}
