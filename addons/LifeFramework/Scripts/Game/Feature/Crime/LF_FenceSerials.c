//------------------------------------------------------------------------------------------------
//! One row of the serial book: which account bought which serial. Plain values, persisted as is.
class LF_FenceSerialRow
{
	int m_iNumber;
	string m_sSerial;
	string m_sBuyerUid;
	string m_sPrefab;

	LF_FenceSerialRow Copy()
	{
		LF_FenceSerialRow copy = new LF_FenceSerialRow();
		copy.m_iNumber = m_iNumber;
		copy.m_sSerial = m_sSerial;
		copy.m_sBuyerUid = m_sBuyerUid;
		copy.m_sPrefab = m_sPrefab;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! The bus subscriber: the fence's sale event stamps the tools it delivered.
class LF_FenceSerialSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnFenceSale sale = LF_OnFenceSale.Cast(busEvent);
		if (!sale)
			return false;

		return LF_FenceSerials.OnSale(sale);
	}
}

//------------------------------------------------------------------------------------------------
//! Serial book for fence tools (#272). Each tool the fence sells gets a serial (LF_SerialComponent)
//! and a row naming the buyer account (server only, never replicated). A tool found at a crime
//! scene and checked at the evidence locker names its buyer: Identify marks that account's wanted
//! reports identified. Persisted as the "fenceserials" payload. The rows hold the account uid and
//! serial text only; the item carries the same serial and is resolved by it on load.
class LF_FenceSerials
{
	static const string FEATURE = "Serials";

