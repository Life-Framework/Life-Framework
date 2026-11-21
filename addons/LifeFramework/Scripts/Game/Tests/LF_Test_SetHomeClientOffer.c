// red-proof: RED-OK property/set-home-client-offer (tools\cli test --no-build --red LF_Test_SetHomeClientOffer, 2026-11-19: "red: a client offers the owner 'Make this my spawn'"); on main LF_SetHomeAction.OfferSet does not exist and the client read the server-only property book, so the owner was never offered "Make this my spawn".
//! property/set-home-client-offer (#3375): "Make this my spawn" on a dedicated client. LF_Properties.GetOwnerKey
//! and SpawnHouseOf read server-only books, so the owner key was always empty on a client and no remote owner
//! saw the action. The client branch now compares the user's key with the house's replicated owner key and skips
//! the spawn-house check; the server's SetSpawnHouse still re-checks ownership. The test runs on the host, so
//! it asks the client branch directly.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SetHomeClientOffer : LF_Test
{
	protected static const ResourceName BUILDING = "{74676598DF38F201}Prefabs/Structures/Core/EL_Building_Base.et";
	protected static const string HOUSE = "set-home-client-house";
	protected static const string OWNER = "set-home-client-owner";
	protected static const string STRANGER = "set-home-client-stranger";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "property/set-home-client-offer";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_HouseLockRecord> housesBefore = LF_Houses.ExportAll();
		array<ref LF_PropertyRecord> propertiesBefore = LF_Properties.ExportAll();
		LF_Houses.ApplyAll(null);
		LF_Properties.ApplyAll(null);

		IEntity building = LF_Utils.SpawnEntityPrefab(BUILDING, "72 0 72");
		ctx.NotNull(building, "the house spawns");
		if (building)
		{
			LF_HouseManagerComponent house = LF_Component<LF_HouseManagerComponent>.Find(building);
			ctx.NotNull(house, "the house carries a house manager");
			if (house)
				RunHouse(ctx, house);

			LF_TestCharacterFixture.Delete(building);
		}

		LF_Properties.ApplyAll(propertiesBefore);
		LF_Houses.ApplyAll(housesBefore);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHouse(LF_TestContext ctx, LF_HouseManagerComponent house)
	{
		ctx.True(house.BindIdentifier(HOUSE), "the house binds a stable identifier");
		house.SetOwner(OWNER);

		ctx.False(LF_SetHomeAction.OfferSet(null, OWNER, true), "no house offers nothing");
		ctx.False(LF_SetHomeAction.OfferSet(house, "", true), "an unknown user is not offered it");

		bool owner = false;
		if (!RedProof())
			owner = LF_SetHomeAction.OfferSet(house, OWNER, true);
		ctx.True(owner, "a client offers the owner 'Make this my spawn'");
		ctx.False(LF_SetHomeAction.OfferSet(house, STRANGER, true), "a client does not offer it to a stranger");

		// The client skips the server-only spawn-house check: the owner is still offered a house that is
		// already the spawn, while the server hides it.
		ctx.True(LF_Properties.SetSpawnHouse(OWNER, HOUSE), "the server sets the spawn house");
		ctx.False(LF_SetHomeAction.OfferSet(house, OWNER, false), "the server hides the action on the current spawn house");
		ctx.True(LF_SetHomeAction.OfferSet(house, OWNER, true), "the client does not read the spawn book");

		// The server re-checks ownership: a stranger's request is refused.
		ctx.False(LF_Properties.SetSpawnHouse(STRANGER, HOUSE), "the server refuses a stranger");

		// A sale changes the replicated owner, and the offer follows it.
		house.SetOwner(STRANGER);
		ctx.False(LF_SetHomeAction.OfferSet(house, OWNER, true), "the old owner is no longer offered it after a sale");
		ctx.True(LF_SetHomeAction.OfferSet(house, STRANGER, true), "the new owner is offered it");
	}
}
