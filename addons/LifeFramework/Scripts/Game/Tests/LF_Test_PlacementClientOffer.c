// red-proof: RED-OK placement/client-offer (tools/cli test --no-build --red LF_Test_PlacementClientOffer, 2026-11-19: "expected '', got 'has no house within 30 m (rule HOUSE_OWNER_OR_KEY)'" for the owner and the key holder; the hook skips the client branch).
//! placement/client-offer (#3361): furniture could not be placed from a dedicated-server client. The
//! HOUSE_OWNER_OR_KEY rule read LF_HouseManagerComponent.Live(), a table only the server fills, so the
//! prompt never showed on a remote client, even in the player's own house. The client branch now reads the
//! house managers around the player and the identifier and owner key they replicate. The fixture house keeps
//! a random identifier, so it is NOT in the live table: only the replicated fields can answer.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlacementClientOffer : LF_Test
{
	protected static const ResourceName BUILDING = "{74676598DF38F201}Prefabs/Structures/Core/EL_Building_Base.et";
	protected static const string OWNER = "client-offer-owner";
	protected static const string STRANGER = "client-offer-stranger";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "placement/client-offer";
	}

	//------------------------------------------------------------------------------------------------
	protected string Verdict(vector at, string characterKey, notnull array<string> heldKeys)
	{
		LF_PlacementWhoFacts facts = new LF_PlacementWhoFacts();
		if (!RedProof())
			LF_PlacementPolicy.GatherHouseClientFor(at, characterKey, heldKeys, facts);

		string toast;
		return LF_PlacementPolicy.Judge(LF_EPlacementWho.HOUSE_OWNER_OR_KEY, facts, toast);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource res = Resource.Load(BUILDING);
		ctx.True(res && res.IsValid(), "the building prefab loads");
		if (ctx.FailureCount() > 0)
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		params.Transform[3] = Vector(120, 0, 120);
		params.Transform[3][1] = GetGame().GetWorld().GetSurfaceY(120, 120);
		IEntity building = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(building, "the building spawns");
		if (!building)
			return;

		LF_HouseManagerComponent house = LF_Component<LF_HouseManagerComponent>.Find(building);
		ctx.NotNull(house, "the building carries LF_HouseManagerComponent");
		if (!house)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(building);
			return;
		}

		string houseId = house.GetHouseIdentifier();
		ctx.False(houseId.IsEmpty(), "the house has a replicated identifier");
		ctx.False(LF_HouseManagerComponent.Live().Contains(houseId), "the house is not in the server's live table, so only replicated state can answer");

		house.SetOwner(OWNER);
		vector inside = building.GetOrigin() + "2 0 2";
		array<string> noKeys = {};
		array<string> keys = {houseId};
		array<string> otherKey = {"some-other-house"};

		ctx.EqualStr("", Verdict(inside, OWNER, noKeys), "the owner is offered the placement from replicated state");
		ctx.EqualStr("", Verdict(inside, STRANGER, keys), "a key holder is offered it");
		ctx.True(!Verdict(inside, STRANGER, noKeys).IsEmpty(), "a stranger is not");
		ctx.True(!Verdict(inside, STRANGER, otherKey).IsEmpty(), "a key to another house does not offer it");
		ctx.True(!Verdict(inside + "500 0 500", OWNER, noKeys).IsEmpty(), "the owner far from the house is not offered it");

		house.SetOwner("");
		ctx.True(!Verdict(inside, OWNER, noKeys).IsEmpty(), "after the house is sold the old owner is not offered it");

		SCR_EntityHelper.DeleteEntityAndChildren(building);
	}
}
