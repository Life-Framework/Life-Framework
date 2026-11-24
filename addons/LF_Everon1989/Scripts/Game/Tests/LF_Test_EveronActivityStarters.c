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
	protected void CheckRow(LF_TestContext ctx, LF_ActivityConfig config, string id, string kind)
	{
		LF_ActivityDef def;
		if (!RedProof())
			def = config.Find(id);

		ctx.NotNull(def, "the pack lists the starter: " + id);
		if (!def)
			return;

		ctx.EqualStr(string.Empty, LF_ActivityConfig.Problem(def), "the starter stands: " + id);
		ctx.True(def.m_iPayPerHour > 0, "the starter says what it pays: " + id);
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

		CheckRow(ctx, config, "crew", "OrgDesk");
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
	}
}
