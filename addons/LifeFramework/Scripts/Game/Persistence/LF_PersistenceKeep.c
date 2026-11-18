//------------------------------------------------------------------------------------------------
//! Persistence switches (#689): the owner chooses in config.json (`persistence.keep*`) which
//! fields survive a restart. A switched-off field is still written and read, so the save keeps its
//! shape and its version, and an old save loads as before; after every payload is read, the load
//! resets that field in the live stores to its default. The next save then writes the default.
//!
//! WHY RESET ON LOAD, NOT SKIP THE WRITE. Binary saves are positional: leaving a payload out
//! would need a version bump per switch combination. Resetting after the read keeps one format,
//! makes a save written with the switch on load "fresh" when the owner turns it off, and is
//! idempotent: a second pass over the same save resets the same fields to the same defaults.
//!
//! FIELDS. keepCash (cash stacks in the carried, civilian stash and duty snapshots),
//! keepCarriedGear (every carried snapshot and civilian stash, cash included; the duty kits stay,
//! #2547; the armory book's outstanding serials are cleared, cooldowns kept, #2621), keepSurvival (hunger, thirst), keepWanted (every character's wanted level; the posters
//! are pruned after, LF_WantedReports.PruneCleared), keepJail (serving sentences),
//! keepLicences (earned licences; the body keeps its starter ones). Fields the save does not hold today (position, vehicle fuel, damage and trunk, house contents) have no switch.
//------------------------------------------------------------------------------------------------
class LF_PersistenceKeep
{
	static const string FEATURE = "Persistence";

	//------------------------------------------------------------------------------------------------
	//! Resets every switched-off field in the live stores. Called by DeserializeStore after the last
	//! payload and before the wanted posters are pruned.
	//! \param[in] persistence The game-mode persistence component (owns the survival map); null skips survival.
	//! \param[in] keep The effective switches (LF_PersistenceConfig.ResolveKeep).
	//! \return How many fields were reset.
	static int ApplyAfterLoad(LF_PersistenceComponent persistence, notnull LF_PersistenceConfig keep)
	{
		int reset;

		if (!keep.m_bKeepCarriedGear)
		{
			int dropped = LF_StorageSnapshot.ClearSlot(LF_StorageSnapshot.SLOT_CARRIED);
			int stashes = LF_StorageSnapshot.ClearSlot(LF_DutyLocker.SLOT_CIVILIAN);
			LF_Debug.Info(FEATURE, string.Format("keepCarriedGear off: %1 carried snapshot(s) reset to the starting loadout, %2 civilian stash(es) dropped", dropped, stashes));
			// The wiped gear included issued kits: the armory book hands their serials back (#2621).
			// A kit in a kept slot (the police duty locker) still exists, so its serials stay out.
			// The issue cooldown stays as saved.
			int serials = LF_ArmoryKits.ClearAllOutstanding(KeptSerials());
			LF_Debug.Info(FEATURE, string.Format("keepCarriedGear off: %1 outstanding armory serial(s) cleared, issue cooldowns kept", serials));
			reset++;
		}

		if (!keep.m_bKeepCash)
		{
			// With carried gear off the carried and civilian slots are already empty; this is one field
			// then, cash riding with the gear. The duty kits keep their gear (the armory book holds
			// their serials, #2488) and lose only the cash an officer left in them.
			int cash = StripCash(!keep.m_bKeepCarriedGear);
			LF_Debug.Info(FEATURE, string.Format("keepCash off: %1 cash removed from the carried, civilian stash and duty snapshots", cash));
			if (keep.m_bKeepCarriedGear)
				reset++;
		}

		if (!keep.m_bKeepSurvival && !persistence)
		{
			LF_Debug.Error(FEATURE, "keepSurvival off: no persistence component, survival stats left as saved");
		}
		else if (!keep.m_bKeepSurvival)
		{
			persistence.ClearSurvivalStats();
			LF_Debug.Info(FEATURE, "keepSurvival off: hunger and thirst start full");
			reset++;
		}

		if (!keep.m_bKeepWanted)
		{
			int wanted;
			LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
			if (accounts)
				wanted = accounts.ClearAllCharactersWanted();
			else
				LF_Debug.Warn(FEATURE, "keepWanted off: no account manager, wanted levels left as saved");

			LF_Debug.Info(FEATURE, string.Format("keepWanted off: %1 wanted level(s) cleared", wanted));
			reset++;
		}

		if (!keep.m_bKeepJail)
		{
			int sentences = LF_Jail.ExportAll().Count();
			LF_Jail.ApplyAll(new array<ref LF_JailRecord>());
			LF_Debug.Info(FEATURE, string.Format("keepJail off: %1 jail sentence(s) dropped", sentences));
			reset++;
		}

		if (!keep.m_bKeepLicences)
		{
			int licences = LF_Progression.ClearAllLicences();
			LF_Debug.Info(FEATURE, string.Format("keepLicences off: earned licences cleared on %1 character(s)", licences));
			reset++;
		}

		return reset;
	}

