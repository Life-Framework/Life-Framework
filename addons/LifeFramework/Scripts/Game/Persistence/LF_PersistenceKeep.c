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
//! FIELDS. keepCash (cash stacks in the carried snapshots), keepCarriedGear (every carried
//! snapshot, cash included), keepSurvival (hunger, thirst, health), keepWanted (every character's
//! wanted level; the posters are pruned after, LF_WantedReports.PruneCleared), keepJail (serving
//! sentences), keepLicences (earned licences; the body keeps its starter ones). Fields the save does
//! not hold today (position, vehicle fuel, damage and trunk, house contents) have no switch.
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
			LF_Debug.Info(FEATURE, string.Format("keepCarriedGear off: %1 carried snapshot(s) reset to the starting loadout", dropped));
			reset++;
		}
		else if (!keep.m_bKeepCash)
		{
			int cash = LF_StorageSnapshot.StripPrefab(LF_StorageSnapshot.SLOT_CARRIED, LF_MoneyUtils.CashPrefab());
			LF_Debug.Info(FEATURE, string.Format("keepCash off: %1 cash on hand removed from the carried snapshots", cash));
			reset++;
		}

		if (!keep.m_bKeepSurvival && !persistence)
		{
			LF_Debug.Error(FEATURE, "keepSurvival off: no persistence component, survival stats left as saved");
		}
		else if (!keep.m_bKeepSurvival)
		{
			persistence.ClearSurvivalStats();
			LF_Debug.Info(FEATURE, "keepSurvival off: hunger, thirst and health start full");
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
}
