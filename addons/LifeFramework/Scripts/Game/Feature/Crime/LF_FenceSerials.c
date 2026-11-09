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
	//! Books the next serial to a buyer. \return The serial, or empty when there is no buyer.
	static string Issue(string buyerUid, string prefab)
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
		row.m_sSerial = "LF-" + number.ToString(4, true);
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
	//! A fence sale: stamp each delivered tool still without a serial and book it to the buyer.
	//! \return False when a delivered tool could not be stamped.
	static bool OnSale(notnull LF_OnFenceSale sale)
	{
		IEntity buyer = FindPlayer(sale.m_sBuyerUid);
		InventoryStorageManagerComponent inventory = null;
		if (buyer)
			inventory = LF_InventoryUtils.GetResponsibleStorageManager(buyer);

		if (!inventory)
		{
			LF_Debug.Error(FEATURE, string.Format("sale of %1 to %2 not stamped: the buyer is not in the world", sale.m_sPrefab, sale.m_sBuyerUid));
			return false;
		}

		int stamped;
		array<IEntity> items = LF_InventoryUtils.FindItemsByPrefab(inventory, sale.m_sPrefab);
		foreach (IEntity item : items)
		{
			if (stamped >= sale.m_iQuantity)
				break;

			LF_SerialComponent component = LF_SerialComponent.Cast(item.FindComponent(LF_SerialComponent));
			if (!component || !component.GetSerial().IsEmpty())
				continue;

			string serial = Issue(sale.m_sBuyerUid, sale.m_sPrefab);
			if (serial.IsEmpty() || !component.SetSerial(serial))
				continue;

			stamped++;
		}

		if (stamped < sale.m_iQuantity)
		{
			LF_Debug.Error(FEATURE, string.Format("sale of %1 to %2: stamped %3 of %4 tool(s)", sale.m_sPrefab, sale.m_sBuyerUid, stamped, sale.m_iQuantity));
			return false;
		}

		return true;
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
