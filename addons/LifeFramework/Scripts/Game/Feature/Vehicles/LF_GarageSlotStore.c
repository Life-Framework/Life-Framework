// LF_GarageSlotStore.c — garage slots a character bought with carried cash (#2656, spec #774).
//
// Server only. Keyed by the character record key (LF_Utils.GetPlayerUID), never an entity. The
// count rides the "garageslots" payload (save version 55); LF_GarageSlots.BoughtSlots reads it, so
// the cap the store action checks rises the moment a slot is bought. The price is LF_GarageSlots'
// next-slot price from LF_Garage.conf; the max refuses past m_iMaxSlots.

//------------------------------------------------------------------------------------------------
//! Persisted bought slots of one character: the "garageslots" payload of LF_PersistenceComponentSerializer.
//! Positional in a binary save: never add, remove or reorder a field. A new field is a new payload.
class LF_GarageSlotRow
{
	//! Row format, written first. 1 is this shape.
	int m_iVersion;
	string m_sCharacterKey;
	int m_iBought;

	static const int ROW_VERSION = 1;

	//------------------------------------------------------------------------------------------------
	static LF_GarageSlotRow Create(string characterKey, int bought)
	{
		LF_GarageSlotRow row = new LF_GarageSlotRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sCharacterKey = characterKey;
		row.m_iBought = bought;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Why a slot buy ended the way it did.
enum LF_EGarageSlotBuy
{
	BOUGHT,
	NO_CHARACTER,	//!< No buyer entity or no character key
	AT_MAX,			//!< The character already holds m_iMaxSlots
	SHORT_CASH,		//!< Carried cash is below the price; nothing taken
	PAYMENT_FAILED	//!< The cash could not be taken in full; what was taken is handed back
}

//------------------------------------------------------------------------------------------------
class LF_GarageSlotStore
{
	static const string FEATURE = "Garage";
	static const string LEDGER_REASON = "garage-slot";

	protected static ref map<string, int> s_mBought = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	//! \return Slots characterKey bought; 0 for an unknown character.
	static int Bought(string characterKey)
	{
		if (characterKey.IsEmpty())
			return 0;

		int bought;
		if (s_mBought.Find(characterKey, bought))
			return bought;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Most slots a character can buy: the max minus the free base slots.
	static int MaxBuyable()
	{
		int most = LF_GarageConfig.MaxSlots() - LF_GarageConfig.BaseSlots();
		if (most < 0)
			return 0;

		return most;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: buy characterKey's next slot with buyer's carried cash.
	//! \param[out] price The price asked (the next slot's), or -1 at the max.
	static LF_EGarageSlotBuy Buy(IEntity buyer, string characterKey, out int price)
	{
		price = -1;
		if (!buyer || characterKey.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "slot buy refused: no buyer or no character key");
			return LF_EGarageSlotBuy.NO_CHARACTER;
		}

		int slotsHeld = LF_GarageSlots.CapOf(characterKey);
		price = LF_GarageSlots.NextPriceFor(characterKey);
		if (price < 0 || Bought(characterKey) >= MaxBuyable())
		{
			LF_Debug.Info(FEATURE, string.Format("slot buy refused: %1 already holds %2 slots, the max", characterKey, slotsHeld));
			return LF_EGarageSlotBuy.AT_MAX;
		}

		int cash = LF_MoneyUtils.GetCash(buyer);
		if (cash < price)
		{
			LF_Debug.Info(FEATURE, string.Format("slot buy refused: %1 carries %2 of the %3 price", characterKey, cash, price));
			return LF_EGarageSlotBuy.SHORT_CASH;
		}

		if (price > 0)
		{
			int removed = LF_MoneyUtils.RemoveCash(buyer, price, LEDGER_REASON);
			if (removed != price)
			{
				if (removed > 0)
					LF_MoneyUtils.AddCash(buyer, removed, LEDGER_REASON + "-rollback");

				LF_Debug.Warn(FEATURE, string.Format("slot buy refused: took %1 of the %2 price from %3, handed it back", removed, price, characterKey));
				return LF_EGarageSlotBuy.PAYMENT_FAILED;
			}
		}

		s_mBought.Set(characterKey, Bought(characterKey) + 1);
		LF_Debug.Info(FEATURE, string.Format("slot bought: %1 paid %2, now %3 slots", characterKey, price, LF_GarageSlots.CapOf(characterKey)));
		return LF_EGarageSlotBuy.BOUGHT;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops one character's bought slots (tests).
	static void Forget(string characterKey)
	{
		s_mBought.Remove(characterKey);
	}

	//------------------------------------------------------------------------------------------------
	//! \return One row per character with at least one bought slot.
	static array<ref LF_GarageSlotRow> ExportAll()
	{
		array<ref LF_GarageSlotRow> rows = new array<ref LF_GarageSlotRow>();
		foreach (string key, int bought : s_mBought)
		{
			if (bought > 0)
				rows.Insert(LF_GarageSlotRow.Create(key, bought));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the store with rows (null empties it). Idempotent. A row with no key or no slot is
	//! skipped; a count past what the config now allows is clamped, so a lowered max never over-grants.
	//! \return Rows kept.
	static int ApplyAll(array<ref LF_GarageSlotRow> rows)
	{
		s_mBought.Clear();
		if (!rows)
			return 0;

		int most = MaxBuyable();
		int kept = 0;
		foreach (LF_GarageSlotRow row : rows)
		{
			if (!row || row.m_sCharacterKey.IsEmpty() || row.m_iBought <= 0)
				continue;

			int bought = row.m_iBought;
			if (bought > most)
			{
				LF_Debug.Warn(FEATURE, string.Format("%1 had %2 bought slots, the config now allows %3: clamped", row.m_sCharacterKey, bought, most));
				bought = most;
			}

			if (bought <= 0)
				continue;

			s_mBought.Set(row.m_sCharacterKey, bought);
			kept++;
		}

		return kept;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mBought = new map<string, int>();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_GarageSlotStore's statics (LF_StaticReset finds it by reflection).
class LF_GarageSlotStore_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_GarageSlotStore";
	}

	override void Apply()
	{
		LF_GarageSlotStore.ResetStatics();
	}
}
