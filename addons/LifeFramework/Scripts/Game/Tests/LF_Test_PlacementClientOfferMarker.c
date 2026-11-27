// red-proof: RED-OK placement/client-offer-marker (tools/cli test --no-build --only, 2026-11-27, on the unfixed STATIC query: "the owner is offered the placement in a marker house (expected '', got 'has no house within 30 m (rule HOUSE_OWNER_OR_KEY)')", the same for the key holder; green with EQueryEntitiesFlags.ALL).
//! placement/client-offer-marker (playtest round 5, 2026-11-27): the owner of a bought Everon house could not place
//! the furniture they had just bought: "Place where I aim" never showed on the remote client, inside the house.
//! Every island house is an `LF_HouseMarker.et` (a GenericEntity with no mesh) dropped into a vanilla building,
//! not a mesh building carrying the house manager as in placement/client-offer. The client branch found houses
//! with a STATIC-only world query, which a mesh-less marker is not part of, so it saw no house within 30 m.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlacementClientOfferMarker : LF_Test
{
	protected static const ResourceName MARKER = "{4D1B7E92A6C03F58}Prefabs/Systems/Houses/LF_HouseMarker.et";
	protected static const string OWNER = "client-offer-marker-owner";
	protected static const string STRANGER = "client-offer-marker-stranger";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "placement/client-offer-marker";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource res = Resource.Load(MARKER);
		ctx.True(res && res.IsValid(), "the house marker prefab loads");
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		params.Transform[3] = Vector(140, 0, 140);
		params.Transform[3][1] = GetGame().GetWorld().GetSurfaceY(140, 140);
		IEntity marker = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(marker, "the marker spawns");
		if (!marker)
			return;

		LF_HouseManagerComponent house = LF_Component<LF_HouseManagerComponent>.Find(marker);
		ctx.NotNull(house, "the marker carries LF_HouseManagerComponent");
		if (!house)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(marker);
			return;
		}

		house.SetOwner(OWNER);
		vector inside = marker.GetOrigin() + "1.5 0 1";
		array<string> noKeys = {};
		array<string> keys = {house.GetHouseIdentifier()};

		string houseId;
		string ownerKey;
		bool found;
		if (!RedProof())
			found = LF_PlacementPolicy.NearestReplicatedHouse(inside, houseId, ownerKey);

		ctx.True(found, "a client finds the mesh-less house marker it stands in");
		ctx.EqualStr(house.GetHouseIdentifier(), houseId, "the house found is this marker's");

		LF_PlacementWhoFacts owner = new LF_PlacementWhoFacts();
		LF_PlacementWhoFacts holder = new LF_PlacementWhoFacts();
		LF_PlacementWhoFacts stranger = new LF_PlacementWhoFacts();
		if (!RedProof())
		{
			LF_PlacementPolicy.GatherHouseClientFor(inside, OWNER, noKeys, owner);
			LF_PlacementPolicy.GatherHouseClientFor(inside, STRANGER, keys, holder);
			LF_PlacementPolicy.GatherHouseClientFor(inside, STRANGER, noKeys, stranger);
		}

		string toast;
		ctx.EqualStr("", LF_PlacementPolicy.Judge(LF_EPlacementWho.HOUSE_OWNER_OR_KEY, owner, toast), "the owner is offered the placement in a marker house");
		ctx.EqualStr("", LF_PlacementPolicy.Judge(LF_EPlacementWho.HOUSE_OWNER_OR_KEY, holder, toast), "a key holder is offered it");
		ctx.True(!LF_PlacementPolicy.Judge(LF_EPlacementWho.HOUSE_OWNER_OR_KEY, stranger, toast).IsEmpty(), "a stranger is not");

		house.SetOwner("");
		SCR_EntityHelper.DeleteEntityAndChildren(marker);
	}
}
