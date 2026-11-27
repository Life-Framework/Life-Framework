// red-proof: (#5147) the three item offences (#LF-Offence_UnlicensedItem, _RestrictedItem, _IllegalItem) read $0 on main, so their band rows fail until the pack gives them a fine. Earlier: on main without the pack's LF_CrimeCatalog_Everon1989.conf the new codes are not in the catalog and today's tickets keep $200, $500, $800 and $1,500, so every row fails (run `tools\cli test --no-build --red LF_Test_EveronLawBookAmounts`; RedProof() reads the core catalog alone).
//! #4208 (police L2, plan "Fines as minutes of work"): under the Everon 1989 header (the pack leg of
//! `tools\cli test`) every law-book code has a section and an amount inside its band, scaled to a civilian's
//! $8 a minute: minor road $50 to $75, road $150 to $250, serious road $300 to $400, minor criminal $50 to $200,
//! property $200 to $600 with 5 to 10 minutes in the cell as the other way out, violent charges and robbery
//! jail only (10 to 15 minutes in 0.3.0, the holding-cell cap). Any ticket-form code (a fine above 0) that the
//! table does not list fails too, so a stray old amount cannot ride along.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronLawBookAmounts : LF_Test
{
	protected static const int CLASS_MINOR_ROAD = 0;
	protected static const int CLASS_ROAD = 1;
	protected static const int CLASS_SERIOUS_ROAD = 2;
	protected static const int CLASS_MINOR_CRIMINAL = 3;
	protected static const int CLASS_PROPERTY = 4;
	protected static const int CLASS_VIOLENT = 5;
	protected static const int CLASS_WEAPONS = 6;
	protected static const int CLASS_AIR = 7;

	protected ref array<string> m_aIds = {};
	protected ref array<int> m_aSections = {};
	protected ref array<int> m_aClasses = {};

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/law-book-amounts";
	}

	//------------------------------------------------------------------------------------------------
	protected void Row(string id, LF_LawSection section, int amountClass)
	{
		m_aIds.Insert(id);
		m_aSections.Insert(section);
		m_aClasses.Insert(amountClass);
	}

	//------------------------------------------------------------------------------------------------
	//! The law book as the plan lists it: every code the pack adds or retunes, with its section and band.
	protected void BuildTable()
	{
		// Road traffic.
		Row("#LF-Ticket_OffenceSpeedingMinor", LF_LawSection.ROAD, CLASS_MINOR_ROAD);
		Row("#LF-Ticket_OffenceParking", LF_LawSection.ROAD, CLASS_MINOR_ROAD);
		Row("#LF-Ticket_OffenceInspection", LF_LawSection.ROAD, CLASS_MINOR_ROAD);
		Row("#LF-Ticket_OffenceSpeeding", LF_LawSection.ROAD, CLASS_ROAD);
		Row("#LF-Ticket_OffenceRecklessDriving", LF_LawSection.ROAD, CLASS_ROAD);
		Row("#LF-Ticket_OffenceOverloading", LF_LawSection.ROAD, CLASS_ROAD);
		Row("#LF-Ticket_OffenceSpeedingMajor", LF_LawSection.ROAD, CLASS_SERIOUS_ROAD);
		Row("#LF-Ticket_OffenceCrash", LF_LawSection.ROAD, CLASS_SERIOUS_ROAD);
		Row("#LF-Ticket_OffenceDangerousIntervention", LF_LawSection.ROAD, CLASS_SERIOUS_ROAD);
		Row("#LF-Ticket_OffenceNoDrivingLicence", LF_LawSection.ROAD, CLASS_SERIOUS_ROAD);
		Row("#LF-Ticket_OffenceHitAndRun", LF_LawSection.ROAD, CLASS_PROPERTY);
		Row("#LF-Ticket_OffenceNoFlyingLicence", LF_LawSection.ROAD, CLASS_AIR);

		// Criminal: minor, property, violent and robbery.
		Row("#LF-Ticket_OffenceInsult", LF_LawSection.CRIMINAL, CLASS_MINOR_CRIMINAL);
		Row("#LF-Ticket_OffenceTrespassing", LF_LawSection.CRIMINAL, CLASS_MINOR_CRIMINAL);
		Row("#LF-Ticket_OffenceThreat", LF_LawSection.CRIMINAL, CLASS_MINOR_CRIMINAL);
		Row("#LF-Ticket_OffenceDisturbance", LF_LawSection.CRIMINAL, CLASS_MINOR_CRIMINAL);
		Row("#LF-Ticket_OffenceDisobeyPolice", LF_LawSection.CRIMINAL, CLASS_MINOR_CRIMINAL);
		Row("#LF-Ticket_OffenceEmergencyLineAbuse", LF_LawSection.CRIMINAL, CLASS_MINOR_CRIMINAL);
		Row("#LF-Offence_Shoplifting", LF_LawSection.CRIMINAL, CLASS_PROPERTY);
		Row("#LF-Offence_BreakIn", LF_LawSection.CRIMINAL, CLASS_PROPERTY);
		Row("#LF-Offence_VehicleTheft", LF_LawSection.CRIMINAL, CLASS_PROPERTY);
		Row("#LF-Offence_WeaponTheft", LF_LawSection.CRIMINAL, CLASS_PROPERTY);
		Row("#LF-Offence_VinTampering", LF_LawSection.CRIMINAL, CLASS_PROPERTY);
		Row("#LF-Ticket_OffenceResistingArrest", LF_LawSection.CRIMINAL, CLASS_PROPERTY);
		Row("#LF-Offence_Robbery", LF_LawSection.CRIMINAL, CLASS_VIOLENT);
		Row("#LF-Offence_Carjacking", LF_LawSection.CRIMINAL, CLASS_VIOLENT);
		Row("#LF-Offence_CashVan", LF_LawSection.CRIMINAL, CLASS_VIOLENT);
		Row("#LF-Offence_HospitalRobbery", LF_LawSection.CRIMINAL, CLASS_VIOLENT);
		Row("#LF-Offence_ArmoryHeist", LF_LawSection.CRIMINAL, CLASS_VIOLENT);
		Row("#LF-Offence_Murder", LF_LawSection.CRIMINAL, CLASS_VIOLENT);
		Row("#LF-Offence_Mayhem", LF_LawSection.CRIMINAL, CLASS_VIOLENT);
		Row("#LF-Offence_MassiveMayhem", LF_LawSection.CRIMINAL, CLASS_VIOLENT);
		Row("#LF-Offence_HostageTaking", LF_LawSection.CRIMINAL, CLASS_VIOLENT);
		Row("#LF-Offence_DeprivationOfLiberty", LF_LawSection.CRIMINAL, CLASS_VIOLENT);

		// Narcotics by amount.
		Row("#LF-Ticket_OffenceNarcoticsA", LF_LawSection.NARCOTICS, CLASS_MINOR_CRIMINAL);
		Row("#LF-Ticket_OffenceNarcoticsB", LF_LawSection.NARCOTICS, CLASS_PROPERTY);
		Row("#LF-Offence_NarcoticsC", LF_LawSection.NARCOTICS, CLASS_VIOLENT);

		// Weapons.
		Row("#LF-Ticket_OffenceUnlawfulDischarge", LF_LawSection.WEAPONS, CLASS_WEAPONS);
		Row("#LF-Ticket_OffenceNoCarryZone", LF_LawSection.WEAPONS, CLASS_WEAPONS);
		Row("#LF-Ticket_OffenceWeaponNoLicence", LF_LawSection.WEAPONS, CLASS_WEAPONS);
		Row("#LF-Ticket_OffenceBrandishing", LF_LawSection.WEAPONS, CLASS_WEAPONS);
		Row("#LF-Ticket_OffenceIllegalWeapon", LF_LawSection.WEAPONS, CLASS_WEAPONS);
		Row("#LF-Offence_IllegalConversion", LF_LawSection.WEAPONS, CLASS_VIOLENT);

		// The search files these when it seizes a carried item (#5147: they read "$0" on the charge sheet).
		Row("#LF-Offence_UnlicensedItem", LF_LawSection.WEAPONS, CLASS_WEAPONS);
		Row("#LF-Offence_RestrictedItem", LF_LawSection.WEAPONS, CLASS_WEAPONS);
		Row("#LF-Offence_IllegalItem", LF_LawSection.WEAPONS, CLASS_WEAPONS);
	}

	//------------------------------------------------------------------------------------------------
	//! The band checks for one code; `amountClass` picks the band.
	protected void CheckBand(LF_TestContext ctx, LF_CrimeEntry entry, int amountClass)
	{
		string id = entry.m_sCrimeId;
		int fine = entry.m_iFine;
		int seconds = entry.m_iSentenceSeconds;

		switch (amountClass)
		{
			case CLASS_MINOR_ROAD:
				ctx.True(fine >= 50 && fine <= 75, string.Format("%1: minor road fine $50 to $75, got $%2", id, fine));
				ctx.Equal(0, seconds, string.Format("%1: a minor road code has no cell time", id));
				ctx.False(entry.m_bJailOnly, string.Format("%1: a minor road code takes a fine", id));
				break;
			case CLASS_ROAD:
				ctx.True(fine >= 150 && fine <= 250, string.Format("%1: road fine $150 to $250, got $%2", id, fine));
				ctx.Equal(0, seconds, string.Format("%1: a road code has no cell time", id));
				ctx.False(entry.m_bJailOnly, string.Format("%1: a road code takes a fine", id));
				break;
			case CLASS_SERIOUS_ROAD:
				ctx.True(fine >= 300 && fine <= 400, string.Format("%1: serious road fine $300 to $400, got $%2", id, fine));
				ctx.Equal(0, seconds, string.Format("%1: a serious road code has no cell time", id));
				ctx.False(entry.m_bJailOnly, string.Format("%1: a serious road code takes a fine", id));
				break;
			case CLASS_MINOR_CRIMINAL:
				ctx.True(fine >= 50 && fine <= 200, string.Format("%1: minor fine $50 to $200, got $%2", id, fine));
				ctx.Equal(0, seconds, string.Format("%1: a minor code has no cell time", id));
				ctx.False(entry.m_bJailOnly, string.Format("%1: a minor code takes a fine", id));
				break;
			case CLASS_PROPERTY:
				ctx.True(fine >= 200 && fine <= 600, string.Format("%1: property fine $200 to $600, got $%2", id, fine));
				ctx.True(seconds >= 300 && seconds <= 600, string.Format("%1: property cell time 5 to 10 min, got %2 s", id, seconds));
				ctx.False(entry.m_bJailOnly, string.Format("%1: a property code offers the fine", id));
				break;
			case CLASS_VIOLENT:
				ctx.True(entry.m_bJailOnly, string.Format("%1: a violent or robbery code is jail only", id));
				ctx.Equal(0, fine, string.Format("%1: a jail-only code has no fine", id));
				ctx.True(seconds >= 600 && seconds <= 900, string.Format("%1: jail only 10 to 15 min in 0.3.0, got %2 s", id, seconds));
				break;
			case CLASS_WEAPONS:
				ctx.True(fine >= 250 && fine <= 800, string.Format("%1: weapons fine $250 to $800, got $%2", id, fine));
				ctx.False(entry.m_bJailOnly, string.Format("%1: a weapons code takes a fine", id));
				break;
			case CLASS_AIR:
				ctx.True(fine >= 600 && fine <= 800, string.Format("%1: flying without a license $600 to $800, got $%2", id, fine));
				ctx.False(entry.m_bJailOnly, string.Format("%1: flying without a license takes a fine", id));
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The act: the running header's overlays layered over the core catalog. The red run reads the core alone.
		if (RedProof())
			LF_ConfigService.SetPackSource(new LF_ConfigPackSource());
		LF_CrimeCatalogConfig.Reload();
		LF_CrimeCatalogConfig config = LF_CrimeCatalogConfig.Get();
		if (!config)
		{
			ctx.Fail("the crime catalog did not load");
			return;
		}

		BuildTable();
		array<int> sectionCount = {0, 0, 0, 0};
		foreach (int i, string id : m_aIds)
		{
			LF_CrimeEntry entry = config.Find(id);
			if (!entry)
			{
				ctx.Fail(string.Format("%1 is not in the law book", id));
				continue;
			}

			ctx.True(entry.m_eLawSection == m_aSections[i], string.Format("%1 is in section %2, expected %3", id, entry.m_eLawSection, m_aSections[i]));
			sectionCount[entry.m_eLawSection] = sectionCount[entry.m_eLawSection] + 1;
			CheckBand(ctx, entry, m_aClasses[i]);
		}

		// Every section of the lectern lists at least one code.
		foreach (int section, int count : sectionCount)
			ctx.True(count > 0, string.Format("law book section %1 lists no code", section));

		// A ticket-form code (a fine above 0) the table does not list is an amount nobody scaled.
		foreach (LF_CrimeEntry entry : config.m_aCrimes)
		{
			if (entry && entry.m_iFine > 0)
				ctx.True(m_aIds.Find(entry.m_sCrimeId) >= 0, string.Format("%1 has a fine of $%2 and no row in the table", entry.m_sCrimeId, entry.m_iFine));
		}

		// The three speeding classes are one stacking group: a stop for 40 over is one charge, not three.
		LF_CrimeEntry minor = config.Find("#LF-Ticket_OffenceSpeedingMinor");
		LF_CrimeEntry mid = config.Find("#LF-Ticket_OffenceSpeeding");
		LF_CrimeEntry major = config.Find("#LF-Ticket_OffenceSpeedingMajor");
		if (minor && mid && major)
		{
			ctx.False(mid.m_sStackGroup.IsEmpty(), "the speeding codes have a stacking group");
			ctx.EqualStr(mid.m_sStackGroup, minor.m_sStackGroup, "speeding up to 15 over stacks with speeding 16 to 30 over");
			ctx.EqualStr(mid.m_sStackGroup, major.m_sStackGroup, "speeding over 30 stacks with speeding 16 to 30 over");
		}

		LF_ConfigService.SetPackSource(null);
		LF_CrimeCatalogConfig.Reload();
	}
}
