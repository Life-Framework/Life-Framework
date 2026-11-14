//------------------------------------------------------------------------------------------------
//! One unit of goods a player took off a shelf and has not paid for. Held by item entity id, which
//! is only ever read on the server. A consumed item (eaten, used) keeps its record as a debt:
//! m_bConsumed, with the last carrier remembered.
class LF_StoreUnpaidItem : Managed
{
	EntityID m_ItemId;
	string m_sStoreKey;
	ResourceName m_Prefab;
	int m_iPrice;
	//! The character body that holds it now, or the last one for a consumed item. INVALID on the floor.
	EntityID m_CarrierId = EntityID.INVALID;
	//! The carrier's character record key (LF_Utils.GetPlayerUID), what the save keys the tag by. Empty
	//! on the floor or on a body no player controls; such a tag is not saved.
	string m_sCarrierKey;
	bool m_bConsumed;
	//! Put back after a restart or a reconnect, on a body that spawned away from the store: the exit
	//! sweep leaves it alone until its carrier has stood in the store again or the store's grace runs
	//! out (m_fRestoredUntilMs, world time). The till charges it either way.
	bool m_bRestored;
	float m_fRestoredUntilMs;
}

//------------------------------------------------------------------------------------------------
//! The server side of the physical store (#202): the registry of stores, the unpaid-goods tags, the
//! till, the shoplifting sweep and the save bridge. Money is physical cash and every sale is an
//! LF_OnMoneyMoved with the reason "store-sale".
//!
//! UNPAID TAG. The tag is a record per item instance here, not a component on the item: a shelf item
//! is a plain vanilla prefab. The record follows the item (pocket, floor, another player) and a
//! consumed item stays on as a debt, so eating the goods in the shop does not make them free.
//!
//! SHOPLIFTING. A character who carries unpaid goods of a store while standing outside its boundary
//! is a thief: wanted level, a crime report with the alarm at the store, and the report is identified
//! (being seen walking out of a shop counts as identified). The goods become theirs.
//!
//! RESTART AND RECONNECT (#479). A tag on goods a living player carries is saved (the "storeunpaid"
//! payload) and parked on disconnect, keyed by the character record key, the store and the prefab.
//! When that character's body spawns and its carried inventory is back, ReattachUnpaid puts each tag
//! on an untagged item of that prefab it carries; a used-up item comes back as the same debt. A tag
//! whose item or store is gone is dropped with a warning. A restored tag is not swept while its carrier
//! has the store's grace (m_iRestoredGraceSeconds) to come back and pay: the restart or the spawn point
//! moved them, they did not walk out. Once they stand in the store, or the grace runs out, the sweep
//! applies as usual, so logging out inside a store is no way to leave with the goods.
class LF_Stores
{
	static const string FEATURE = "Store";
	static const string SALE_REASON = "store-sale";
	//! The prefab slot (#1128): The receipt a store hands over after a sale.
	static const string SLOT_RECEIPT = "store_receipt";

	//! The prefab (slot "store_receipt"), or empty when no usable prefab resolves (logged).
	static ResourceName ReceiptPrefab()
	{
		return LF_PrefabSlots.Get(SLOT_RECEIPT);
	}
	static const int TICK_MS = 500;
	//! A fresh shelf is filled this long after the store appears, once the world has settled.
	static const int FILL_DELAY_MS = 2000;

	protected static ref array<ref LF_StoreUnpaidItem> s_aUnpaid;
	//! Saved or parked tags whose character has not spawned yet (#479). Saved again until then.
	protected static ref array<ref LF_StoreUnpaidRow> s_aParked;
	protected static ref map<string, ref array<ref LF_StoreStockRow>> s_mPending;
	protected static bool s_bTicking;
	protected static int s_iReceipts;
	protected static EntityID s_LastReceipt = EntityID.INVALID;

