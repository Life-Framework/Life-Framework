//------------------------------------------------------------------------------------------------
//! Why a reclaim from a site's lost-and-found was or was not allowed (#3457). Logged and shown to the owner.
enum LF_ELostAndFoundResult
{
	RECLAIMED,
	NOTHING_TO_RECLAIM,	//!< nothing of this character's waits at this site (a stranger, or already taken)
	CANNOT_AFFORD,	//!< the fee is not in hand; nothing was taken and nothing moved
	OUT_OF_REACH,	//!< no lost-and-found box within reach of the character
	BAD_CALLER,	//!< no character, no character key, no site or no box behind the request
	NO_HOLDER,	//!< the box has no storage to unpack the things into
	NOT_SERVER	//!< reclaiming is a server decision
}

//------------------------------------------------------------------------------------------------
//! A site's lost-and-found (#3457, chapter #1038, rule #2658). When a rent lapses the locker is freed and
//! its contents wait here for their owner, who pays `reclaimFee` at the box beside the locker bank and
//! takes every item back, into their inventory or at their feet. Nothing is deleted, and nobody but the
//! owner can take it: the things are an LF_StorageSnapshotRecord (owner = the character record key, slot
//! "lostfound:<siteId>") in the existing "carried" payload, so they survive death and a restart with no
//! new save section, exactly like the locker contents they came from (LF_LockerStorageManagerComponent).
//!
//! Server only. The owner is always derived on the server from the acting entity and the box from that
//! entity's own position; nothing a client sends picks either. The lapse is decided by LF_LockerRules and
//! applied by MoveLapsed, from a slow sweep (armed by the first bank or box) and again for the one renter
//! at the box or opening the locker (OpenAtTime), so a lapsed locker never hands its contents out for free.
class LF_LostAndFound
{
	static const string SLOT_PREFIX = "lostfound:";

	//! Seconds between lapse sweeps: a lapse is counted in days, so a slow poll is exact enough.
	static const int SWEEP_MS = 300000;	// lf-fixed: an internal poll interval, not gameplay

	protected static bool s_bSweepArmed;

	//------------------------------------------------------------------------------------------------
	//! The snapshot slot a site's lost-and-found lives under (per character: the owner key is the character).
	static string SlotFor(string siteId)
	{
		return SLOT_PREFIX + siteId;
	}

