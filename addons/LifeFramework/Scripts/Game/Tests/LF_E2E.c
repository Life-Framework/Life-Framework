//! Pass-2 helpers of the two-boot e2e (`tools\cli e2e`, `wt e2e`; docs/persistence.md "Pre-move save gate").
//!
//! Pass 2 reloads a save that an OLDER build can have written: `--legacy-save` (version 23) and the pinned
//! `--pre-move` ref (the main guard runs it after every merge). Such a save never held a payload added later, so
//! a pass-2 check for that payload fails on a correct change (#2480 -> #2483/#2500, #2798 -> #2801, #2812). Every
//! pass-2 check for newer data asks here first; `validate-e2e-payload-guard` fails one that does not.
//!
//! The payload's place in the save order is the only knowledge kept here (`PayloadSinceVersion`), so a change
//! to how payload versions are counted (per-payload sections, #2841) is a change to `SaveHas` alone.
class LF_E2E
{
	static const string FEATURE = "E2E";

	//------------------------------------------------------------------------------------------------
	//! \return True when the save loaded at boot is new enough to hold this payload. No stamped save (version 0:
	//! the load failed or nothing was loaded) counts as "has", so a broken load fails the check instead of skipping it.
	//! An unknown payload name logs an error and also counts as "has": the check runs and fails loudly.
	static bool SaveHas(string payload)
	{
		int since = LF_PersistenceComponentSerializer.PayloadSinceVersion(payload);
		if (since <= 0)
		{
			LF_Debug.Error(FEATURE, string.Format("e2e check asks for payload '%1', which RegisterPersistencePayloads does not list", payload));
			return true;
		}

		int boot = LF_SaveStamp.BootSaveVersion();
		if (boot <= 0)
			return true;

		return boot >= since;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the restart's first store read applied this save SECTION (`LF_<Area>SavePayload.NAME`,
	//! #2841). The section form of SaveHas: an older save (--pre-move, --legacy-save) holds no sections at all.
	static bool SaveHasSection(string name)
	{
		return LF_SaveSections.BootHadSection(name);
	}

	//------------------------------------------------------------------------------------------------
	//! Guard for a pass-2 check of a save section: `if (LF_E2E.RequiresSection(LF_XSavePayload.NAME)) { ...asserts... }`.
	//! \return True when the loaded save held the section; otherwise logs the skip and returns false.
	static bool RequiresSection(string name)
	{
		if (SaveHasSection(name))
			return true;

		LF_Debug.Info(FEATURE, string.Format("e2e check skipped: the loaded save (version %1) holds no '%2' save section, an older build wrote it", LF_SaveStamp.BootSaveVersion(), name));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Guard for a pass-2 check of a payload: `if (LF_E2E.RequiresPayload("x")) { ...asserts... }`.
	//! \return True when the loaded save can hold the payload; otherwise logs the skip and returns false.
	static bool RequiresPayload(string payload)
	{
		if (SaveHas(payload))
			return true;

		LF_Debug.Info(FEATURE, string.Format("e2e check skipped: the loaded save (version %1) is older than payload '%2' (version %3)", LF_SaveStamp.BootSaveVersion(), payload, LF_PersistenceComponentSerializer.PayloadSinceVersion(payload)));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Guard for a pass-2 check whose data has no payload of its own: pass 1 on an older build never seeded it.
	//! \param[in] what What the check covers, for the skip line.
	//! \param[in] seeded True when the loaded game holds what this tree's pass 1 seeds.
	//! \return `seeded`; logs the skip when false.
	static bool RequiresSeed(string what, bool seeded)
	{
		if (seeded)
			return true;

		LF_Debug.Info(FEATURE, string.Format("e2e check skipped: the loaded save (version %1) holds no %2, an older build wrote it", LF_SaveStamp.BootSaveVersion(), what));
		return false;
	}
}
