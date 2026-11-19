// red-proof: 2026-11-19 `tools\cli test --no-build --red LF_Test_HouseMarker` (batched with a pack test) printed `RED PROOF OK` (#3584): with the marker's RegisterNow skipped the building is never locked, so `a stranger is refused at the door of an unowned house` fails. Green with the hook off.
//! houses/house-marker (#3584): a house marker (LF_HouseMarker.et: a lock marker plus LF_HouseManagerComponent) inside a
//! vanilla house shuts every door for strangers. Its owner passes without a key, a key holder passes, and buying the
//! house at its sign makes the buyer the owner and gives them the key while a stranger stays out with the owned-house reason.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_HouseMarker : LF_Test
{
	protected static const ResourceName HOUSE = "{EDBC0E94793BA9F1}Prefabs/Structures/Houses/Village/House_Village_E_1I01/House_Village_E_1I01.et";
	protected static const ResourceName MARKER = "{4D1B7E92A6C03F58}Prefabs/Systems/Houses/LF_HouseMarker.et";
	protected static const ResourceName SIGN = "{2F815241C89039BC}Prefabs/Props/Property/LF_HouseSaleSign.et";
	protected static const string HOUSE_ID = "sale-house-a";
	protected static const string OWNER_UID = "lf-test-marker-owner";
	protected static const string BUYER_UID = "lf-test-marker-buyer";
	protected static const string STRANGER_UID = "lf-test-marker-stranger";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "houses/house-marker";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PropertyConfig.Reload();
		LF_Properties.ResetStatics();
		BaseWorld world = GetGame().GetWorld();
		vector at = "60 0 140";
		at[1] = world.GetSurfaceY(at[0], at[2]);
		IEntity house = LF_Utils.SpawnEntityPrefab(HOUSE, at);
		ctx.NotNull(house, "a vanilla house spawns");
		IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity stranger = LF_TestCharacterFixture.Spawn(ctx);
		IEntity marker;
		IEntity sign;
		if (house && owner && buyer && stranger)
			RunHouse(ctx, house, owner, buyer, stranger, marker, sign);

		LF_TestCharacterFixture.Delete(owner);
		LF_TestCharacterFixture.Delete(buyer);
		LF_TestCharacterFixture.Delete(stranger);
		if (sign)
			SCR_EntityHelper.DeleteEntityAndChildren(sign);

		if (marker)
			SCR_EntityHelper.DeleteEntityAndChildren(marker);

		if (house)
			SCR_EntityHelper.DeleteEntityAndChildren(house);

		// The house book keeps a row for the example house id; drop it (the Houses feature's own statics, never ResetAll).
		LF_Houses.ResetStatics();
		LF_Properties.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHouse(LF_TestContext ctx, IEntity house, IEntity owner, IEntity buyer, IEntity stranger, out IEntity marker, out IEntity sign)
	{
		array<IEntity> doors = {};
		CollectDoors(house, doors);
		ctx.True(doors.Count() > 0, "the house has child doors with a door action");
		if (doors.Count() == 0)
			return;

		IEntity door = doors[0];
		SCR_DoorUserAction action = DoorAction(door);
		vector mins;
		vector maxs;
		house.GetWorldBounds(mins, maxs);
		marker = LF_Utils.SpawnEntityPrefab(MARKER, (mins + maxs) * 0.5);
		ctx.NotNull(marker, "the house marker spawns");
		if (!marker)
			return;

		LF_BuildingLockComponent lock = LF_Component<LF_BuildingLockComponent>.Find(marker);
		LF_HouseManagerComponent manager = LF_Component<LF_HouseManagerComponent>.Find(marker);
		ctx.NotNull(lock, "the marker carries the lock component");
		ctx.NotNull(manager, "and a house manager");
		if (!lock || !manager)
			return;

		ctx.True(manager.BindIdentifier(HOUSE_ID), "the house takes its identifier");
		if (!RedProof())
			ctx.True(lock.RegisterNow(), "the marker finds and locks its building");

		LF_Houses.BindCharacter(owner, OWNER_UID);
		LF_Houses.BindCharacter(buyer, BUYER_UID);
		LF_Houses.BindCharacter(stranger, STRANGER_UID);

		ctx.False(action.CanBePerformedScript(stranger), "a stranger is refused at the door of an unowned house");
		ctx.EqualStr(LF_BuildingLocks.KEY_PRIVATE, LF_BuildingLocks.ReasonFor(door, stranger), "with the marker's sign key as the reason");

		// The owner passes without a key; the stranger still does not.
		manager.SetOwner(OWNER_UID);
		ctx.True(action.CanBePerformedScript(owner), "the owner passes a door of their house");
		ctx.False(action.CanBePerformedScript(stranger), "a stranger is still refused");
		ctx.EqualStr("", LF_BuildingLocks.ReasonFor(door, owner), "no reason for the owner");
		manager.SetOwner("");

		// Buy it at the sign: the buyer is the owner and holds the key.
		int price = LF_PropertyConfig.HousePrice(HOUSE_ID);
		ctx.True(price > 0, "the example house has a price");
		sign = LF_Utils.SpawnEntityPrefab(SIGN, buyer.GetOrigin() + "1 0 0");
		LF_HouseSaleComponent saleSign;
		if (sign)
			saleSign = LF_Component<LF_HouseSaleComponent>.Find(sign);

		ctx.NotNull(saleSign, "the For sale sign spawns");
		if (!saleSign || price <= 0)
			return;

		saleSign.SetHouseId(HOUSE_ID);
		int carried = LF_MoneyUtils.GetCash(buyer);
		if (carried > 0)
			LF_MoneyUtils.RemoveCash(buyer, carried);

		LF_MoneyUtils.AddCash(buyer, price + 100);
		string soldId;
		ctx.Equal(LF_EHouseBuy.BOUGHT, LF_HouseSaleComponent.Buy(buyer, BUYER_UID, soldId, HOUSE_ID), "the buyer buys the house at its sign");
		ctx.EqualStr(BUYER_UID, manager.GetOwnerKey(), "the marker's house is theirs");
		ctx.True(action.CanBePerformedScript(buyer), "the buyer opens the door");
		ctx.False(action.CanBePerformedScript(stranger), "a stranger is refused at the door of the sold house");
		ctx.EqualStr(LF_BuildingLocks.KEY_PRIVATE, LF_BuildingLocks.ReasonFor(door, stranger), "with the owned-house reason");

		// The key opens it for someone who is not the owner: the owner leaves, the key stays in the buyer's pocket.
		manager.SetOwner(OWNER_UID);
		ctx.True(action.CanBePerformedScript(buyer), "a key bound to the house opens its door");
		ctx.False(action.CanBePerformedScript(stranger), "and a stranger without a key does not");

		// Leave the shared example house unowned: later tests sell it too.
		manager.SetOwner("");
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_DoorUserAction DoorAction(IEntity door)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(door.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return null;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			SCR_DoorUserAction doorAction = SCR_DoorUserAction.Cast(action);
			if (doorAction)
				return doorAction;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void CollectDoors(IEntity entity, notnull array<IEntity> doors)
	{
		if (DoorAction(entity) && entity.FindComponent(BaseDoorComponent))
			doors.Insert(entity);

		IEntity child = entity.GetChildren();
		while (child)
		{
			CollectDoors(child, doors);
			child = child.GetSibling();
		}
	}
}
