// LF_ShoulderCarry.c: shoulder carry rules (#3263). Hoist a log (later an ore sack or a crate) onto the
// shoulder through the hand-carry slot, walk slowly, drop it. The rules are static and run on the machine
// that owns the carrier; the item's LF_ShoulderCarryComponent applies the speed cap on every machine
// from the slot change, so no RPC is needed.

enum LF_EShoulderRefusal
{
	NONE,
	NO_ITEM,
	NOT_CARRYABLE,
	NO_CARRIER,
	CARRIER_DOWN,
	ALREADY_CARRYING,
	ITEM_HELD,
	TOO_FAR,
	NO_HANDS
}

//------------------------------------------------------------------------------------------------
class LF_ShoulderCarry
{
	static const string FEATURE = "Carry";

	//! How far from the carrier the item may lie and still be hoisted (metres).
	static const float REACH = 4.0; // lf-fixed: the action's own reach is about 2 m; this only stops a far-away script call

	//! The slowest a profile may ask for, as a fraction of the jog speed.
	static const float MIN_CAP = 0.1; // lf-fixed: keeps a bad prefab value from freezing a player

	protected static int s_iHoists;

	//------------------------------------------------------------------------------------------------
	//! Pure: the one rule every caller (action, test, truck bed) shares. First refusal wins.
	static LF_EShoulderRefusal Decide(bool hasItem, bool hasProfile, bool hasCarrier, bool carrierDown, bool hasHands, bool alreadyCarrying, bool itemHeld, float distance)
	{
		if (!hasItem)
			return LF_EShoulderRefusal.NO_ITEM;

		if (!hasProfile)
			return LF_EShoulderRefusal.NOT_CARRYABLE;

		if (!hasCarrier)
			return LF_EShoulderRefusal.NO_CARRIER;

		if (carrierDown)
			return LF_EShoulderRefusal.CARRIER_DOWN;

		if (!hasHands)
			return LF_EShoulderRefusal.NO_HANDS;

		if (alreadyCarrying)
			return LF_EShoulderRefusal.ALREADY_CARRYING;

		if (itemHeld)
			return LF_EShoulderRefusal.ITEM_HELD;

		if (distance > REACH)
			return LF_EShoulderRefusal.TOO_FAR;

		return LF_EShoulderRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a profile's speed cap kept inside what a player can walk at.
	static float ClampCap(float cap)
	{
		if (cap < MIN_CAP)
			return MIN_CAP;

		if (cap > 1)
			return 1;

		return cap;
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalText(LF_EShoulderRefusal refusal)
	{
		switch (refusal)
		{
			case LF_EShoulderRefusal.NO_ITEM: return "no item";
			case LF_EShoulderRefusal.NOT_CARRYABLE: return "the item has no shoulder-carry profile";
			case LF_EShoulderRefusal.NO_CARRIER: return "no carrier";
			case LF_EShoulderRefusal.CARRIER_DOWN: return "the carrier is down";
			case LF_EShoulderRefusal.ALREADY_CARRYING: return "the carrier already carries a load";
			case LF_EShoulderRefusal.ITEM_HELD: return "the item is already in someone's hands or pack";
			case LF_EShoulderRefusal.TOO_FAR: return "the item is out of reach";
			case LF_EShoulderRefusal.NO_HANDS: return "the carrier has no hand-carry storage";
		}

		return "ok";
	}

	//------------------------------------------------------------------------------------------------
	//! The item `character` carries on the shoulder, or null.
	static IEntity Carried(IEntity character)
	{
		if (!character)
			return null;

		LF_HandInventoryStorageComponent hands = LF_Component<LF_HandInventoryStorageComponent>.Find(character);
		if (!hands)
			return null;

		array<IEntity> items();
		hands.GetAll(items);
		foreach (IEntity item : items)
		{
			if (item && LF_Component<LF_ShoulderCarryComponent>.Find(item))
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether `character` could hoist `item` now; logs the reason when not.
	static LF_EShoulderRefusal Check(IEntity character, IEntity item)
	{
		bool hasHands = false;
		float distance = 0;
		if (character)
			hasHands = LF_Component<LF_HandInventoryStorageComponent>.Find(character) != null;

		if (character && item)
			distance = vector.Distance(character.GetOrigin(), item.GetOrigin());

		bool held = false;
		if (item)
		{
			InventoryItemComponent inv = LF_Component<InventoryItemComponent>.Find(item);
			held = inv && inv.GetParentSlot() != null;
		}

		LF_EShoulderRefusal refusal = Decide(item != null, item && LF_Component<LF_ShoulderCarryComponent>.Find(item) != null, character != null, character && LF_ParcelComponent.CarrierIsDown(character), hasHands, Carried(character) != null, held, distance);
		if (refusal != LF_EShoulderRefusal.NONE)
			LF_Debug.Warn(FEATURE, "hoist refused: " + RefusalText(refusal));

		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts `item` on `character`'s shoulder: into the hand-carry storage (the weapon is holstered and the
	//! hands are busy by LF_HandInventoryStorageComponent), then the item's component caps the speed.
	static bool Hoist(IEntity character, IEntity item)
	{
		if (Check(character, item) != LF_EShoulderRefusal.NONE)
			return false;

		LF_HandInventoryStorageComponent hands = LF_Component<LF_HandInventoryStorageComponent>.Find(character);
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!manager)
		{
			LF_Debug.Error(FEATURE, "hoist refused: the carrier has no inventory storage manager");
			return false;
		}

		if (!manager.TryInsertItemInStorage(item, hands))
		{
			LF_Debug.Warn(FEATURE, "hoist refused: the hand-carry storage did not take the item");
			return false;
		}

		s_iHoists = s_iHoists + 1;
		LF_Debug.Info(FEATURE, string.Format("hoisted %1 (%2)", LF_Utils.GetPrefabName(item), s_iHoists));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the carried load down at the carrier's feet. The item's slot change lifts the speed cap.
	static bool Drop(IEntity character)
	{
		IEntity item = Carried(character);
		if (!item)
			return false;

		if (!LF_InventoryUtils.DropItem(character, item))
		{
			LF_Debug.Warn(FEATURE, "drop refused: the item did not leave the hand-carry storage");
			return false;
		}

		LF_Debug.Info(FEATURE, string.Format("dropped %1", LF_Utils.GetPrefabName(item)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iHoists = 0;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ShoulderCarry's statics (LF_StaticReset finds it by reflection).
class LF_ShoulderCarry_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ShoulderCarry";
	}

	override void Apply()
	{
		LF_ShoulderCarry.ResetStatics();
	}
}