	//------------------------------------------------------------------------------------------------
	//! Serials of every item still in a stored snapshot (called after the carried and civilian slots
	//! are dropped, so these are the guns the wipe kept).
	protected static set<string> KeptSerials()
	{
		set<string> serials = new set<string>();
		array<ref LF_StorageSnapshotRecord> records = LF_StorageSnapshot.ExportAll();
		foreach (LF_StorageSnapshotRecord record : records)
		{
			foreach (LF_StorageItemRecord item : record.m_aItems)
			{
				if (item && !item.m_sSerial.IsEmpty())
					serials.Insert(item.m_sSerial);
			}
		}

		return serials;
	}

	//------------------------------------------------------------------------------------------------
	//! Strips the cash stacks from every slot a player can carry back: carried, the civilian stash,
	//! both duty slots and car cargo (#2809). A duty slot is not cleared, only its cash goes.
	//! \param[in] dutyOnly True when the carried and civilian slots were already dropped.
	//! \return The total cash removed.
	protected static int StripCash(bool dutyOnly)
	{
		ResourceName cashPrefab = LF_MoneyUtils.CashPrefab();
		if (cashPrefab.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "keepCash off: no cash prefab configured, cash left as saved");
			return 0;
		}

		int cash;
		if (!dutyOnly)
		{
			cash += LF_StorageSnapshot.StripPrefab(LF_StorageSnapshot.SLOT_CARRIED, cashPrefab);
			cash += LF_StorageSnapshot.StripPrefab(LF_DutyLocker.SLOT_CIVILIAN, cashPrefab);
		}

		cash += LF_StorageSnapshot.StripPrefab(LF_DutyLocker.SLOT_POLICE, cashPrefab);
		cash += LF_StorageSnapshot.StripPrefab(LF_DutyLocker.SLOT_MEDIC, cashPrefab);
		// A car's saved cargo (#2809) is not gear a wipe drops, but its cash is cash.
		cash += LF_StorageSnapshot.StripPrefab(LF_VehicleOwnership.SLOT_CARGO, cashPrefab);

		// Marked notes (#2850) are dirty money, money all the same: they go with the cash.
		int marked = StripMarkedNotes(dutyOnly);
		LF_Debug.Info(FEATURE, string.Format("keepCash off: %1 marked notes removed with the cash", marked));
		return cash;
	}

	//------------------------------------------------------------------------------------------------
	//! Strips the marked-notes stacks (LF_MarkedCash) from the slots StripCash clears.
	//! \param[in] dutyOnly True when the carried and civilian slots were already dropped.
	//! \return The total marked notes removed, 0 when the slot has no prefab.
	protected static int StripMarkedNotes(bool dutyOnly)
	{
		ResourceName marked = LF_MarkedCash.Prefab();
		if (marked.IsEmpty())
			return 0;

		int removed;
		if (!dutyOnly)
		{
			removed += LF_StorageSnapshot.StripPrefab(LF_StorageSnapshot.SLOT_CARRIED, marked);
			removed += LF_StorageSnapshot.StripPrefab(LF_DutyLocker.SLOT_CIVILIAN, marked);
		}

		removed += LF_StorageSnapshot.StripPrefab(LF_DutyLocker.SLOT_POLICE, marked);
		removed += LF_StorageSnapshot.StripPrefab(LF_DutyLocker.SLOT_MEDIC, marked);
		removed += LF_StorageSnapshot.StripPrefab(LF_VehicleOwnership.SLOT_CARGO, marked);
		return removed;
	}
}
