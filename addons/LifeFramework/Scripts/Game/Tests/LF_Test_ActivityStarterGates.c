// red-proof: 2026-11-24 `tools/cli.mjs test --no-build --red LF_Test_ActivityStarterGates`: see the RED-OK line in the PR (the hook leaves every gate field off the defs, so each "hidden" assertion and each wrong-state check fails).
//! Hand-filled facts for the starter gates: trade levels, an owned vehicle class, a record, a crew, a
//! perk and a progression flag.
class LF_TestGateFacts : LF_ActivityFacts
{
	ref map<string, int> m_mLevels = new map<string, int>();
	ref array<string> m_aFlags = {};
	string m_sOwnedClass;
	bool m_bRecord;
	bool m_bCrew;
	bool m_bPerk;

	override int Level(string job)
	{
		if (!m_mLevels.Contains(job))
			return 1;

		return m_mLevels.Get(job);
	}

	override void Trades(notnull array<string> into)
	{
		into.Clear();
		foreach (string job, int level : m_mLevels)
		{
			if (job != "CIVILIAN")
				into.Insert(job);
		}
	}

	override bool OwnsVehicleClass(string capacityClass)
	{
		// The ladder of LF_Capacity, lowest first: an owned class opens its own gate and every one below it.
		array<string> ladder = {"car", "pickup", "van", "truck"};
		int ownedRung = ladder.Find(m_sOwnedClass);
		int neededRung = ladder.Find(capacityClass);
		return ownedRung != -1 && neededRung != -1 && ownedRung >= neededRung;
	}

	override bool HasCriminalRecord()
	{
		return m_bRecord;
	}

	override bool InCrew()
	{
		return m_bCrew;
	}

	override bool HasPickedPerk()
	{
		return m_bPerk;
	}

	override bool HasFlag(string flag)
	{
		return m_aFlags.Find(flag) != -1;
	}
}

