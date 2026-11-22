//------------------------------------------------------------------------------------------------
//! The law book's four sections (#4202). A code belongs to one; the lectern reads one section at a
//! time. The order is the book's order, and the number is what the catalog `.conf` stores.
enum LF_LawSection
{
	CRIMINAL,
	ROAD,
	NARCOTICS,
	WEAPONS
}

//------------------------------------------------------------------------------------------------
//! Names and lookups for the law book's codes. A code is an `LF_CrimeEntry`: the section, the stacking
//! group and the jail-only flag sit beside its fine and sentence, so there is one catalog and one id.
class LF_LawCode
{
	static const string FEATURE = "Law";

	static const string KEY_CRIMINAL = "criminal";
	static const string KEY_ROAD = "road";
	static const string KEY_NARCOTICS = "narcotics";
	static const string KEY_WEAPONS = "weapons";

	//------------------------------------------------------------------------------------------------
	//! The owner-config key for a section ("road").
	static string SectionKey(LF_LawSection section)
	{
		switch (section)
		{
			case LF_LawSection.ROAD: return KEY_ROAD;
			case LF_LawSection.NARCOTICS: return KEY_NARCOTICS;
			case LF_LawSection.WEAPONS: return KEY_WEAPONS;
		}

		return KEY_CRIMINAL;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads an owner-config section key.
	//! \return True and the section when the key is one of the four; false (and `section` untouched) otherwise.
	static bool ParseSection(string key, inout LF_LawSection section)
	{
		if (key == KEY_CRIMINAL)
			section = LF_LawSection.CRIMINAL;
		else if (key == KEY_ROAD)
			section = LF_LawSection.ROAD;
		else if (key == KEY_NARCOTICS)
			section = LF_LawSection.NARCOTICS;
		else if (key == KEY_WEAPONS)
			section = LF_LawSection.WEAPONS;
		else
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True when an int read from a `.conf` is one of the four sections.
	static bool IsSection(int value)
	{
		return value >= LF_LawSection.CRIMINAL && value <= LF_LawSection.WEAPONS;
	}

	//------------------------------------------------------------------------------------------------
	//! The catalog entries for these offence ids, in order. An id the catalog does not list is skipped
	//! (LF_CrimeCatalog.Lookup logs it once).
	static void Resolve(notnull array<string> crimeIds, notnull array<LF_CrimeEntry> outCodes)
	{
		outCodes.Clear();
		foreach (string crimeId : crimeIds)
		{
			LF_CrimeEntry entry = LF_CrimeCatalog.Lookup(crimeId);
			if (entry)
				outCodes.Insert(entry);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The codes of one section, in catalog order: what the lectern lists under that heading.
	static void CollectSection(LF_LawSection section, notnull array<LF_CrimeEntry> outCodes)
	{
		outCodes.Clear();
		LF_CrimeCatalogConfig config = LF_CrimeCatalogConfig.Get();
		if (!config || !config.m_aCrimes)
			return;

		foreach (LF_CrimeEntry entry : config.m_aCrimes)
		{
			if (entry && !entry.m_sCrimeId.IsEmpty() && entry.m_eLawSection == section)
				outCodes.Insert(entry);
		}
	}
}