	//------------------------------------------------------------------------------------------------
	//! True when something of this character's waits at the site.
	static bool HasThings(string characterKey, string siteId)
	{
		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Get(characterKey, SlotFor(siteId));
		return record && record.m_aItems && record.m_aItems.Count() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: moves one lapsed locker's contents to the site's lost-and-found and frees the locker. Does
	//! nothing when the character holds no locker here or it has not reached its lapse moment.
	//! \return True when the locker was freed.
	static bool MoveLapsed(int now, string siteId, string characterKey, int rentPeriodDays, int gracePeriodDays)
	{
		if (!Replication.IsServer() || siteId.IsEmpty() || characterKey.IsEmpty())
			return false;

		if (!LF_LockerBank.IsRenting(siteId, characterKey))
			return false;

		int paidUntil = LF_LockerBank.PaidUntil(siteId, characterKey);
		if (!LF_LockerRules.MoveToLostAndFound(now, paidUntil, rentPeriodDays, gracePeriodDays))
			return false;

		// Open right now: close it first so the snapshot holds what is on screen.
		LF_LockerStorageManagerComponent open = LF_LockerStorageManagerComponent.FindOpen(siteId, characterKey);
		if (open)
			open.Release("rent lapsed");

		string lockerSlot = LF_LockerStorageManagerComponent.SlotFor(siteId);
		LF_StorageSnapshotRecord contents = LF_StorageSnapshot.Get(characterKey, lockerSlot);
		int moved;
		if (contents && contents.m_aItems && contents.m_aItems.Count() > 0)
			moved = Deposit(characterKey, siteId, contents);

		LF_StorageSnapshot.Clear(characterKey, lockerSlot);
		LF_LockerBank.Free(siteId, characterKey);
		LF_Debug.Info(LF_LockerBank.FEATURE, string.Format("%1's locker at %2 lapsed: %3 item record(s) wait in lost-and-found", characterKey, siteId, moved));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a lapsed locker's records to what already waits for the character at the site (a second
	//! lapse after a new rent), shifting the parent indexes of the appended records.
	//! \return Item records now waiting.
	protected static int Deposit(string characterKey, string siteId, notnull LF_StorageSnapshotRecord contents)
	{
		string slot = SlotFor(siteId);
		LF_StorageSnapshotRecord waiting = LF_StorageSnapshot.Get(characterKey, slot);
		if (!waiting)
			waiting = LF_StorageSnapshotRecord.Create(characterKey, slot);

		int offset = waiting.m_aItems.Count();
		foreach (LF_StorageItemRecord item : contents.m_aItems)
		{
			if (!item)
				continue;

			if (item.m_iParent >= 0)
				item.m_iParent = item.m_iParent + offset;

			waiting.m_aItems.Insert(item);
		}

		LF_StorageSnapshot.Set(waiting);
		return waiting.m_aItems.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: lapses every rental that has reached its lapse moment.
	//! \return Lockers freed.
	static int SweepAll(int now, int rentPeriodDays, int gracePeriodDays)
	{
		int freed;
		array<ref LF_LockerRentalRow> rows = LF_LockerBank.ExportAll();
		foreach (LF_LockerRentalRow row : rows)
		{
			if (MoveLapsed(now, row.m_sSiteId, row.m_sCharacterKey, rentPeriodDays, gracePeriodDays))
				freed++;
		}

		return freed;
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the slow lapse sweep once, from the first bank or box placed. The timer reads the clock and the
	//! config each time it fires and does nothing off the server.
	static void ArmSweep()
	{
		if (s_bSweepArmed || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bSweepArmed = true;
		GetGame().GetCallqueue().CallLater(SweepTick, SWEEP_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! True once the lapse sweep is running.
	static bool IsSweepArmed()
	{
		return s_bSweepArmed;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SweepTick()
	{
		if (!Replication.IsServer() || !LF_Systems.IsEnabled(LF_LockerBank.FEATURE))
			return;

		SweepAll(System.GetUnixTime(), LF_LockerConfig.RentPeriodDays(), LF_LockerConfig.GracePeriodDays());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the acting character reclaims at the nearest lost-and-found box within that box's reach.
	static LF_ELostAndFoundResult ReclaimNearest(IEntity user)
	{
		if (!Replication.IsServer())
			return LF_ELostAndFoundResult.NOT_SERVER;

		if (!user)
		{
			LF_Debug.Error(LF_LockerBank.FEATURE, "reclaim refused: no character behind the request");
			return LF_ELostAndFoundResult.BAD_CALLER;
		}

		LF_LostAndFoundBoxComponent box = LF_LostAndFoundBoxComponent.FindNearest(user.GetOrigin());
		if (!box)
		{
			LF_Debug.Info(LF_LockerBank.FEATURE, string.Format("reclaim refused at %1: no lost-and-found box within reach", user.GetOrigin()));
			return LF_ELostAndFoundResult.OUT_OF_REACH;
		}

		return ReclaimAt(System.GetUnixTime(), user, LF_Utils.GetPlayerUID(user), box.GetSiteId(), user.GetOrigin(), box.GetOwner(), box.GetReach(), LF_LockerConfig.ReclaimFee(), LF_LockerConfig.RentPeriodDays(), LF_LockerConfig.GracePeriodDays());
	}

	//------------------------------------------------------------------------------------------------
	//! The seam tests drive with their own clock, positions and fee. Takes the fee only when every check
	//! passes, then moves every waiting item into the character's inventory or drops it at their feet.
	static LF_ELostAndFoundResult ReclaimAt(int now, IEntity user, string characterKey, string siteId, vector userPos, IEntity box, float reach, int fee, int rentPeriodDays, int gracePeriodDays)
	{
		if (!Replication.IsServer())
			return LF_ELostAndFoundResult.NOT_SERVER;

		if (!user || !box || characterKey.IsEmpty() || siteId.IsEmpty())
		{
			LF_Debug.Error(LF_LockerBank.FEATURE, string.Format("reclaim refused: missing character (%1), site (%2) or box", characterKey, siteId));
			return LF_ELostAndFoundResult.BAD_CALLER;
		}

		if (!LF_Utils.IsAliveState(user))
		{
			LF_Debug.Info(LF_LockerBank.FEATURE, string.Format("%1 refused at %2: not alive", characterKey, siteId));
			return LF_ELostAndFoundResult.BAD_CALLER;
		}

		if (vector.Distance(userPos, box.GetOrigin()) > reach)
		{
			LF_Debug.Info(LF_LockerBank.FEATURE, string.Format("%1 refused at %2: out of reach", characterKey, siteId));
			return LF_ELostAndFoundResult.OUT_OF_REACH;
		}

		// A lapse the sweep has not reached yet still counts for the one standing here.
		MoveLapsed(now, siteId, characterKey, rentPeriodDays, gracePeriodDays);

		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Get(characterKey, SlotFor(siteId));
		if (!record || !record.m_aItems || record.m_aItems.Count() == 0)
		{
			LF_Debug.Info(LF_LockerBank.FEATURE, string.Format("%1 refused at %2: nothing waits in lost-and-found", characterKey, siteId));
			return LF_ELostAndFoundResult.NOTHING_TO_RECLAIM;
		}

		LF_LostAndFoundStorageManagerComponent holder = LF_LostAndFoundStorageManagerComponent.Cast(box.FindComponent(LF_LostAndFoundStorageManagerComponent));
		BaseInventoryStorageComponent holderStorage = BaseInventoryStorageComponent.Cast(box.FindComponent(BaseInventoryStorageComponent));
		InventoryStorageManagerComponent userManager = LF_InventoryUtils.GetResponsibleStorageManager(user);
		if (!holder || !holderStorage || !userManager)
		{
			LF_Debug.Error(LF_LockerBank.FEATURE, string.Format("reclaim failed for %1 at %2: the box has no storage or the character no inventory", characterKey, siteId));
			return LF_ELostAndFoundResult.NO_HOLDER;
		}

		if (fee < 0)
			fee = 0;

		if (LF_LockerRules.CheckReclaim(LF_MoneyUtils.GetCash(user), fee) == LF_ELockerReclaimResult.CANNOT_AFFORD)
		{
			LF_Debug.Info(LF_LockerBank.FEATURE, string.Format("%1 refused at %2: short of the reclaim fee %3", characterKey, siteId, fee));
			return LF_ELostAndFoundResult.CANNOT_AFFORD;
		}

		if (fee > 0)
		{
			int taken = LF_MoneyUtils.RemoveCash(user, fee, "locker-reclaim");
			if (taken < fee)
			{
				// The cash moved between the check and the take: hand back what was taken, move nothing.
				if (taken > 0)
					LF_MoneyUtils.AddCash(user, taken, "locker-reclaim-refund", "", true);
				LF_Debug.Warn(LF_LockerBank.FEATURE, string.Format("%1 refused at %2: took %3 of %4, handed back", characterKey, siteId, taken, fee));
				return LF_ELostAndFoundResult.CANNOT_AFFORD;
			}
		}

		// Unpack the record into the box's storage (a tree with its nesting and per-item state), then move
		// each top-level item to the character. The snapshot is cleared once the things are theirs, so a
		// second reclaim finds nothing.
		int spawned = LF_StorageSnapshot.Apply(box, record);
		array<IEntity> items = {};
		holderStorage.GetAll(items, false);
		if (items.Count() == 0)
		{
			// Nothing unpacked (every prefab failed to load): the record stays, the fee goes back.
			if (fee > 0)
				LF_MoneyUtils.AddCash(user, fee, "locker-reclaim-refund", "", true);
			LF_Debug.Error(LF_LockerBank.FEATURE, string.Format("reclaim failed for %1 at %2: %3 record(s) unpacked to nothing, fee handed back", characterKey, siteId, record.m_aItems.Count()));
			return LF_ELostAndFoundResult.NO_HOLDER;
		}

		int toInventory;
		int atFeet;
		foreach (IEntity item : items)
		{
			// Out of the box first (it lands on the ground), then picked up like any item off the floor.
			if (!holder.TryRemoveItemFromStorage(item, holderStorage))
			{
				LF_Debug.Warn(LF_LockerBank.FEATURE, string.Format("%1 at %2: %3 would not leave the box", characterKey, siteId, LF_Utils.GetPrefabName(item)));
				continue;
			}

			if (userManager.TryInsertItem(item))
			{
				toInventory++;
				continue;
			}

			item.SetOrigin(userPos);
			atFeet++;
		}

		LF_StorageSnapshot.Clear(characterKey, SlotFor(siteId));
		LF_Debug.Info(LF_LockerBank.FEATURE, string.Format("%1 reclaimed at %2 for %3: %4 spawned, %5 into the inventory, %6 at their feet", characterKey, siteId, fee, spawned, toInventory, atFeet));
		return LF_ELostAndFoundResult.RECLAIMED;
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key the owner sees for a result.
	static string ResultKey(LF_ELostAndFoundResult result)
	{
		switch (result)
		{
			case LF_ELostAndFoundResult.RECLAIMED:
				return "#LF-LostAndFound_Reclaimed";
			case LF_ELostAndFoundResult.NOTHING_TO_RECLAIM:
				return "#LF-LostAndFound_Nothing";
			case LF_ELostAndFoundResult.CANNOT_AFFORD:
				return "#LF-LostAndFound_CannotAfford";
			case LF_ELostAndFoundResult.OUT_OF_REACH:
				return "#LF-LostAndFound_OutOfReach";
		}

		return "#LF-LostAndFound_Refused";
	}

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset): the sweep timer dies with the world.
	static void ResetStatics()
	{
		if (s_bSweepArmed && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(SweepTick);

		s_bSweepArmed = false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LostAndFound's statics (LF_StaticReset finds it by reflection).
class LF_LostAndFound_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LostAndFound";
	}

	override void Apply()
	{
		LF_LostAndFound.ResetStatics();
	}
}
