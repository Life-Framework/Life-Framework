// red-proof: 2026-11-24 `tools/cli.mjs test --no-build --only LF_Test_EveronActivityStarters` ran on the table before the four rows existed: every row lookup came back null and the test failed (RED); with the rows in it passes.
//! everon/activity-starters (#4709): under the Everon 1989 header (the pack leg) the F2 "What to do next" table also
//! offers the four island starters after the seven earning ones: found a crew at the gang registry (`OrgDesk`), the
//! truck check at the north-east truck yard (`TruckCheckStart`), the perk pick at the labor office (`LabourOffice`)
//! and the first rumor of crime at the fence's yard (`FenceYard`). Each stands (LF_ActivityConfig.Problem), reads
//! from pack strings, and its first step guides to a place kind the island places.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronActivityStarters : LF_Test
{
	//! Place kinds the Saint-Philippe layers place (Place_saint-philippe_org, _labour, _fence and
	//! Place_ne-compound_truckyard under Worlds/MainWorld/MainWorld_Layers).
	protected static const string ISLAND_KINDS = "OrgDesk,TruckCheckStart,LabourOffice,FenceYard";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/activity-starters";
	}

	//------------------------------------------------------------------------------------------------
	//! \param wage False for a row that is a first step, not a job: it pays nothing and F2 says "no wage" (#5119).
	protected void CheckRow(LF_TestContext ctx, LF_ActivityConfig config, string id, string kind, bool wage = true)
	{
		LF_ActivityDef def;
		if (!RedProof())
			def = config.Find(id);

		ctx.NotNull(def, "the pack lists the starter: " + id);
		if (!def)
			return;

		ctx.EqualStr(string.Empty, LF_ActivityConfig.Problem(def), "the starter stands: " + id);
		if (wage)
		{
			ctx.True(def.m_iPayPerHour > 0, "the starter says what it pays: " + id);
		}
		else
		{
			// Founding a crew pays nothing: no made-up hourly figure on the row (#5119).
			ctx.Equal(0, def.m_iPayPerHour, "a starter that is not a wage carries no pay figure: " + id);
			string badge = LF_ActivityPage.PayText(def.m_iPayPerHour);
			ctx.EqualStr(LF_Terms.Text("#LF-Activity_PayNone"), badge, "its F2 badge says it pays no wage: " + id);
			ctx.False(badge.Contains("$"), "its F2 badge shows no money: " + id);
		}

		ctx.EqualStr(kind, def.m_sPlaceKind, "the starter's distance is to the place: " + id);
		ctx.True(def.StepCount() > 0, "the starter has a step: " + id);
		if (def.StepCount() == 0)
			return;

		LF_ActivityStep first = def.m_aSteps[0];
		ctx.EqualStr(kind, first.m_sTargetKind, "Guide me points at the place kind: " + id);
		array<string> islandKinds = {};
		ISLAND_KINDS.Split(",", islandKinds, false);
		ctx.True(islandKinds.Find(first.m_sTargetKind) != -1, "the island places that kind: " + first.m_sTargetKind);

		string name = WidgetManager.Translate(def.m_sNameKey);
		ctx.True(!name.IsEmpty() && name != def.m_sNameKey, "the name is in the string table: " + id);
		string pitch = WidgetManager.Translate(def.m_sPitchKey);
		ctx.True(!pitch.IsEmpty() && pitch != def.m_sPitchKey, "the pitch is in the string table: " + id);
		foreach (LF_ActivityStep step : def.m_aSteps)
		{
			string note = WidgetManager.Translate(step.m_sNoteKey);
			ctx.True(!note.IsEmpty() && note != step.m_sNoteKey, "the step note is in the string table: " + id + "/" + step.m_sStepId);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ActivityTable.ResetStatics();
		LF_ActivityConfig config = LF_ActivityTable.Get();
		ctx.NotNull(config, "the activity table loads");
		if (!config)
			return;

		CheckRow(ctx, config, "crew", "OrgDesk", false);
		CheckRow(ctx, config, "truckcheck", "TruckCheckStart");
		CheckRow(ctx, config, "perkpick", "LabourOffice");
		CheckRow(ctx, config, "rumor", "FenceYard");

		string ids = string.Empty;
		foreach (LF_ActivityDef def : config.m_aActivities)
		{
			if (!ids.IsEmpty())
				ids += ",";

			ids += def.m_sId;
		}

		ctx.EqualStr("mining,postal,lumber,fishing,apples,police,medic,crew,truckcheck,perkpick,rumor", ids, "the four starters follow the seven earning activities");

		CheckGates(ctx, config);
	}

	//------------------------------------------------------------------------------------------------
	//! #4771: the pack rows carry their gates and finish on the real event. A new civilian sees only the crew
	//! row; Hauling 5 or a van opens the truck check; any trade at level 10 opens the perk pick; a criminal
	//! record opens the rumor.
	protected void CheckGates(LF_TestContext ctx, LF_ActivityConfig config)
	{
		LF_ActivityDef crew = config.Find("crew");
		LF_ActivityDef truck = config.Find("truckcheck");
		LF_ActivityDef perk = config.Find("perkpick");
		LF_ActivityDef rumor = config.Find("rumor");
		if (!crew || !truck || !perk || !rumor)
			return;

		LF_TestGateFacts fresh = new LF_TestGateFacts();
		ctx.EqualStr(string.Empty, LF_ActivityRules.Gate(crew, fresh), "a new civilian sees the crew row");
		ctx.EqualStr("trade", LF_ActivityRules.Gate(truck, fresh), "a new civilian does not see the truck check");
		ctx.EqualStr("trade", LF_ActivityRules.Gate(perk, fresh), "a new civilian does not see the perk pick");
		ctx.EqualStr("record", LF_ActivityRules.Gate(rumor, fresh), "a new civilian does not see the rumor");

		LF_TestGateFacts miner = new LF_TestGateFacts();
		miner.m_mLevels.Set("MINER", 10);
		ctx.EqualStr(string.Empty, LF_ActivityRules.Gate(perk, miner), "a level-10 miner sees the perk pick");
		ctx.EqualStr("trade", LF_ActivityRules.Gate(truck, miner), "a level-10 miner has no Hauling 5 or van for the truck check");

		LF_TestGateFacts hauler = new LF_TestGateFacts();
		hauler.m_mLevels.Set("DELIVERY", 5);
		ctx.EqualStr(string.Empty, LF_ActivityRules.Gate(truck, hauler), "Hauling 5 sees the truck check");
		LF_TestGateFacts owner = new LF_TestGateFacts();
		owner.m_sOwnedClass = "van";
		ctx.EqualStr(string.Empty, LF_ActivityRules.Gate(truck, owner), "a van owner sees the truck check");

		LF_TestGateFacts crook = new LF_TestGateFacts();
		crook.m_bRecord = true;
		ctx.EqualStr(string.Empty, LF_ActivityRules.Gate(rumor, crook), "a criminal record sees the rumor");

		ctx.Equal(LF_EActivityCheck.CREW, crew.m_aSteps[0].m_eCheck, "the crew row finishes when a crew is founded");
		ctx.Equal(LF_EActivityCheck.FLAG, truck.m_aSteps[0].m_eCheck, "the truck check row finishes when the check is passed");
		ctx.EqualStr(LF_DrivingCheck.LICENCE_FLAG, truck.m_aSteps[0].m_sCheckParam, "on the truck licence flag");
		ctx.Equal(LF_EActivityCheck.PERK, perk.m_aSteps[0].m_eCheck, "the perk row finishes when a perk is picked");
		ctx.Equal(LF_EActivityCheck.ARRIVE, rumor.m_aSteps[0].m_eCheck, "the rumor row still finishes on arrival (no fence event yet)");
	}
}
