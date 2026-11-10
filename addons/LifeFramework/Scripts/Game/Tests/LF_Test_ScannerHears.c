// red-proof: observed 2026-11-10 with `tools/cli test --no-build --only LF_Test_ScannerHears`: `Frequency(...) > 0` changed to `>= 0` in LF_Scanner.Hears failed it with `- a taxi call is heard by no one (expected 0, got 1)`. Restored; green.
//------------------------------------------------------------------------------------------------
//! comms/scanner-hears (#253): a scanner carrier who is not a receiver hears a police call and the
//! bus carries LF_OnScannerHeard; one with no scanner hears nothing; a call to a service with no
//! radio frequency (taxi) is not heard; the fence sells the scanner and stamps a serial.
// tier: WORLD
class LF_Test_ScannerHears : LF_Test
{
	static const ResourceName SCANNER_PREFAB = LF_Scanner.PREFAB;


	override string GetName()
	{
		return "comms/scanner-hears";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity scannerEntity = LF_Utils.SpawnEntityPrefab(SCANNER_PREFAB, vector.Zero);
		ctx.NotNull(scannerEntity, "the scanner prefab spawns");
		if (!scannerEntity)
			return;

		LF_ScannerComponent scanner = LF_ScannerComponent.Cast(scannerEntity.FindComponent(LF_ScannerComponent));
		ctx.NotNull(scanner, "the scanner prefab carries LF_ScannerComponent");
		ctx.NotNull(scannerEntity.FindComponent(LF_SerialComponent), "the scanner carries a serial, so the fence can stamp it");

		if (scanner)
		{
			array<int> receivers = {};
			LF_OnServiceCall police = LF_OnServiceCall.Create("caller-uid", "police", "Village payphone", "#LF-Payphone_Summary_Crime", "radio,board,pager", "96.5 0 263", receivers);
			LF_OnServiceCall taxi = LF_OnServiceCall.Create("caller-uid", "taxi", "Village payphone", "a fare", "pager,board", "96.5 0 263", receivers);
			ctx.True(LF_Scanner.Hears(police), "a police call is on the scanner");
			ctx.False(LF_Scanner.Hears(taxi), "a taxi call has no radio frequency, so the scanner does not hear it");

			array<ref array<LF_ScannerComponent>> carried = {};
			array<LF_ScannerComponent> mine = {};
			mine.Insert(scanner);
			array<LF_ScannerComponent> empty = {};
			carried.Insert(mine);
			carried.Insert(empty);
			array<string> uids = {};
			uids.Insert("scanner-listener");
			uids.Insert("no-scanner");
			array<IEntity> bodies = {};
			bodies.Insert(null);
			bodies.Insert(null);

			LF_ScannerProbe probe = new LF_ScannerProbe();
			LF_EventBus.Subscribe(LF_OnScannerHeard, probe);
			ctx.Equal(1, LF_Scanner.Hear(police, uids, carried, bodies), "the carrier hears the police call, the one without a scanner does not");
			ctx.Equal(1, probe.m_iHeard, "the bus carries one LF_OnScannerHeard");
			ctx.EqualStr("scanner-listener", probe.m_sUid, "the event names the listener");
			ctx.Equal(0, LF_Scanner.Hear(taxi, uids, carried, bodies), "a taxi call is heard by no one");
			ctx.Equal(1, probe.m_iHeard, "no event for the taxi call");
			LF_EventBus.Unsubscribe(LF_OnScannerHeard, probe);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(scannerEntity);
	}
}

//------------------------------------------------------------------------------------------------
class LF_ScannerProbe : LF_EventSubscriber
{
	int m_iHeard;
	string m_sUid;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnScannerHeard heard = LF_OnScannerHeard.Cast(busEvent);
		if (!heard)
			return false;

		m_iHeard++;
		m_sUid = heard.m_sListenerUid;
		return true;
	}
}