	//------------------------------------------------------------------------------------------------
	static map<string, LF_StoreComponent> Stores()
	{
		return LF_StoreComponent.Live();
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_StoreUnpaidItem> Unpaid()
	{
		if (!s_aUnpaid)
			s_aUnpaid = new array<ref LF_StoreUnpaidItem>();

		return s_aUnpaid;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref array<ref LF_StoreStockRow>> Pending()
	{
		if (!s_mPending)
			s_mPending = new map<string, ref array<ref LF_StoreStockRow>>();

		return s_mPending;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bTicking)
			GetGame().GetCallqueue().Remove(Tick);

		LF_StoreComponent.ResetLive();
		s_aUnpaid = null;
		s_aParked = null;
		s_mPending = null;
		s_bTicking = false;
		s_iReceipts = 0;
		s_LastReceipt = EntityID.INVALID;
	}

	//------------------------------------------------------------------------------------------------
	//! eturn The receipt the till printed last (held or lying where the user stood), or null.
	static IEntity LastReceipt()
	{
		if (s_LastReceipt == EntityID.INVALID)
			return null;

		return GetGame().GetWorld().FindEntityByID(s_LastReceipt);
	}

	//------------------------------------------------------------------------------------------------
	static LF_StoreComponent Find(string storeKey)
	{
		return Stores().Get(storeKey);
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Stores().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a store appeared. A saved stock that arrived first is applied; the first fill and the
	//! tick are scheduled.
	static void Register(LF_StoreComponent store)
	{
		if (!store)
			return;

		string key = store.GetStoreKey();
		if (key.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "a store has no store key; it works but its stock is not saved");
			return;
		}

		LF_StoreComponent known = Stores().Get(key);
		if (known && known != store)
		{
			LF_Debug.Error(FEATURE, string.Format("store key %1 is used twice; the second store is not registered", key));
			return;
		}

		Stores().Set(key, store);

		array<ref LF_StoreStockRow> rows = Pending().Get(key);
		if (rows)
		{
			foreach (LF_StoreStockRow row : rows)
				store.ImportRow(row);

			Pending().Remove(key);
		}

		GetGame().GetCallqueue().CallLater(store.FirstFill, FILL_DELAY_MS, false);
		if (!s_bTicking)
		{
			s_bTicking = true;
			GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void Unregister(LF_StoreComponent store)
	{
		if (!store)
			return;

		// Records of a store that is gone can never be paid; they must not make anyone a thief.
		array<ref LF_StoreUnpaidItem> all = Unpaid();
		for (int i = all.Count() - 1; i >= 0; i--)
		{
			if (all[i].m_sStoreKey == store.GetStoreKey())
				all.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Unpaid tags
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: tags an item a player took off a shelf. root is the topmost holder of the item.
	static LF_StoreUnpaidItem Tag(LF_StoreComponent store, IEntity item, ResourceName prefab, int price)
	{
		if (!store || !item)
			return null;

		if (price <= 0)
		{
			LF_Debug.Error(FEATURE, string.Format("store %1: %2 taken off the shelf has no price, so it is free; give it a price in ShopPrices.conf", store.GetStoreKey(), prefab));
			return null;
		}

		LF_StoreUnpaidItem record = new LF_StoreUnpaidItem();
		record.m_ItemId = item.GetID();
		record.m_sStoreKey = store.GetStoreKey();
		record.m_Prefab = prefab;
		record.m_iPrice = price;
		Unpaid().Insert(record);
		RefreshCarrier(record, item);
		LF_Debug.Info(FEATURE, string.Format("store %1: %2 taken off the shelf, unpaid %3 (carrier %4)", record.m_sStoreKey, prefab, price, CarrierText(record)));
		return record;
	}

	//------------------------------------------------------------------------------------------------
	protected static string CarrierText(LF_StoreUnpaidItem record)
	{
		if (record.m_CarrierId == EntityID.INVALID)
			return "none";

		IEntity carrier = GetGame().GetWorld().FindEntityByID(record.m_CarrierId);
		if (!carrier)
			return "gone";

		return ThiefKey(carrier);
	}

	//------------------------------------------------------------------------------------------------
	//! The account uid of a character, or its entity id as text for a body with no player (tests).
	static string ThiefKey(IEntity character)
	{
		string uid = LF_Utils.GetPlayerUID(character);
		if (uid.IsEmpty())
			return character.GetID().ToString();

		return uid;
	}

	//------------------------------------------------------------------------------------------------
	//! Works out who holds a record's item now. A character holding it becomes the carrier; an item on
	//! the floor has none; an item that is gone becomes a debt of its last carrier, or is dropped.
	//! \return False when the record should be removed.
	protected static bool RefreshCarrier(LF_StoreUnpaidItem record, IEntity item)
	{
		if (record.m_bConsumed)
			return GetGame().GetWorld().FindEntityByID(record.m_CarrierId) != null;

		if (!item)
		{
			if (record.m_CarrierId == EntityID.INVALID)
				return false;

			record.m_bConsumed = true;
			LF_Debug.Info(FEATURE, string.Format("store %1: unpaid %2 used up by %3, it stays on their bill", record.m_sStoreKey, record.m_Prefab, CarrierText(record)));
			return true;
		}

		IEntity root = LF_InventoryUtils.GetStorageHierachyRoot(item);
		EntityID carrier = EntityID.INVALID;
		if (root != item && SCR_ChimeraCharacter.Cast(root))
			carrier = root.GetID();

		if (carrier != record.m_CarrierId)
		{
			record.m_CarrierId = carrier;
			record.m_sCarrierKey = "";
		}

		// Asked again while empty: a player whose identity was not resolved at the pickup gets it later.
		if (carrier != EntityID.INVALID && record.m_sCarrierKey.IsEmpty())
			record.m_sCarrierKey = LF_Utils.GetPlayerUID(root);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The unpaid items of a store a character holds now, consumed ones included.
	static void CollectFor(string storeKey, IEntity character, notnull array<LF_StoreUnpaidItem> outItems)
	{
		outItems.Clear();
		if (!character)
			return;

		foreach (LF_StoreUnpaidItem record : Unpaid())
		{
			if (record.m_sStoreKey == storeKey && record.m_CarrierId == character.GetID())
				outItems.Insert(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return Total price of everything of a store the character has not paid for.
	static int OwedBy(string storeKey, IEntity character)
	{
		array<LF_StoreUnpaidItem> items = {};
		CollectFor(storeKey, character, items);
		int total;
		foreach (LF_StoreUnpaidItem record : items)
			total += record.m_iPrice;

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when an item must not be merged into another stack: it stands on a store shelf or
	//! carries an unpaid tag. Server only; the quantity merge asks this on every item added.
	static bool HoldsApart(IEntity item)
	{
		if (!item)
			return false;

		EntityID id = item.GetID();
		if (s_aUnpaid)
		{
			foreach (LF_StoreUnpaidItem record : s_aUnpaid)
			{
				if (record.m_ItemId == id)
					return true;
			}
		}

		foreach (string key, LF_StoreComponent store : Stores())
		{
			if (store.IsShelfItem(item))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: deletes the last receipt when it lies loose in the world (its holder was deleted with it
	//! already otherwise).
	static void DeleteLooseReceipt()
	{
		IEntity receipt = LastReceipt();
		if (receipt && LF_InventoryUtils.GetStorageHierachyRoot(receipt) == receipt)
			SCR_EntityHelper.DeleteEntityAndChildren(receipt);

		s_LastReceipt = EntityID.INVALID;
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: forgets every unpaid record.
	static void ClearUnpaid()
	{
		Unpaid().Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many unpaid records exist, over every store.
	static int UnpaidCount()
	{
		return Unpaid().Count();
	}

	//------------------------------------------------------------------------------------------------
	// The tick
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: one step of every store. Finds taken goods, restocks on the clock, updates who holds
	//! what, and catches shoplifters at the boundary.
	static void Tick()
	{
		if (!Replication.IsServer())
			return;

		float nowMs = GetGame().GetWorld().GetWorldTime();
		foreach (string key, LF_StoreComponent store : Stores())
		{
			store.DetectTaken();
			store.RestockIfDue(nowMs);
		}

		RefreshAll();
		SweepExits();
	}

	//------------------------------------------------------------------------------------------------
	//! Refreshes the carrier of every record and drops the ones that cannot matter any more.
	static void RefreshAll()
	{
		array<ref LF_StoreUnpaidItem> all = Unpaid();
		for (int i = all.Count() - 1; i >= 0; i--)
		{
			LF_StoreUnpaidItem record = all[i];
			IEntity item = GetGame().GetWorld().FindEntityByID(record.m_ItemId);
			if (!RefreshCarrier(record, item))
				all.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every carrier of unpaid goods who stands outside that store's boundary is a thief.
	//! \return Thieves caught this step.
	static int SweepExits()
	{
		int caught;
		for (int guard = 0; guard < 64; guard++)
		{
			LF_StoreUnpaidItem offender = FindOffender();
			if (!offender)
				break;

			IEntity thief = GetGame().GetWorld().FindEntityByID(offender.m_CarrierId);
			LF_StoreComponent store = Find(offender.m_sStoreKey);
			if (!thief || !store)
			{
				Unpaid().RemoveItem(offender);
				continue;
			}

			Shoplift(store, thief);
			caught++;
		}

		return caught;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_StoreUnpaidItem FindOffender()
	{
		foreach (LF_StoreUnpaidItem record : Unpaid())
		{
			if (record.m_CarrierId == EntityID.INVALID)
				continue;

			IEntity carrier = GetGame().GetWorld().FindEntityByID(record.m_CarrierId);
			LF_StoreComponent store = Find(record.m_sStoreKey);
			if (!carrier || !store)
				return record;

			bool inside = store.Contains(carrier.GetOrigin());
			if (record.m_bRestored)
			{
				if (inside)
				{
					record.m_bRestored = false;
					LF_Debug.Info(FEATURE, string.Format("store %1: %2 is back in the store with restored unpaid %3; walking out now is shoplifting", record.m_sStoreKey, CarrierText(record), record.m_Prefab));
					continue;
				}

				if (GetGame().GetWorld().GetWorldTime() < record.m_fRestoredUntilMs)
					continue;

				record.m_bRestored = false;
				LF_Debug.Info(FEATURE, string.Format("store %1: %2 did not come back to pay for restored unpaid %3 in time", record.m_sStoreKey, CarrierText(record), record.m_Prefab));
			}

			if (!inside)
				return record;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	// Shoplifting
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: the character left the store with unpaid goods. Every unpaid record of theirs at this
	//! store is closed (the goods are theirs now), the wanted level goes up, the crime is reported with
	//! the alarm at the store, and the report names them.
	//! \return The value of what was taken.
	static int Shoplift(LF_StoreComponent store, IEntity thief)
	{
		if (!Replication.IsServer() || !store || !thief)
			return 0;

		string key = store.GetStoreKey();
		array<LF_StoreUnpaidItem> items = {};
		CollectFor(key, thief, items);
		int value;
		foreach (LF_StoreUnpaidItem record : items)
		{
			value += record.m_iPrice;
			Unpaid().RemoveItem(record);
		}

		if (items.IsEmpty())
			return 0;

		string uid = ThiefKey(thief);
		LF_PlayerAccount account = LF_RobAction.FindAccount(uid);
		if (account)
		{
			LF_CrimeCatalog.ApplyWanted(account, uid, LF_WantedReports.OFFENCE_SHOPLIFTING);
			LF_PlayerAccountManager accountManager = LF_PlayerAccountManager.GetInstance();
			if (accountManager)
				accountManager.SaveAndReleaseAccount(account);
		}

		LF_Dispatch.Report(uid, thief, LF_WantedReports.OFFENCE_SHOPLIFTING, store.GetOwner().GetOrigin());
		LF_WantedReports.Identify(uid, "leaving a store with unpaid goods");
		LF_Debug.Info(FEATURE, string.Format("store %1: %2 walked out with %3 unpaid item(s) worth %4; alarm, dispatch, wanted level and name", key, uid, items.Count(), value));
		LF_Utils.NotifyEntity(thief, "#LF-Store_Thief", "#LF-Store_Title", 5.0);
		return value;
	}

	//------------------------------------------------------------------------------------------------
	// The till
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Server: the till takes the cash for everything of this store the character carries unpaid (or
	//! has used up), clears the tags and hands over a receipt. All or nothing: a short purse pays for
	//! none of it, and nothing changes.
	//! \param[out] paid Cash taken, 0 unless PAID.
	static LF_EStorePay Pay(LF_StoreComponent store, IEntity user, out int paid)
	{
		paid = 0;
		if (!Replication.IsServer() || !store || !user)
			return LF_EStorePay.NO_STORE;

		// A pickup the tick has not seen yet must still be on the bill.
		store.DetectTaken();
		RefreshAll();

		string key = store.GetStoreKey();
		array<LF_StoreUnpaidItem> items = {};
		CollectFor(key, user, items);
		if (items.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("store %1: till refused %2, nothing to pay for", key, ThiefKey(user)));
			return LF_EStorePay.NOTHING;
		}

		array<int> prices = {};
		foreach (LF_StoreUnpaidItem record : items)
			prices.Insert(record.m_iPrice);

		int total = LF_StoreRules.Total(prices);
		if (LF_MoneyUtils.GetCash(user) < total)
		{
			LF_Debug.Info(FEATURE, string.Format("store %1: till refused %2, %3 owed and not enough cash", key, ThiefKey(user), total));
			paid = total;
			return LF_EStorePay.NOT_ENOUGH_CASH;
		}

		int removed = LF_MoneyUtils.RemoveCash(user, total, SALE_REASON);
		if (removed != total)
		{
			if (removed > 0)
				LF_MoneyUtils.AddCash(user, removed, SALE_REASON + "-rollback");

			LF_Debug.Warn(FEATURE, string.Format("store %1: till could not take %2 from %3 (took %4); nothing changed", key, total, ThiefKey(user), removed));
			return LF_EStorePay.FAILED;
		}

		int count = items.Count();
		foreach (LF_StoreUnpaidItem record : items)
			Unpaid().RemoveItem(record);

		paid = total;
		GiveReceipt(store, user, total, count);
		LF_Debug.Info(FEATURE, string.Format("store %1: %2 paid %3 for %4 item(s); tags cleared", key, ThiefKey(user), total, count));
		return LF_EStorePay.PAID;
	}

	//------------------------------------------------------------------------------------------------
	protected static void GiveReceipt(LF_StoreComponent store, IEntity user, int total, int count)
	{
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(user);
		if (!storage)
			return;

		IEntity paper = LF_Utils.SpawnEntityPrefab(ReceiptPrefab(), user.GetOrigin());
		if (!paper)
		{
			LF_Debug.Error(FEATURE, string.Format("receipt did not spawn: %1", ReceiptPrefab()));
			return;
		}

		s_LastReceipt = paper.GetID();
		LF_ReceiptComponent receipt = LF_Component<LF_ReceiptComponent>.Find(paper);
		if (receipt)
		{
			s_iReceipts++;
			receipt.Print(s_iReceipts, total, count);
		}

		if (!storage.TryInsertItem(paper))
			LF_Debug.Warn(FEATURE, string.Format("store %1: receipt did not fit; it lies at the user's feet", store.GetStoreKey()));
	}

	//------------------------------------------------------------------------------------------------
	//! Harness `shelf` (dev gate): the character takes the first display copy it can find in the store
	//! it stands in, through the inventory insert a pickup uses. The next look of the server tags it.
	static string HarnessTakeFromShelf(IEntity character)
	{
		foreach (string key, LF_StoreComponent store : Stores())
		{
			if (!store.Contains(character.GetOrigin()))
				continue;

			InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
			if (!storage)
				return "shelf no_inventory";

			for (int line = 0; line < store.GetLineCount(); line++)
			{
				ResourceName prefab = store.GetLinePrefab(line);
				for (int slot = 0; slot < 16; slot++)
				{
					IEntity item = store.GetShelfItem(prefab, slot);
					if (!item)
						continue;

					if (!storage.TryInsertItem(item))
						return string.Format("shelf insert_failed store=%1 item=%2", key, prefab);

					EntityID itemId = item.GetID();
					bool handleAlive = item != null;
					bool found = GetGame().GetWorld().FindEntityByID(itemId) != null;
					int held = LF_InventoryUtils.GetAmount(character, prefab);
					store.DetectTaken();
					return string.Format("shelf took store=%1 item=%2 stock=%3 unpaid=%4 owed=%5 handle=%6 found=%7 held=%8", key, prefab, store.GetStock(prefab), UnpaidCount(), OwedBy(key, character), handleAlive, found, held);
				}
			}

			return string.Format("shelf empty store=%1", key);
		}

		return "shelf not_in_a_store";
	}

	//------------------------------------------------------------------------------------------------
	// Persistence ("storestock", save version 21)
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the stock of every live store, plus saved rows whose store has not appeared.
	static array<ref LF_StoreStockRow> ExportAll()
	{
		array<ref LF_StoreStockRow> rows = new array<ref LF_StoreStockRow>();
		foreach (string key, LF_StoreComponent store : Stores())
			store.Export(rows);

		foreach (string pendingKey, array<ref LF_StoreStockRow> pending : Pending())
		{
			foreach (LF_StoreStockRow row : pending)
				rows.Insert(row.Copy());
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the saved stock replaces the live one. A live store with no row keeps its
	//! stock; a row with no live store waits for it. Idempotent: a second load converges, and the
	//! shelf is made to match, never doubled.
	static void ApplyAll(array<ref LF_StoreStockRow> rows)
	{
		s_mPending = new map<string, ref array<ref LF_StoreStockRow>>();
		if (!rows)
			return;

		foreach (LF_StoreStockRow row : rows)
		{
			if (!row || row.m_sStoreKey.IsEmpty() || row.m_sPrefab.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "load skipped a stock row with no store key or item");
				continue;
			}

			array<ref LF_StoreStockRow> list = s_mPending.Get(row.m_sStoreKey);
			if (!list)
			{
				list = new array<ref LF_StoreStockRow>();
				s_mPending.Set(row.m_sStoreKey, list);
			}

			list.Insert(row.Copy());
		}

		foreach (string key, LF_StoreComponent store : Stores())
		{
			array<ref LF_StoreStockRow> mine = s_mPending.Get(key);
			if (!mine)
				continue;

			foreach (LF_StoreStockRow storeRow : mine)
				store.ImportRow(storeRow);

			store.AfterImport();
			s_mPending.Remove(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Unpaid tags across a restart ("storeunpaid", save version 25, #479)
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_StoreUnpaidRow> Parked()
	{
		if (!s_aParked)
			s_aParked = new array<ref LF_StoreUnpaidRow>();

		return s_aParked;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_StoreUnpaidRow RowOf(LF_StoreUnpaidItem record, string carrierKey)
	{
		LF_StoreUnpaidRow row = new LF_StoreUnpaidRow();
		row.m_sStoreKey = record.m_sStoreKey;
		row.m_sPrefab = record.m_Prefab;
		row.m_iPrice = record.m_iPrice;
		row.m_sCarrierKey = carrierKey;
		row.m_bConsumed = record.m_bConsumed;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many saved or parked tags wait for their character to spawn.
	static int ParkedCount()
	{
		return Parked().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: copies of the tags that wait for their character.
	static array<ref LF_StoreUnpaidRow> CopyParked()
	{
		array<ref LF_StoreUnpaidRow> rows = new array<ref LF_StoreUnpaidRow>();
		foreach (LF_StoreUnpaidRow row : Parked())
			rows.Insert(row.Copy());

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: replaces the waiting tags and binds none of them (a test puts back what it found).
	static void SetParked(array<ref LF_StoreUnpaidRow> rows)
	{
		s_aParked = new array<ref LF_StoreUnpaidRow>();
		if (!rows)
			return;

		foreach (LF_StoreUnpaidRow row : rows)
		{
			if (row)
				s_aParked.Insert(row.Copy());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: a row for every tag a living player carries (or used up), plus the tags that
	//! still wait for their character. A tag on the floor, on a dead body, or on a body no player
	//! controls is not saved: death drops what you carry, and nothing could claim the others.
	static array<ref LF_StoreUnpaidRow> ExportUnpaid()
	{
		array<ref LF_StoreUnpaidRow> rows = new array<ref LF_StoreUnpaidRow>();
		foreach (LF_StoreUnpaidItem record : Unpaid())
		{
			if (record.m_sCarrierKey.IsEmpty() || record.m_iPrice <= 0)
				continue;

			IEntity carrier = GetGame().GetWorld().FindEntityByID(record.m_CarrierId);
			if (!carrier || !LF_StorageSnapshot.IsAlive(carrier))
				continue;

			rows.Insert(RowOf(record, record.m_sCarrierKey));
		}

		foreach (LF_StoreUnpaidRow parked : Parked())
			rows.Insert(parked.Copy());

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the saved tags replace the waiting ones. A character in the world now (a load
	//! re-applied to a live session) takes its tags at once, replacing the ones it carries; the rest wait
	//! for their character to spawn. Idempotent: a second load binds the same tags to the same items.
	//! \return Tags put back on characters in the world now.
	static int ApplyUnpaid(array<ref LF_StoreUnpaidRow> rows)
	{
		s_aParked = new array<ref LF_StoreUnpaidRow>();
		if (!rows)
			return 0;

		foreach (LF_StoreUnpaidRow row : rows)
		{
			if (!row || row.m_sStoreKey.IsEmpty() || row.m_sPrefab.IsEmpty() || row.m_sCarrierKey.IsEmpty() || row.m_iPrice <= 0)
			{
				LF_Debug.Warn(FEATURE, "load skipped an unpaid tag with no store, item, carrier or price");
				continue;
			}

			s_aParked.Insert(row.Copy());
		}

		if (!Replication.IsServer() || !GetGame().GetPlayerManager())
			return 0;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		int bound;
		foreach (int playerId : playerIds)
		{
			IEntity controlled = playerManager.GetPlayerControlledEntity(playerId);
			if (SCR_ChimeraCharacter.Cast(controlled) && LF_StorageSnapshot.IsAlive(controlled))
				bound += ReattachUnpaid(LF_Utils.GetPlayerUID(playerId), controlled);
		}

		return bound;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character's body is in the world with its carried inventory back (spawn, or a load on
	//! a live session). Its waiting tags go back on untagged items of their prefab it carries, replacing
	//! any tag it carries now; a used-up item comes back as the same debt. A tag whose store is not in
	//! the world, or whose item the character no longer carries, is dropped with a warning.
	//! \return Tags put back.
	static int ReattachUnpaid(string carrierKey, IEntity character)
	{
		if (carrierKey.IsEmpty() || !character || !s_aParked || s_aParked.IsEmpty())
			return 0;

		array<ref LF_StoreUnpaidRow> mine = {};
		for (int i = s_aParked.Count() - 1; i >= 0; i--)
		{
			if (s_aParked[i].m_sCarrierKey != carrierKey)
				continue;

			mine.InsertAt(s_aParked[i], 0);
			s_aParked.Remove(i);
		}

		if (mine.IsEmpty())
			return 0;

		EntityID characterId = character.GetID();
		array<ref LF_StoreUnpaidItem> all = Unpaid();
		for (int j = all.Count() - 1; j >= 0; j--)
		{
			if (all[j].m_CarrierId == characterId)
				all.Remove(j);
		}

		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
		float nowMs = GetGame().GetWorld().GetWorldTime();
		int bound;
		int dropped;
		int graceSeconds;
		foreach (LF_StoreUnpaidRow row : mine)
		{
			LF_StoreComponent store = Find(row.m_sStoreKey);
			if (!store)
			{
				LF_Debug.Warn(FEATURE, string.Format("store %1 is not in the world: unpaid %2 of %3 is dropped", row.m_sStoreKey, row.m_sPrefab, carrierKey));
				dropped++;
				continue;
			}

			IEntity item;
			if (!row.m_bConsumed)
			{
				item = FindUntagged(storage, row.m_sPrefab);
				if (!item)
				{
					LF_Debug.Warn(FEATURE, string.Format("store %1: %2 no longer carries unpaid %3; the tag is dropped", row.m_sStoreKey, carrierKey, row.m_sPrefab));
					dropped++;
					continue;
				}
			}

			LF_StoreUnpaidItem record = new LF_StoreUnpaidItem();
			if (item)
				record.m_ItemId = item.GetID();
			else
				record.m_ItemId = EntityID.INVALID;

			record.m_sStoreKey = row.m_sStoreKey;
			record.m_Prefab = row.m_sPrefab;
			record.m_iPrice = row.m_iPrice;
			record.m_CarrierId = characterId;
			record.m_sCarrierKey = carrierKey;
			record.m_bConsumed = row.m_bConsumed;
			record.m_bRestored = true;
			record.m_fRestoredUntilMs = nowMs + store.GetRestoredGraceSeconds() * 1000;
			graceSeconds = Math.Max(graceSeconds, store.GetRestoredGraceSeconds());
			all.Insert(record);
			bound++;
		}

		LF_Debug.Info(FEATURE, string.Format("%1 unpaid tag(s) back on %2, %3 dropped; %4 s to come back and pay", bound, carrierKey, dropped, graceSeconds));
		if (bound > 0)
			LF_Utils.NotifyEntity(character, "#LF-Store_UnpaidRestored", "#LF-Store_Title", 8.0, ((graceSeconds + 59) / 60).ToString());

		return bound;
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindUntagged(InventoryStorageManagerComponent storage, ResourceName prefab)
	{
		if (!storage)
			return null;

		array<IEntity> items = LF_InventoryUtils.FindItemsByPrefab(storage, prefab);
		foreach (IEntity item : items)
		{
			if (!HoldsApart(item))
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a living player leaves. The tags on what they carry wait under their character key for
	//! the next spawn, so logging out does not make the goods free. A dead body keeps nothing.
	//! \return Tags parked.
	static int ParkCarrier(string carrierKey, IEntity character)
	{
		if (!character || !s_aUnpaid || !LF_StorageSnapshot.IsAlive(character))
			return 0;

		if (carrierKey.IsEmpty())
		{
			array<LF_StoreUnpaidItem> carried = {};
			foreach (LF_StoreUnpaidItem candidate : s_aUnpaid)
			{
				if (candidate.m_CarrierId == character.GetID())
					carried.Insert(candidate);
			}

			if (!carried.IsEmpty())
				LF_Debug.Warn(FEATURE, string.Format("a player with no character key left carrying %1 unpaid item(s); the tags are lost", carried.Count()));

			return 0;
		}

		// A pickup the tick has not seen yet goes with them.
		foreach (string key, LF_StoreComponent store : Stores())
			store.DetectTaken();

		RefreshAll();
		EntityID characterId = character.GetID();
		int parked;
		for (int i = s_aUnpaid.Count() - 1; i >= 0; i--)
		{
			LF_StoreUnpaidItem record = s_aUnpaid[i];
			if (record.m_CarrierId != characterId)
				continue;

			Parked().Insert(RowOf(record, carrierKey));
			s_aUnpaid.Remove(i);
			parked++;
		}

		if (parked > 0)
			LF_Debug.Info(FEATURE, string.Format("%1 left carrying %2 unpaid item(s); the tags wait for their next spawn", carrierKey, parked));

		return parked;
	}
}

//------------------------------------------------------------------------------------------------
//! The till of a physical store (#202, #582): "Pay for goods" on the cash register itself. The
//! register is its own entity on the counter and names its store by key; an action put straight on a
//! store entity still works. The server does all of it (LF_Stores.Pay) and checks the payer stands at
//! the register; a client only asks, so the action performs on every machine and acts on the server alone.
class LF_StoreTillAction : ScriptedUserAction
{
	[Attribute("", UIWidgets.EditBox, "Key of the store this register belongs to (LF_StoreComponent.m_sStoreKey). Empty: the store component on this same entity")]
	protected string m_sStoreKey;

	//------------------------------------------------------------------------------------------------
	protected LF_StoreComponent ResolveStore(IEntity owner)
	{
		if (!m_sStoreKey.IsEmpty())
			return LF_Stores.Find(m_sStoreKey);

		return LF_Component<LF_StoreComponent>.Find(owner);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		// Hidden while the Shop system is off (#1074).
		return LF_Systems.IsEnabled(LF_ShopRefusal.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_sStoreKey.IsEmpty())
			return true;

		return LF_Component<LF_StoreComponent>.Find(GetOwner()) != null;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_ShopRefusal.SYSTEM, "store till"))
			return;

		LF_StoreComponent store = ResolveStore(pOwnerEntity);
		if (store && pOwnerEntity && pUserEntity)
		{
			float distance = vector.Distance(pOwnerEntity.GetOrigin(), pUserEntity.GetOrigin());
			if (!LF_StoreRules.TillInReach(distance))
			{
				LF_Debug.Warn(LF_Stores.FEATURE, string.Format("store %1: till refused %2, %3 m from the register", store.GetStoreKey(), LF_Stores.ThiefKey(pUserEntity), distance));
				return;
			}
		}

		int paid;
		LF_EStorePay result = LF_Stores.Pay(store, pUserEntity, paid);
		string key = LF_StoreRules.PayKey(result);
		if (!key.IsEmpty())
			LF_Utils.NotifyEntity(pUserEntity, key, "#LF-Store_Title", 4.0, paid.ToString());
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Shop", description: "Paper receipt a till hands over: what was paid.")]
class LF_ReceiptComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! The paper receipt (#202). The till prints the receipt number, the total and the item count onto it;
//! Read shows them. The fields are session state: a receipt carried over a restart comes back blank
//! (the carried snapshot keeps the prefab) and reads as a blank slip. The ledger holds the sale.
class LF_ReceiptComponent : ScriptComponent
{
	protected int m_iNumber;
	protected int m_iTotal;
	protected int m_iItems;

	//------------------------------------------------------------------------------------------------
	void Print(int number, int total, int items)
	{
		m_iNumber = number;
		m_iTotal = total;
		m_iItems = items;
	}

	//------------------------------------------------------------------------------------------------
	bool IsPrinted()
	{
		return m_iNumber > 0;
	}

	//------------------------------------------------------------------------------------------------
	int GetTotal()
	{
		return m_iTotal;
	}

	//------------------------------------------------------------------------------------------------
	int GetItems()
	{
		return m_iItems;
	}

	//------------------------------------------------------------------------------------------------
	string Describe()
	{
		return string.Format("no. %1: %2 item(s), $%3", m_iNumber, m_iItems, m_iTotal);
	}
}

//------------------------------------------------------------------------------------------------
//! "Read receipt": the server toasts what the paper says to the reader.
class LF_ReceiptReadAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		LF_ReceiptComponent receipt = LF_Component<LF_ReceiptComponent>.Find(pOwnerEntity);
		if (!receipt || !receipt.IsPrinted())
		{
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Store_ReceiptBlank", "#LF-Store_ReceiptTitle", 4.0);
			return;
		}

		LF_Utils.NotifyEntity(pUserEntity, "#LF-Store_ReceiptText", "#LF-Store_ReceiptTitle", 5.0, receipt.Describe());
	}
}
