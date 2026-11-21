// red-proof: RedProof() skips loading the three tables, so `the course table loads`, `the compartment table loads` and `the seizure rules load` fail (run `tools\cli test --no-build --red LF_Test_EveronDetective`).
//! #3436 in game: under the Everon 1989 header (the pack leg of `tools\cli test`) the pack overlays the
//! detective and traffic courses (rank, fee, study time), the 1989 licence seizure rows and the vehicles
//! that take a hidden compartment. Every seizure row names a real crime code and a real licence, every
//! compartment row resolves to a vehicle prefab (the period Lada, Skoda and UAZ cars, no stock truck),
//! and a course fee stays a small cost against a day's pay.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronDetective : LF_Test
{
	static const ResourceName OVERLAY_COURSES = "{3436A00000000001}Configs/Qualifications/LF_Qualifications_Everon1989.conf";
	static const ResourceName OVERLAY_COMPARTMENTS = "{3436A00000000002}Configs/Vehicles/LF_Compartments_Everon1989.conf";
	static const ResourceName OVERLAY_SEIZURE = "{3436A00000000003}Configs/Crime/LF_LicenceSeizure_Everon1989.conf";
	static const ResourceName UAZ469 = "{99F1610551D54D17}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_base.et";
	static const ResourceName LADA_2101 = "{5E696DD4888CE406}Prefabs/Vehicles/Wheeled/V2101/V2101_beige.et";
	static const ResourceName AMBULANCE = "{D8C109C681E7AAF1}Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et";
	//! A courier day in the flagship: six parcel runs at 40 (LF_Jobs_Everon1989.conf).
	static const int DAY_PAY = 240;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/detective";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
		{
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY_COURSES) != -1, "the header lists the course overlay");
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY_COMPARTMENTS) != -1, "the header lists the compartment overlay");
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY_SEIZURE) != -1, "the header lists the seizure overlay");
		}

		LF_QualificationConfig courses;
		LF_VehicleCompartmentConfig compartments;
		LF_LicenceSeizureRules seizure;
		if (!RedProof())
		{
			LF_QualificationConfig.Reload();
			LF_VehicleCompartmentConfig.Reload();
			LF_LicenceSeizureRules.ResetStatics();
			courses = LF_QualificationConfig.Get();
			compartments = LF_VehicleCompartmentConfig.Get();
			seizure = LF_LicenceSeizureRules.Get();
		}

		CheckCourses(ctx, courses);
		CheckCompartments(ctx, compartments);
		CheckSeizure(ctx, seizure);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckCourses(LF_TestContext ctx, LF_QualificationConfig courses)
	{
		ctx.NotNull(courses, "the course table loads");
		if (!courses)
			return;

		LF_QualificationCourse detective = courses.Find("detective_course");
		LF_QualificationCourse traffic = courses.Find("traffic_course");
		ctx.NotNull(detective, "the detective course is in the table");
		ctx.NotNull(traffic, "the traffic course is in the table");
		if (!detective || !traffic)
			return;

		ctx.EqualStr("police", detective.m_sFaction, "the detective course is for the police");
		ctx.True(detective.m_iMinRank >= 1, "the detective course needs a rank above the entry rank");
		ctx.True(detective.m_iStudyMinutes > traffic.m_iStudyMinutes, "the detective course takes longer to study than the traffic course");
		ctx.True(detective.m_iFee > traffic.m_iFee, "the detective course costs more than the traffic course");
		ctx.True(traffic.m_iFee > 0 && detective.m_iFee < 2 * DAY_PAY, "a course fee is a cost to the officer, under two days of civilian pay");

		array<string> unlocks = {};
		detective.GetUnlocks(unlocks);
		ctx.True(unlocks.Find("detective") != -1, "the detective course unlocks detective");
		unlocks.Clear();
		traffic.GetUnlocks(unlocks);
		ctx.True(unlocks.Find("traffic") != -1, "the traffic course unlocks traffic");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckCompartments(LF_TestContext ctx, LF_VehicleCompartmentConfig compartments)
	{
		ctx.NotNull(compartments, "the compartment table loads");
		if (!compartments)
			return;

		ctx.True(compartments.m_aRows.Count() >= 4, "the pack names the period cars that take a compartment");
		foreach (LF_VehicleCompartmentRow row : compartments.m_aRows)
		{
			Resource resource = Resource.Load(row.m_sPrefab);
			ctx.True(resource != null && resource.IsValid(), string.Format("%1 resolves to a vehicle prefab", row.m_sPrefab));
			ctx.True(row.m_iSize >= 1 && row.m_iSize <= LF_VehicleOwnership.COMPARTMENT_MAX, string.Format("%1 has a size inside 1-%2", row.m_sPrefab, LF_VehicleOwnership.COMPARTMENT_MAX));
			ctx.True(row.m_iFitPrice > 0, string.Format("%1 costs something to fit", row.m_sPrefab));
		}

		ctx.True(LF_VehicleCompartmentConfig.SizeFor(LADA_2101) > 0, "the Lada 2101 takes a compartment");
		ctx.True(LF_VehicleCompartmentConfig.SizeFor(UAZ469) > LF_VehicleCompartmentConfig.SizeFor(LADA_2101), "the UAZ takes a bigger compartment than the Lada");
		ctx.Equal(0, LF_VehicleCompartmentConfig.SizeFor(AMBULANCE), "the ambulance takes no compartment");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSeizure(LF_TestContext ctx, LF_LicenceSeizureRules seizure)
	{
		ctx.NotNull(seizure, "the seizure rules load");
		if (!seizure)
			return;

		LF_CrimeCatalogConfig catalog = LF_CrimeCatalogConfig.Get();
		ctx.NotNull(catalog, "the crime catalog loads");
		ctx.True(seizure.Find("#LF-Offence_VehicleTheft") != null, "vehicle theft takes a licence");
		ctx.True(seizure.Find("#LF-Offence_Carjacking") != null, "carjacking takes a licence");
		ctx.True(seizure.Find("#LF-Ticket_OffenceBrandishing") != null, "the core brandishing row stays under the pack rows");

		// Every row the pack adds: a real catalog crime and real licence names (a core row is the core's own check).
		array<string> packCodes = {"#LF-Offence_VehicleTheft", "#LF-Offence_Carjacking", "#LF-Offence_ArmoryHeist", "#LF-Offence_Robbery", "#LF-Ticket_OffenceSpeeding"};
		foreach (string code : packCodes)
		{
			bool known;
			if (catalog && catalog.m_aCrimes)
			{
				foreach (LF_CrimeEntry entry : catalog.m_aCrimes)
				{
					if (entry && entry.m_sCrimeId == code)
						known = true;
				}
			}

			ctx.True(known, string.Format("seizure row %1 names a crime in the catalog", code));
			LF_LicenceSeizureRule rule = seizure.Find(code);
			ctx.NotNull(rule, string.Format("the pack adds a seizure row for %1", code));
			if (!rule)
				continue;

			ctx.True(rule.m_aLicences != null && !rule.m_aLicences.IsEmpty(), string.Format("seizure row %1 takes a licence", code));
			foreach (string name : rule.m_aLicences)
			{
				LF_ELicenseType licence = typename.StringToEnum(LF_ELicenseType, name);
				ctx.EqualStr(name, typename.EnumToString(LF_ELicenseType, licence), string.Format("seizure row %1 names a real licence (%2)", code, name));
			}
		}

		array<string> codes = {"#LF-Offence_VehicleTheft"};
		ctx.Equal(14, LF_LicenceSeizureRules.BanDays(codes, LF_ELicenseType.DRIVING_LICENCE), "a vehicle theft conviction bans the driving licence for 14 days");
	}
}