	protected static ref array<ref LF_FenceSerialRow> s_aRows;
	protected static ref LF_FenceSerialSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_FenceSerialRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_FenceSerialRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes to LF_OnFenceSale once. Called when the game mode starts.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.SubscriberCount(LF_OnFenceSale) > 0)
			return true;

		s_Subscriber = new LF_FenceSerialSubscriber();
		return LF_EventBus.Subscribe(LF_OnFenceSale, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! Books the next serial to a buyer. \param[in] outlet Catalogue outlet that sold or issued it;
	//! its id prefixes the serial (LF_OutletSerial, #1765). Empty keeps the old "LF-0001" shape.
	//! \return The serial, or empty when there is no buyer.
	static string Issue(string buyerUid, string prefab, string outlet = "")
	{
		if (buyerUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "no serial issued: the sale has no buyer account");
			return string.Empty;
		}

		int number = 1;
		foreach (LF_FenceSerialRow existing : Rows())
		{
			if (existing.m_iNumber >= number)
				number = existing.m_iNumber + 1;
		}

		LF_FenceSerialRow row = new LF_FenceSerialRow();
		row.m_iNumber = number;
		row.m_sSerial = LF_OutletSerial.Mint(outlet, number);
		row.m_sBuyerUid = buyerUid;
		row.m_sPrefab = prefab;
		Rows().Insert(row);
		LF_Debug.Info(FEATURE, string.Format("serial %1 issued to %2 (%3)", row.m_sSerial, buyerUid, prefab));
		return row.m_sSerial;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The book row for a serial, or null when the book has none.
	static LF_FenceSerialRow Find(string serial)
	{
		if (serial.IsEmpty())
			return null;

		foreach (LF_FenceSerialRow row : Rows())
		{
			if (row.m_sSerial == serial)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the serial names its buyer; every open wanted report of that account is identified.
	//! \param[in] serial Serial read off a bagged tool.
	//! \param[in] how Who checked it, for the log (the evidence locker).
	//! \return Reports newly identified, 0 for an unknown serial or nothing open.
	static int Identify(string serial, string how)
	{
		LF_FenceSerialRow row = Find(serial);
		if (!row)
		{
			LF_Debug.Info(FEATURE, string.Format("serial '%1' checked by %2: not in the book", serial, how));
			return 0;
		}

		LF_Debug.Info(FEATURE, string.Format("serial %1 checked by %2: bought by %3", serial, how, row.m_sBuyerUid));
		return LF_WantedReports.Identify(row.m_sBuyerUid, "serial " + serial);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: reads the serial off a tool entity and identifies its buyer. The evidence locker
	//! (#269) calls this for each bagged item it checks.
	static int IdentifyItem(IEntity item, string how)
	{
		if (!item)
			return 0;

		LF_SerialComponent serial = LF_SerialComponent.Cast(item.FindComponent(LF_SerialComponent));
		if (!serial || serial.GetSerial().IsEmpty())
			return 0;

		return Identify(serial.GetSerial(), how);
	}

	//------------------------------------------------------------------------------------------------
	//! A fence sale: stamp each delivered tool still without a serial and book it to the buyer, naming
	//! the outlet that sold it (#2153). eturn False when a delivered tool could not be stamped.
	static bool OnSale(notnull LF_OnFenceSale sale)
	{
		IEntity buyer = FindPlayer(sale.m_sBuyerUid);
		if (!buyer)
		{
			LF_Debug.Error(FEATURE, string.Format("sale of %1 to %2 not stamped: the buyer is not in the world", sale.m_sPrefab, sale.m_sBuyerUid));
			return false;
		}

		int stamped = StampDelivered(buyer, sale.m_sBuyerUid, sale.m_sPrefab, sale.m_iQuantity, sale.m_sOutlet);
		if (stamped < sale.m_iQuantity)
		{
			LF_Debug.Error(FEATURE, string.Format("sale of %1 to %2: stamped %3 of %4 tool(s)", sale.m_sPrefab, sale.m_sBuyerUid, stamped, sale.m_iQuantity));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a weapon sold at an outlet that is not a fence (the licensed gun counter, #2153): the
	//! delivered weapon is registered to its buyer with the outlet in its serial. A weapon prefab with
	//! no LF_SerialComponent cannot carry one; that is logged and the sale stands.
	//! eturn Weapons stamped.
	static int OnWeaponSale(IEntity buyer, string buyerUid, string prefab, int quantity, string outlet)
	{
		int stamped = StampDelivered(buyer, buyerUid, prefab, quantity, outlet);
		if (stamped < quantity)
			LF_Debug.Warn(FEATURE, string.Format("weapon sale of %1 at %2 to %3: stamped %4 of %5 (a weapon prefab needs LF_SerialComponent)", prefab, outlet, buyerUid, stamped, quantity));

		return stamped;
	}

	//------------------------------------------------------------------------------------------------
	//! Stamps up to quantity delivered items of a prefab that carry an empty serial component.
	//! eturn Items stamped.
	protected static int StampDelivered(IEntity buyer, string buyerUid, string prefab, int quantity, string outlet)
	{
		if (!buyer)
			return 0;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(buyer);
		if (!inventory)
			return 0;

		// A bought weapon lands in the weapon slot when one is free, which the bag search does not see.
		array<IEntity> items = {};
		items.InsertAll(LF_InventoryUtils.FindItemsByPrefab(inventory, prefab));
		EquipedWeaponStorageComponent weapons = LF_Component<EquipedWeaponStorageComponent>.Find(buyer);
		if (weapons)
		{
			array<IEntity> equipped = {};
			weapons.GetAll(equipped);
			foreach (IEntity worn : equipped)
			{
				if (LF_Utils.SamePrefab(LF_Utils.GetPrefabName(worn), prefab) && items.Find(worn) == -1)
					items.Insert(worn);
			}
		}

		int stamped;
		foreach (IEntity item : items)
		{
			if (stamped >= quantity)
				break;

			LF_SerialComponent component = LF_SerialComponent.Cast(item.FindComponent(LF_SerialComponent));
			if (!component || !component.GetSerial().IsEmpty())
				continue;

			string serial = Issue(buyerUid, prefab, outlet);
			if (serial.IsEmpty() || !component.SetSerial(serial))
				continue;

			stamped++;
		}

		return stamped;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindPlayer(string uid)
	{
		if (uid.IsEmpty())
			return null;

		array<int> playerIds = new array<int>();
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (LF_Utils.GetPlayerUID(playerId) == uid)
				return GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_FenceSerialRow> ExportAll()
	{
		array<ref LF_FenceSerialRow> records = new array<ref LF_FenceSerialRow>();
		foreach (LF_FenceSerialRow row : Rows())
		{
			records.Insert(row.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the book (idempotent). Rows with no serial or buyer, or a repeated serial, are skipped.
	static void ApplyAll(array<ref LF_FenceSerialRow> records)
	{
		s_aRows = new array<ref LF_FenceSerialRow>();
		if (!records)
			return;

		foreach (LF_FenceSerialRow record : records)
		{
			if (!record || record.m_sSerial.IsEmpty() || record.m_sBuyerUid.IsEmpty() || Find(record.m_sSerial))
			{
				LF_Debug.Warn(FEATURE, "load skipped a row with no serial, no buyer or a repeated serial");
				continue;
			}

			s_aRows.Insert(record.Copy());
		}
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FenceSerials's statics (registered at script load; Core/LF_StaticReset.c).
class LF_FenceSerials_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_FenceSerials_StaticResetHook());

	override string Owner()
	{
		return "LF_FenceSerials";
	}

	override void Apply()
	{
		LF_FenceSerials.ResetStatics();
	}
}
