// red-proof: 2026-11-23 `tools\cli test --no-build --red LF_Test_RunPlate` RED PROOF OK: with RedProof() on the three runs are skipped, so the STOLEN, CLEAR and UNKNOWN outcomes and the three pager lines fail ("the pager holds no message 0"). Green without the hook.
//! police/run-plate (#4425): an on-duty officer at the wheel of a police car runs the plate of the car
//! ahead. A reported car reads STOLEN, a registered one CLEAR, an unregistered one UNKNOWN, each with its
//! plate on the officer's pager; no car ahead, an officer off duty, or one not at the wheel is refused in
//! words and writes nothing. The server finds the car itself (FindAhead): a car behind or past 30 m is not it.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_RunPlate : LF_Test
{
	protected static const ResourceName MARKER = "{5D2B8E41A7C3906F}Prefabs/Systems/Vehicles/LF_PoliceCarMarker.et";
	protected static const ResourceName PAGER_PREFAB = "{747E7E1669676AA1}Prefabs/Items/Roleplay/LF_Pager.et";
	protected static const string OFFICER = "run-plate-officer";
	protected static const string PLATE_STOLEN = "RP ST 4425";
	protected static const string PLATE_CLEAR = "RP CL 4425";
	protected static const string PLATE_UNKNOWN = "RP UN 4425";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/run-plate";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_StolenVehicles.ResetStatics();
		RunPure(ctx);

		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer)
			return;

		IEntity pagerEntity = LF_Utils.SpawnEntityPrefab(PAGER_PREFAB, vector.Zero);
		IEntity markerA = LF_Utils.SpawnEntityPrefab(MARKER, officer.GetOrigin() + "6 0.5 0");
		IEntity markerB = LF_Utils.SpawnEntityPrefab(MARKER, officer.GetOrigin() + "6 0.5 12");
		LF_ParkedCarMarkerComponent markerOne;
		LF_ParkedCarMarkerComponent markerTwo;
		if (markerA)
			markerOne = LF_Component<LF_ParkedCarMarkerComponent>.Find(markerA);

		if (markerB)
			markerTwo = LF_Component<LF_ParkedCarMarkerComponent>.Find(markerB);

		IEntity policeCar;
		IEntity ahead;
		if (markerOne)
			policeCar = markerOne.SpawnCar("test");

		if (markerTwo)
			ahead = markerTwo.SpawnCar("test");

		ctx.True(pagerEntity && policeCar && ahead, "a pager, a police car and a second car spawn");
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(OFFICER);
		LF_PagerComponent pager;
		if (pagerEntity)
			pager = LF_Component<LF_PagerComponent>.Find(pagerEntity);

		if (pager && policeCar && ahead && account)
		{
			LF_Utils.SetUidForTests(officer, OFFICER);
			RunCars(ctx, officer, policeCar, ahead, pager, account);
		}

		if (account)
		{
			account.SetOnDuty(false);
			account.SetFaction(LF_Faction.CIVILIAN);
		}

		LF_Utils.SetUidForTests(null, "");
		LF_LicensePlateRegistry.Release(PLATE_CLEAR);
		LF_LicensePlateRegistry.Release(PLATE_STOLEN);
		LF_StolenVehicles.ResetStatics();
		LF_TestCharacterFixture.Delete(policeCar);
		LF_TestCharacterFixture.Delete(ahead);
		LF_TestCharacterFixture.Delete(markerA);
		LF_TestCharacterFixture.Delete(markerB);
		LF_TestCharacterFixture.Delete(pagerEntity);
		LF_TestCharacterFixture.Delete(officer);
	}

	//------------------------------------------------------------------------------------------------
	//! The cone, the decision order and the pager line, no world.
	protected void RunPure(LF_TestContext ctx)
	{
		vector at = "100 0 100";
		vector fwd = "0 0 1";
		ctx.True(LF_RunPlateRules.IsAhead(at, fwd, at + "0 0 12"), "a car 12 m dead ahead is in front");
		ctx.True(LF_RunPlateRules.IsAhead(at, fwd, at + "1.5 0 20"), "a car a lane off, 20 m ahead, is in front");
		ctx.False(LF_RunPlateRules.IsAhead(at, fwd, at + "0 0 -12"), "a car behind is not in front");
		ctx.False(LF_RunPlateRules.IsAhead(at, fwd, at + "0 0 45"), "a car past 30 m is not in front");
		ctx.False(LF_RunPlateRules.IsAhead(at, fwd, at + "18 0 6"), "a car off to the side is not in front");
		ctx.False(LF_RunPlateRules.IsAhead(at, fwd, at + "0 0 0.5"), "our own car is not in front");

		ctx.True(LF_RunPlateRules.Decide(false, true, true, LF_EPlateStatus.STOLEN) == LF_ERunPlate.OFF_DUTY, "off duty is refused first");
		ctx.True(LF_RunPlateRules.Decide(true, false, true, LF_EPlateStatus.STOLEN) == LF_ERunPlate.NOT_DRIVING, "not at the wheel is refused");
		ctx.True(LF_RunPlateRules.Decide(true, true, false, LF_EPlateStatus.STOLEN) == LF_ERunPlate.NO_CAR, "no car ahead is refused");
		ctx.True(LF_RunPlateRules.Decide(true, true, true, LF_EPlateStatus.STOLEN) == LF_ERunPlate.STOLEN, "a reported plate reads STOLEN");
		ctx.True(LF_RunPlateRules.Decide(true, true, true, LF_EPlateStatus.CLEAR) == LF_ERunPlate.CLEAR, "a registered plate reads CLEAR");
		ctx.True(LF_RunPlateRules.Decide(true, true, true, LF_EPlateStatus.UNKNOWN) == LF_ERunPlate.UNKNOWN, "an unregistered plate reads UNKNOWN");
		ctx.False(LF_RunPlateRules.RefusalKey(LF_ERunPlate.NO_CAR).IsEmpty(), "a refusal has words");
		ctx.True(LF_RunPlateRules.RefusalKey(LF_ERunPlate.STOLEN).IsEmpty(), "an answer is not a refusal");

		string entry = LF_Pager.Entry("21:14", LF_RunPlate.Summary(PLATE_STOLEN, LF_ERunPlate.STOLEN), "Place");
		ctx.True(LF_RunPlate.IsEntry(entry), "a plate answer reads as one");
		ctx.False(LF_RunPlate.IsEntry(LF_Pager.Entry("21:14", "robbery", "Place")), "a dispatch line is not a plate answer");
		ctx.False(LF_RunPlate.IsEntry(LF_Pager.Entry("21:14", LF_StolenCarPager.Summary(PLATE_STOLEN, "#key"), "Place")), "a hotlist line is not a plate answer");
		array<string> messages = {};
		messages.Insert(entry);
		array<ref LF_UIPart> card = LF_Pager.BuildParts(messages);
		ctx.Equal(3, card.Count(), "the pager card is a title, the answer line and a close button");
		bool shown;
		foreach (LF_UIPart part : card)
		{
			if (part && part.GetText().Contains(PLATE_STOLEN))
				shown = true;
		}

		ctx.True(shown, "the pager card shows the plate");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCars(LF_TestContext ctx, IEntity officer, IEntity policeCar, IEntity ahead, LF_PagerComponent pager, LF_PlayerAccount account)
	{
		// The second car was spawned 12 m in front of the police car (a moved car is not in the world query until the next frame).
		ctx.True(LF_RunPlate.FindAhead(policeCar) == ahead, "the server finds the car 12 m ahead");
		ctx.True(LF_RunPlate.FindAhead(ahead) == null, "the police car behind it is not in front of it");

		LF_LicensePlateManagerComponent plates = LF_Component<LF_LicensePlateManagerComponent>.Find(ahead);
		ctx.NotNull(plates, "the car ahead carries a plate manager");
		if (!plates)
			return;

		array<LF_PagerComponent> pagers = {};
		pagers.Insert(pager);

		// Off duty: refused in words, the pager stays empty.
		account.SetFaction(LF_Faction.CIVILIAN);
		account.SetOnDuty(false);
		plates.m_Registration = PLATE_STOLEN;
		LF_StolenVehicles.FileReport("run-plate-stolen", PLATE_STOLEN, "#EL-Vehicle_S105", "run-plate-owner", "#LF-Wanted_PlaceGrid 003-002");
		ctx.True(LF_RunPlate.Run(officer, true, ahead, pagers, "21:14") == LF_ERunPlate.OFF_DUTY, "an officer off duty is refused");
		ctx.Equal(0, pager.MessageCount(), "a refused run writes nothing to the pager");

		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);

		// Not at the wheel and no car ahead: refused, the pager stays empty.
		ctx.True(LF_RunPlate.Run(officer, false, ahead, pagers, "21:14") == LF_ERunPlate.NOT_DRIVING, "an officer not at the wheel is refused");
		ctx.True(LF_RunPlate.Run(officer, true, null, pagers, "21:14") == LF_ERunPlate.NO_CAR, "no car ahead is refused");
		ctx.Equal(0, pager.MessageCount(), "the refusals wrote nothing to the pager");

		// A reported car reads STOLEN with its plate on the pager. RedProof() skips the run.
		LF_ERunPlate outcome = LF_ERunPlate.NO_CAR;
		if (!RedProof())
			outcome = LF_RunPlate.Run(officer, true, ahead, pagers, "21:14");

		ctx.True(outcome == LF_ERunPlate.STOLEN, "a reported car ahead reads STOLEN");
		ctx.Equal(1, pager.MessageCount(), "the answer is on the pager");
		ExpectLine(ctx, pager, PLATE_STOLEN, 0);

		// A registered car reads CLEAR.
		plates.m_Registration = PLATE_CLEAR;
		LF_LicensePlateRegistry.Remember(PLATE_CLEAR, "run-plate-clear");
		if (!RedProof())
			outcome = LF_RunPlate.Run(officer, true, ahead, pagers, "21:15");

		ctx.True(outcome == LF_ERunPlate.CLEAR, "a registered car ahead reads CLEAR");
		ctx.Equal(2, pager.MessageCount(), "the second answer is on the pager");
		ExpectLine(ctx, pager, PLATE_CLEAR, 0);

		// An unregistered plate reads UNKNOWN.
		plates.m_Registration = PLATE_UNKNOWN;
		if (!RedProof())
			outcome = LF_RunPlate.Run(officer, true, ahead, pagers, "21:16");

		ctx.True(outcome == LF_ERunPlate.UNKNOWN, "an unregistered car ahead reads UNKNOWN");
		ctx.Equal(3, pager.MessageCount(), "the third answer is on the pager");
		ExpectLine(ctx, pager, PLATE_UNKNOWN, 0);

		// The real action path: the officer is not seated in the car, so the server refuses on the seat.
		ctx.True(LF_RunPlate.RunFromCar(policeCar, officer) == LF_ERunPlate.NOT_DRIVING, "pressing the action from outside the car is refused");
		ctx.Equal(3, pager.MessageCount(), "the refused press wrote nothing");
	}

	//------------------------------------------------------------------------------------------------
	//! Message `index` of the pager is a plate answer that shows the plate.
	protected void ExpectLine(LF_TestContext ctx, LF_PagerComponent pager, string plate, int index)
	{
		array<string> messages = {};
		pager.GetMessages(messages);
		if (messages.Count() <= index)
		{
			ctx.Fail("the pager holds no message " + index);
			return;
		}

		ctx.True(LF_RunPlate.IsEntry(messages[index]), "the pager entry is a plate answer");
		ctx.True(messages[index].Contains(plate), "the pager entry carries the plate");
	}
}
