// red-proof: 2026-11-24 `tools/cli.mjs test --no-build --red LF_Test_CasualtySpot,LF_Test_CasualtyDressed,LF_Test_CasualtyBed`: RED PROOF OK (all 3 listed test(s) fail with the hook on). And with the CloseDelivered line removed from LF_BedDelivery.Complete this test failed: "the delivery closes the call (expected 0, got 1)", "the board line is gone (expected 0, got 1)", "the close event (the map mark) is published once (expected 1, got 0)". Restored; green.
//------------------------------------------------------------------------------------------------
class LF_CasualtyBedSink : LF_EventSubscriber
{
	int m_iClosed;
	string m_sUid;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnServiceCallClosed closed = LF_OnServiceCallClosed.Cast(busEvent);
		if (closed && closed.m_sCallerUid.StartsWith(LF_CasualtyCalls.CALLER_PREFIX))
		{
			m_iClosed++;
			m_sUid = closed.m_sCallerUid;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! medic/casualty-bed (#4849): a casualty a medic puts in a clinic bed closes its call like a treat does:
//! the call leaves the open list, the medic board line goes and the close event that takes the dispatch map
//! mark down is published once for that casualty. A patient who is not a casualty changes nothing there.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CasualtyBed : LF_Test
{
	protected static const string KIND = "test_casualty_bed_site";
	protected static const string SUMMARY = "test casualty: bed me";
	protected static const int MEDIC_ID = 9484;

	//! The woken casualty: out of the open list after the delivery, so the test deletes it.
	protected IEntity m_Body;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "medic/casualty-bed";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity bed = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.HOSPITAL_BED_1);
		ctx.NotNull(bed, "DebugWorld has Annex_Hospital_Bed_1");
		if (!bed)
			return;

		LF_CasualtyCalls.ClearAll();
		LF_CasualtyCalls.ResetStatics();
		LF_CasualtyTreat.ResetStatics();
		LF_BedDelivery.ResetStatics();
		LF_CasualtyCallConfig config = new LF_CasualtyCallConfig();
		config.m_bEnabled = true;
		config.m_iIntervalSeconds = 1;
		config.m_iPerMedic = 1;
		config.m_iMaxOpen = 2;
		config.m_fSpotRadius = 6;
		config.m_sPrefab = LF_CasualtyCallConfig.DEFAULT_PREFAB;
		config.m_aRows = {};
		config.m_aRows.Insert(LF_CasualtyCallRow.Make(KIND, SUMMARY, 1));
		LF_CasualtyCallConfig.Override(config);
		LF_ServiceBoard.Install();
		LF_CasualtyBedSink sink = new LF_CasualtyBedSink();
		LF_EventBus.Subscribe(LF_OnServiceCallClosed, sink);

		IEntity medic = LF_TestCharacterFixture.Spawn(ctx);
		if (medic)
		{
			bool registered = LF_Places.Register(KIND, "bed-1", medic);
			ctx.True(registered, "the test place registers");
			if (registered)
				RunBed(ctx, sink, bed, medic);
			LF_Places.Unregister(KIND, "bed-1");
		}

		LF_EventBus.Unsubscribe(LF_OnServiceCallClosed, sink);
		if (m_Body)
			SCR_EntityHelper.DeleteEntityAndChildren(m_Body);
		LF_CasualtyCalls.ClearAll();
		LF_TestCharacterFixture.Delete(medic);
		LF_CasualtyTreat.ResetStatics();
		LF_BedDelivery.ResetStatics();
		LF_CasualtyCalls.ResetStatics();
		LF_CasualtyCallConfig.ResetStatics();
		LF_Downed.ResetStatics();
		LF_CasualtyCalls.Start();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBed(LF_TestContext ctx, LF_CasualtyBedSink sink, IEntity bed, IEntity medic)
	{
		int now = System.GetTickCount() + 2000;
		array<ref LF_ServiceCandidate> medics = {};
		medics.Insert(LF_ServiceCandidate.Make(MEDIC_ID, false, LF_EJobType.MEDIC));
		LF_Casualty casualty = LF_CasualtyCalls.Tick(now, medics);
		ctx.True(casualty != null, "the generator raises a casualty");
		if (!casualty)
			return;

		IEntity body = LF_CasualtyCalls.Body(casualty);
		ctx.True(body != null, "the casualty has a body");
		if (!body)
			return;

		m_Body = body;
		ctx.Equal(1, LF_CasualtyCalls.Open().Count(), "the call is open");
		ctx.Equal(1, BoardCount(now), "the call is on the medic board");
		ctx.Equal(0, sink.m_iClosed, "no close event yet");

		// The medic carried the casualty to the clinic: bedside, the delivery rule.
		medic.SetOrigin(bed.GetOrigin() + "1.2 0 0");
		body.SetOrigin(medic.GetOrigin() + "1 0 0");
		LF_EDeliveryRefusal delivered = LF_EDeliveryRefusal.NO_PATIENT;
		if (!RedProof())
			delivered = LF_BedDelivery.Deliver(medic, bed, body, true);

		ctx.Equal(LF_EDeliveryRefusal.NONE, delivered, "the casualty is put in the bed");
		ctx.False(LF_Downed.IsDowned(body), "the patient is revived in the bed");
		ctx.Equal(0, LF_CasualtyCalls.Open().Count(), "the delivery closes the call");
		ctx.Equal(0, BoardCount(now), "the board line is gone");
		ctx.Equal(1, sink.m_iClosed, "the close event (the map mark) is published once");
		ctx.EqualStr(casualty.m_sCallerUid, sink.m_sUid, "the close names the casualty's call");

		// The expiry finds nothing left to clear, so no second close.
		array<ref LF_ServiceCandidate> nobody = {};
		LF_CasualtyCalls.Tick(casualty.m_iExpiresMs + 1, nobody);
		ctx.Equal(1, sink.m_iClosed, "the expiry does not close the call again");
	}

	//------------------------------------------------------------------------------------------------
	//! Medic board entries with the test summary.
	protected int BoardCount(int nowMs)
	{
		array<string> entries = {};
		LF_ServiceBoard.BuildEntries("medic", nowMs, 0, entries);
		int count;
		foreach (string entry : entries)
		{
			if (entry.Contains(SUMMARY))
				count++;
		}

		return count;
	}
}
