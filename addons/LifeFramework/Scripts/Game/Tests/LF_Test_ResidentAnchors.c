// red-proof: 2026-11-18, before LF_ResidentAnchors existed the test did not compile; with
//   LF_ResidentAnchors.Resolve stubbed to return the place origin for every anchor id the run fails:
//   cli test --no-build --only LF_Test_ResidentAnchors
// (the run that went red on the first build and green on the implementation is recorded in the PR)
//! residents/anchors: a timetable line (place kind + anchor id) becomes a world position and facing
//! (#2857). A `test_shop` with a `counter` and a `door` resolves each anchor to its own transform; an
//! empty anchor id is the place origin; an unknown id falls back to the origin and warns once per
//! kind and id pair; of two places of one kind the one nearest the given point answers.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentAnchors : LF_Test
{
	protected static const string KIND = "test_residents_shop";
	protected static const ResourceName PLACE_PREFAB = "{25C8B889A777399D}Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et";
	protected static const ResourceName ANCHOR_PREFAB = "{CCED18E36D627B1D}Prefabs/Systems/Residents/LF_ResidentAnchor.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/anchors";
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAnchor(LF_TestContext ctx, string id, vector at, vector angles)
	{
		IEntity anchor = LF_Utils.SpawnEntityPrefab(ANCHOR_PREFAB, at, angles, false);
		ctx.NotNull(anchor, "anchor '" + id + "' spawns");
		if (!anchor)
			return null;

		LF_ResidentAnchorComponent comp = LF_ResidentAnchorComponent.Cast(anchor.FindComponent(LF_ResidentAnchorComponent));
		ctx.NotNull(comp, "anchor '" + id + "' carries the component");
		if (comp && !RedProof())
			comp.SetAnchorId(id);

		return anchor;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity vault = LF_Places.Find(LF_Bank.PLACE_KIND, LF_TestAnnex.BANK_VAULT);
		ctx.NotNull(vault, "the annex vault is placed, to spawn the stand-ins near");
		if (!vault)
			return;

		vector baseAt = vault.GetOrigin() + "0 -20 0";
		vector farAt = baseAt + "400 0 0";
		IEntity placeA = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, baseAt, "0 0 0", false);
		IEntity placeB = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, farAt, "0 0 0", false);
		ctx.True(placeA && placeB, "two test shops spawn");
		if (!placeA || !placeB)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(placeA);
			SCR_EntityHelper.DeleteEntityAndChildren(placeB);
			return;
		}

		ctx.True(LF_Places.Register(KIND, "shop_a", placeA), "shop a is placed");
		ctx.True(LF_Places.Register(KIND, "shop_b", placeB), "shop b is placed");

		vector counterAt = baseAt + "3 0 4";
		vector doorAt = baseAt + "-2 0 9";
		IEntity counter = SpawnAnchor(ctx, "counter", counterAt, "90 0 0");
		IEntity door = SpawnAnchor(ctx, "door", doorAt, "180 0 0");

		LF_ResidentSpot spot = new LF_ResidentSpot();
		int warnsBefore = LF_ResidentAnchors.WarnCount();

		ctx.True(LF_ResidentAnchors.Resolve(KIND, "counter", baseAt, spot), "a place of the kind resolves");
		ctx.True(spot.m_bAnchored, "the counter anchor is found");
		ctx.EqualFloat(counterAt[0], spot.m_vOrigin[0], 0.05, "the counter x is the anchor's");
		ctx.EqualFloat(counterAt[2], spot.m_vOrigin[2], 0.05, "the counter z is the anchor's");
		ctx.EqualFloat(90, spot.m_vAngles[0], 0.5, "the counter yaw is the anchor's facing");

		ctx.True(LF_ResidentAnchors.Resolve(KIND, "door", baseAt, spot), "the door resolves");
		ctx.EqualFloat(doorAt[0], spot.m_vOrigin[0], 0.05, "the door x is the anchor's");
		ctx.EqualFloat(doorAt[2], spot.m_vOrigin[2], 0.05, "the door z is the anchor's");

		ctx.True(LF_ResidentAnchors.Resolve(KIND, "", baseAt, spot), "an empty anchor id resolves");
		ctx.True(!spot.m_bAnchored, "an empty anchor id is no anchor");
		ctx.EqualFloat(baseAt[0], spot.m_vOrigin[0], 0.05, "an empty anchor id is the place origin x");
		ctx.EqualFloat(baseAt[2], spot.m_vOrigin[2], 0.05, "an empty anchor id is the place origin z");
		ctx.Equal(warnsBefore, LF_ResidentAnchors.WarnCount(), "an empty anchor id does not warn");

		// Unknown id: origin of the place, one warning per pair.
		ctx.True(LF_ResidentAnchors.Resolve(KIND, "bench", baseAt, spot), "an unknown anchor id still resolves");
		ctx.True(!spot.m_bAnchored, "an unknown anchor id is no anchor");
		ctx.EqualFloat(baseAt[0], spot.m_vOrigin[0], 0.05, "an unknown anchor id falls back to the place origin x");
		ctx.Equal(warnsBefore + 1, LF_ResidentAnchors.WarnCount(), "the first unknown pair warns");
		LF_ResidentAnchors.Resolve(KIND, "bench", baseAt, spot);
		ctx.Equal(warnsBefore + 1, LF_ResidentAnchors.WarnCount(), "the same pair warns once");
		LF_ResidentAnchors.Resolve(KIND, "stool", baseAt, spot);
		ctx.Equal(warnsBefore + 2, LF_ResidentAnchors.WarnCount(), "another pair warns again");

		// Two places: the nearest to the point answers, and shop b has no counter of its own.
		ctx.True(LF_ResidentAnchors.Resolve(KIND, "counter", farAt, spot), "shop b resolves");
		ctx.True(!spot.m_bAnchored, "shop a's counter is not shop b's");
		ctx.EqualFloat(farAt[0], spot.m_vOrigin[0], 0.05, "the nearest place to the point is shop b");

		IEntity counterB = SpawnAnchor(ctx, "counter", farAt + "1 0 1", "0 0 0");
		ctx.True(LF_ResidentAnchors.Resolve(KIND, "counter", farAt, spot), "shop b resolves with its counter");
		ctx.True(spot.m_bAnchored, "shop b's own counter is found");
		ctx.EqualFloat(farAt[0] + 1, spot.m_vOrigin[0], 0.05, "shop b's counter is the one near the point");
		ctx.True(LF_ResidentAnchors.Resolve(KIND, "counter", baseAt, spot), "shop a still resolves");
		ctx.EqualFloat(counterAt[0], spot.m_vOrigin[0], 0.05, "near shop a the counter is shop a's");

		ctx.True(!LF_ResidentAnchors.Resolve("test_residents_nowhere", "counter", baseAt, spot), "a kind with no place does not resolve");

		LF_Places.Unregister(KIND, "shop_a");
		LF_Places.Unregister(KIND, "shop_b");
		SCR_EntityHelper.DeleteEntityAndChildren(counter);
		SCR_EntityHelper.DeleteEntityAndChildren(door);
		SCR_EntityHelper.DeleteEntityAndChildren(counterB);
		SCR_EntityHelper.DeleteEntityAndChildren(placeA);
		SCR_EntityHelper.DeleteEntityAndChildren(placeB);
	}
}