//------------------------------------------------------------------------------------------------
//! The F2 starter gates (#4771): an activity can be gated on a trade level (one trade or the best of any),
//! on owning a vehicle class (opening it beside the trade gate: "a van or Hauling 5") and on a criminal
//! record, and a step can be done by the real thing (a crew joined, a perk picked, a progression flag).
//! No world: the rules read hand-filled facts. The Everon rows that use them are checked by
//! `everon/activity-starters` in the pack leg.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ActivityStarterGates : LF_Test
{
	override string GetName()
	{
		return "activities/starter-gates";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ActivityDef Def(string id, LF_EActivityCheck check, string param = "")
	{
		LF_ActivityDef def = LF_ActivityDef.Make(id);
		LF_ActivityStep step = new LF_ActivityStep();
		step.m_sStepId = "step";
		step.m_sTitleKey = "#T";
		step.m_sNoteKey = "#N";
		step.m_eCheck = check;
		step.m_sCheckParam = param;
		step.m_aObjectives = {};
		def.m_aSteps.Insert(step);
		return def;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The perk pick: any trade at level 10.
		LF_ActivityDef perk = Def("perk", LF_EActivityCheck.PERK);
		if (!RedProof())
		{
			perk.m_sTrade = LF_ActivityRules.ANY_TRADE;
			perk.m_iMinTradeLevel = 10;
		}

		LF_TestGateFacts fresh = new LF_TestGateFacts();
		ctx.EqualStr("trade", LF_ActivityRules.Gate(perk, fresh), "a level-1 civilian does not see the perk row");
		ctx.True(LF_ActivityRules.StateOf(perk, fresh) == LF_EActivityState.LOCKED, "and it is not listed for them");

		LF_TestGateFacts nine = new LF_TestGateFacts();
		nine.m_mLevels.Set("MINER", 9);
		ctx.EqualStr("trade", LF_ActivityRules.Gate(perk, nine), "a level-9 miner does not see the perk row yet");

		LF_TestGateFacts miner = new LF_TestGateFacts();
		miner.m_mLevels.Set("MINER", 10);
		ctx.EqualStr(string.Empty, LF_ActivityRules.Gate(perk, miner), "a level-10 miner sees the perk row");
		ctx.True(LF_ActivityRules.StateOf(perk, miner) == LF_EActivityState.AVAILABLE, "and can start it");

		LF_TestGateFacts civilian = new LF_TestGateFacts();
		civilian.m_mLevels.Set("CIVILIAN", 12);
		ctx.EqualStr("trade", LF_ActivityRules.Gate(perk, civilian), "the civilian ladder is no trade");

		// One named trade: Hauling is the DELIVERY job. Mining 10 does not open it.
		LF_ActivityDef hauling = Def("hauling", LF_EActivityCheck.FLAG, "truck_licence");
		if (!RedProof())
		{
			hauling.m_sTrade = "DELIVERY";
			hauling.m_iMinTradeLevel = 5;
		}

		ctx.EqualStr("trade", LF_ActivityRules.Gate(hauling, miner), "a level-10 miner does not pass a Hauling 5 gate");
		LF_TestGateFacts hauler = new LF_TestGateFacts();
		hauler.m_mLevels.Set("DELIVERY", 5);
		ctx.EqualStr(string.Empty, LF_ActivityRules.Gate(hauling, hauler), "Hauling 5 opens the truck check");

		// A van opens it too, either gate is enough; a car does not.
		if (!RedProof())
			hauling.m_sOwnsClass = "van";

		LF_TestGateFacts car = new LF_TestGateFacts();
		car.m_sOwnedClass = "car";
		ctx.EqualStr("trade", LF_ActivityRules.Gate(hauling, car), "a car is not a van");
		LF_TestGateFacts van = new LF_TestGateFacts();
		van.m_sOwnedClass = "van";
		ctx.EqualStr(string.Empty, LF_ActivityRules.Gate(hauling, van), "owning a van opens the truck check at level 1");
		LF_TestGateFacts truck = new LF_TestGateFacts();
		truck.m_sOwnedClass = "truck";
		ctx.EqualStr(string.Empty, LF_ActivityRules.Gate(hauling, truck), "a truck is a van or better");
		ctx.EqualStr(string.Empty, LF_ActivityRules.Gate(hauling, hauler), "Hauling 5 still opens it with no van");
		ctx.EqualStr("trade", LF_ActivityRules.Gate(hauling, fresh), "a new civilian sees neither");

		// The rumor: only after a criminal record.
		LF_ActivityDef rumor = Def("rumor", LF_EActivityCheck.ARRIVE);
		rumor.m_aSteps[0].m_sTargetKind = "FenceYard";
		if (!RedProof())
			rumor.m_bNeedsRecord = true;

		ctx.EqualStr("record", LF_ActivityRules.Gate(rumor, fresh), "no record hides the rumor row");
		LF_TestGateFacts crook = new LF_TestGateFacts();
		crook.m_bRecord = true;
		ctx.EqualStr(string.Empty, LF_ActivityRules.Gate(rumor, crook), "a record shows the rumor row");
		ctx.EqualStr("trade", LF_ActivityRules.Gate(perk, crook), "a record does not open the perk row");

		// A row with no gate field is open to everyone, as before.
		LF_ActivityDef open = Def("open", LF_EActivityCheck.ARRIVE);
		open.m_aSteps[0].m_sTargetKind = "job";
		ctx.EqualStr(string.Empty, LF_ActivityRules.Gate(open, fresh), "an ungated activity shows to a new civilian");

		// The real events: a crew joined, a perk picked, a flag set complete their steps.
		LF_ActivityStep crewStep = new LF_ActivityStep();
		crewStep.m_eCheck = LF_EActivityCheck.CREW;
		LF_ActivityStep perkStep = new LF_ActivityStep();
		perkStep.m_eCheck = LF_EActivityCheck.PERK;
		LF_ActivityStep flagStep = hauling.m_aSteps[0];
		LF_TestGateFacts member = new LF_TestGateFacts();
		member.m_bCrew = true;
		member.m_bPerk = true;
		member.m_aFlags.Insert("truck_licence");
		ctx.False(LF_ActivityRules.CheckHolds(crewStep, fresh, 0, 0), "standing at the registry founds no crew");
		ctx.False(LF_ActivityRules.CheckHolds(perkStep, fresh, 0, 0), "standing at the counter picks no perk");
		ctx.False(LF_ActivityRules.CheckHolds(flagStep, fresh, 0, 0), "standing at the yard passes no truck check");
		ctx.True(LF_ActivityRules.CheckHolds(crewStep, member, 0, 0), "a crew member has founded or joined a crew");
		ctx.True(LF_ActivityRules.CheckHolds(perkStep, member, 0, 0), "a perk picked completes the pick step");
		ctx.True(LF_ActivityRules.CheckHolds(flagStep, member, 0, 0), "the truck licence flag completes the truck check step");

		// A flag step with no flag stands for nothing.
		LF_ActivityDef noFlag = Def("noflag", LF_EActivityCheck.FLAG);
		ctx.True(!LF_ActivityConfig.Problem(noFlag).IsEmpty(), "a flag check with no flag is refused");
		ctx.EqualStr(string.Empty, LF_ActivityConfig.Problem(Def("crew", LF_EActivityCheck.CREW)), "a crew check needs no parameter");
	}
}
