// red-proof: 2026-11-23, with the place binding skipped (RedProof() leaves m_sPlaceId empty) the barber
//   claims the depot's counter 23 m away and the run fails: cli test --no-build --red LF_Test_ResidentAnchorPlaceId
//! residents/anchor-place-id: a resident anchor can name its one place (#4613). A depot and a barber
//! of two kinds stand 23 m apart; a `counter` anchor beside the depot bound to the depot's id is the
//! depot's counter and never the barber's (a loose one is claimed by whoever's place of the kind is
//! nearest within 30 m, so the barber stood at the depot kiosk); each shop's own bound counter is its
//! own; an anchor bound to an id no place holds is nobody's.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentAnchorPlaceId : LF_Test
{
	protected static const string KIND_DEPOT = "test_residents_depot";
	protected static const string KIND_BARBER = "test_residents_barber";
	protected static const string ID_DEPOT = "depot_a";
	protected static const string ID_BARBER = "barber_a";
	protected static const ResourceName PLACE_PREFAB = "{25C8B889A777399D}Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et";
	protected static const ResourceName ANCHOR_PREFAB = "{CCED18E36D627B1D}Prefabs/Systems/Residents/LF_ResidentAnchor.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/anchor-place-id";
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAnchor(LF_TestContext ctx, string id, string placeId, vector at)
	{
		IEntity anchor = LF_Utils.SpawnEntityPrefab(ANCHOR_PREFAB, at, "0 0 0", false);
		ctx.NotNull(anchor, "anchor '" + id + "' spawns");
		if (!anchor)
			return null;

		LF_ResidentAnchorComponent comp = LF_ResidentAnchorComponent.Cast(anchor.FindComponent(LF_ResidentAnchorComponent));
		ctx.NotNull(comp, "anchor '" + id + "' carries the component");
		if (!comp)
			return anchor;

		if (!RedProof())
			comp.SetPlaceId(placeId);

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

		vector depotAt = vault.GetOrigin() + "0 -20 0";
		vector barberAt = depotAt + "23 0 0";
		IEntity depot = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, depotAt, "0 0 0", false);
		IEntity barber = LF_Utils.SpawnEntityPrefab(PLACE_PREFAB, barberAt, "0 0 0", false);
		ctx.True(depot && barber, "the depot and the barber spawn");
		if (!depot || !barber)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(depot);
			SCR_EntityHelper.DeleteEntityAndChildren(barber);
			return;
		}

		ctx.True(LF_Places.Register(KIND_DEPOT, ID_DEPOT, depot), "the depot is placed");
		ctx.True(LF_Places.Register(KIND_BARBER, ID_BARBER, barber), "the barber is placed");

		LF_ResidentSpot spot = new LF_ResidentSpot();

		// The depot's counter, bound to the depot, stands 1 m from it and 22 m from the barber.
		vector depotCounterAt = depotAt + "1 0 1";
		IEntity depotCounter = SpawnAnchor(ctx, "counter", ID_DEPOT, depotCounterAt);

		ctx.True(LF_ResidentAnchors.ResolveIn(KIND_DEPOT, ID_DEPOT, "counter", depotAt, spot), "the depot resolves");
		ctx.True(spot.m_bAnchored, "the depot finds its bound counter");
		ctx.EqualFloat(depotCounterAt[0], spot.m_vOrigin[0], 0.05, "the depot's counter is the bound anchor");

		ctx.True(LF_ResidentAnchors.ResolveIn(KIND_BARBER, ID_BARBER, "counter", barberAt, spot), "the barber resolves");
		ctx.True(!spot.m_bAnchored, "the barber does not claim the depot's counter");
		ctx.EqualFloat(barberAt[0], spot.m_vOrigin[0], 0.05, "the barber stands at his own origin");

		// Each shop's own counter, both bound.
		vector barberCounterAt = barberAt + "1 0 1";
		IEntity barberCounter = SpawnAnchor(ctx, "counter", ID_BARBER, barberCounterAt);
		ctx.True(LF_ResidentAnchors.ResolveIn(KIND_BARBER, ID_BARBER, "counter", barberAt, spot), "the barber resolves with his counter");
		ctx.True(spot.m_bAnchored, "the barber finds his bound counter");
		ctx.EqualFloat(barberCounterAt[0], spot.m_vOrigin[0], 0.05, "the barber's counter is his own");
		ctx.True(LF_ResidentAnchors.ResolveIn(KIND_DEPOT, ID_DEPOT, "counter", depotAt, spot), "the depot still resolves");
		ctx.EqualFloat(depotCounterAt[0], spot.m_vOrigin[0], 0.05, "the depot keeps its own counter");

		// An anchor bound to an id no place holds is nobody's, however near.
		IEntity orphan = SpawnAnchor(ctx, "stool", "test_nowhere", depotAt + "2 0 2");
		ctx.True(LF_ResidentAnchors.ResolveIn(KIND_DEPOT, ID_DEPOT, "stool", depotAt, spot), "an unclaimed id still resolves");
		ctx.True(!spot.m_bAnchored, "an anchor bound to an unknown place is claimed by nobody");

		LF_Places.Unregister(KIND_DEPOT, ID_DEPOT);
		LF_Places.Unregister(KIND_BARBER, ID_BARBER);
		SCR_EntityHelper.DeleteEntityAndChildren(depotCounter);
		SCR_EntityHelper.DeleteEntityAndChildren(barberCounter);
		SCR_EntityHelper.DeleteEntityAndChildren(orphan);
		SCR_EntityHelper.DeleteEntityAndChildren(depot);
		SCR_EntityHelper.DeleteEntityAndChildren(barber);
	}
}
