// red-proof: with `RedProof()` on no vehicle is registered, so the owner never owns one: the "owns a registered car" check
// fails (the starter car would be granted a second time to a player who already has a car).

//! vehicles/owns-any: LF_VehicleOwnership.OwnsAny, the gate behind the starter car: an owner with ANY vehicle record, stored,
//! out on the street or impounded, is not given another free one. An empty owner owns nothing, another owner's car is not
//! this owner's (and a longer uid that starts with this one is a different owner), and scrapping the last car makes the
//! owner a car-less one again.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_VehicleOwnsAny : LF_Test
{
	protected static const string OWNER = "owns-any-owner";
	protected static const string OWNER_LONGER = "owns-any-owner-two";
	protected static const string CAR_A = "owns-any-car-a";
	protected static const string CAR_B = "owns-any-car-b";
	protected static const string PREFAB = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";

	override string GetName()
	{
		return "vehicles/owns-any";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		bool emptyOwns = true;
		bool beforeOwns = true;
		bool registered = false;
		bool afterOwns = false;
		bool otherOwns = true;
		bool longerOwns = true;
		bool emptyAfterOwns = true;
		bool stillOwnsWithOne = false;
		bool scrappedA = false;
		bool scrappedB = false;
		bool ownsAfterScrap = true;
		if (!RedProof())
		{
			emptyOwns = LF_VehicleOwnership.OwnsAny("");
			beforeOwns = LF_VehicleOwnership.OwnsAny(OWNER);
			registered = LF_VehicleOwnership.Register(CAR_A, OWNER, CAR_A, PREFAB, "", "owns-any-shop", "", "#EL-Vehicle_S105", true);
			afterOwns = LF_VehicleOwnership.OwnsAny(OWNER);
			otherOwns = LF_VehicleOwnership.OwnsAny("owns-any-nobody");
			longerOwns = LF_VehicleOwnership.OwnsAny(OWNER_LONGER);
			emptyAfterOwns = LF_VehicleOwnership.OwnsAny("");

			LF_VehicleOwnership.Register(CAR_B, OWNER, CAR_B, PREFAB, "", "owns-any-shop", "", "#EL-Vehicle_S105", true);
			scrappedA = LF_VehicleOwnership.Scrap(CAR_A);
			stillOwnsWithOne = LF_VehicleOwnership.OwnsAny(OWNER);
			scrappedB = LF_VehicleOwnership.Scrap(CAR_B);
			ownsAfterScrap = LF_VehicleOwnership.OwnsAny(OWNER);
		}

		ctx.False(emptyOwns, "an empty owner owns nothing");
		ctx.False(beforeOwns, "an owner with no record owns no car");
		ctx.True(registered, "the car registers");
		ctx.True(afterOwns, "an owner with a registered car owns one");
		ctx.False(otherOwns, "another owner's car is not theirs");
		ctx.False(longerOwns, "a uid that merely starts with the owner's is a different owner");
		ctx.False(emptyAfterOwns, "an empty owner still owns nothing with cars in the world");
		ctx.True(scrappedA, "the first car is scrapped");
		ctx.True(stillOwnsWithOne, "scrapping one of two cars leaves the owner with a car");
		ctx.True(scrappedB, "the second car is scrapped");
		ctx.False(ownsAfterScrap, "with every car scrapped the owner is car-less again");
	}
}
