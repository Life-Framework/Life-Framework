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
//! #2547), keepSurvival (hunger, thirst), keepWanted (every character's wanted level; the posters
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
	//! Strips the cash stacks from every slot a player can carry back: carried, the civilian stash and
	//! both duty slots. A duty slot is not cleared, only its cash goes.
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
		return cash;
	}
}
